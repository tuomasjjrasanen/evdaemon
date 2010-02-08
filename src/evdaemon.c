/*
  evdaemon - Monitor Linux event devices and modify their behavior.
  Copyright © 2010 Tuomas Räsänen (tuos) <tuos@codegrove.org>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <getopt.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <linux/uinput.h>
#include <stdint.h>
#include <libudev.h>
#include <limits.h>

#include "config.h"

#define IS_MODIFIER_KEY(keycode) ((keycode) == KEY_LEFTCTRL   ||    \
                                  (keycode) == KEY_RIGHTCTRL  ||    \
                                  (keycode) == KEY_LEFTSHIFT  ||    \
                                  (keycode) == KEY_RIGHTSHIFT ||    \
                                  (keycode) == KEY_LEFTALT    ||    \
                                  (keycode) == KEY_RIGHTALT)


static const int SELECT_TIMEOUT_SECONDS = 1;
static const int EXIT_UNDEFINED = INT_MAX;

extern char *program_invocation_name;
extern char *program_invocation_short_name;

static int running = 1;

/* Args and default values: */
static int    arg_daemon = 0;
static double arg_idle_time = 0.75;
static int    arg_monitor_modifiers = 0;

void help_and_exit()
{
        fprintf(stderr, "Try `%s --help' for more information.\n",
                program_invocation_name);
        exit(EXIT_FAILURE);
}

void parse_args(int argc, char **argv)
{
        const struct option options[] = {
                {"daemon", no_argument, NULL, 'd'},
                {"version", no_argument, NULL, 'V'},
                {"help", no_argument, NULL, 'h'},
                {0, 0, 0, 0}
        };

        while (1) {
                int option;

                option = getopt_long(argc, argv, "Vh", options, NULL);

                if (option == -1)
                        break;

                switch (option) {
                case 'd':
                        arg_daemon = 1;
                        break;
                case 'V':
                        printf("%s %s\n"
                               "Copyright © 2010 %s\n"
                               "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
                               "This is free software: you are free to change and redistribute it.\n"
                               "There is NO WARRANTY, to the extent permitted by law.\n",
                               PACKAGE_NAME, VERSION, PACKAGE_AUTHOR);
                        exit(EXIT_SUCCESS);
                case 'h':
                        printf("Usage: %s [OPTION]...\n"
                               "%s\n"
                               "\n"
                               "Options:\n"
                               "     --daemon               run as a daemon process\n"
                               " -h, --help                 display this help and exit\n"
                               " -V, --version              output version infromation and exit\n"
                               "\n"
                               "Processing information and error messages are always printed into syslog. If\n"
                               "%s is not running as a daemon, logs are also printed into stderr.\n"
                               "\n"
                               "Report %s bugs to <%s>\n"
                               "Home page: <%s>\n",
                               program_invocation_name, PACKAGE_DESCRIPTION,
                               PACKAGE_NAME, PACKAGE_NAME,
                               PACKAGE_BUGREPORT,
                               PACKAGE_URL);
                        exit(EXIT_SUCCESS);
                case '?':
                        help_and_exit();
                default:
                        errx(EXIT_FAILURE, "argument parsing failed");
                }		
        }

        if (optind != argc) {
                fprintf(stderr, "%s: wrong number of arguments\n",
                        program_invocation_name);
                help_and_exit();
        }
}

static int bit_test(int bit_i, const uint8_t *bytes)
{
        return (bytes[bit_i / 8] & (1 << (bit_i % 8))) && 1;
}

static double timestamp(const struct timeval *tv)
{
        return tv->tv_sec + (tv->tv_usec / 1000000.0);
}

static void sigterm_handler(int signum)
{
        syslog(LOG_INFO, "stopping");
        running = 0;
}

static const char *get_uinput_devnode()
{
        static char        uinput_devnode[_POSIX_PATH_MAX + 1];
        const char         *devnode;
        struct udev        *udev;
        struct udev_device *udev_dev;

        if ((udev = udev_new()) == NULL)
                return NULL;

        udev_dev = udev_device_new_from_subsystem_sysname(udev, "misc",
                                                          "uinput");
	
        if ((devnode = udev_device_get_devnode(udev_dev)) == NULL)
                return NULL;

        /* I'm on very defensive mood.. it's due the ignorance. :P */
        if (strlen(devnode) >= sizeof(uinput_devnode)) {
                errno = ENAMETOOLONG;
                return NULL;
        }

        strncpy(uinput_devnode, devnode, _POSIX_PATH_MAX);

        if (udev_dev == NULL) {
                udev_unref(udev);
                return NULL;
        }

        udev_device_unref(udev_dev);
        udev_unref(udev);

        return uinput_devnode;
}

static int clone_evdev(int evdev_fd)
{
        struct uinput_user_dev user_dev;
        struct input_id        id;
        int                    original_errno = 0;
        int                    uinput_fd = -1;
        int                    evtype;
        char                   devname[sizeof(user_dev.name)];
        uint8_t                evdev_typebits[EV_MAX / 8 + 1];

        if (ioctl(evdev_fd, EVIOCGNAME(sizeof(devname)), devname) == -1)
                goto err;

        if (ioctl(evdev_fd, EVIOCGID, &id) == -1)
                goto err;

        uinput_fd = open(get_uinput_devnode(), O_WRONLY);

        if (uinput_fd == -1)
                goto err;

        if (ioctl(evdev_fd, EVIOCGBIT(0, EV_MAX), evdev_typebits) < 0)
                goto err;

        for (evtype = 0; evtype < EV_MAX; ++evtype) {
                if (bit_test(evtype, evdev_typebits)) {
                        int max_bits = 0;
                        int io;
                        if (ioctl(uinput_fd, UI_SET_EVBIT, evtype) == -1) {
                                goto err;
                        }
                        switch (evtype) {
                        case EV_REL:
                                max_bits = REL_MAX;
                                io = UI_SET_RELBIT;
                                break;
                        case EV_MSC:
                                max_bits = MSC_MAX;
                                io = UI_SET_MSCBIT;
                                break;
                        case EV_KEY:
                                max_bits = KEY_MAX;
                                io = UI_SET_KEYBIT;
                                break;
                        case EV_ABS:
                                max_bits = ABS_MAX;
                                io = UI_SET_ABSBIT;
                                break;
                        case EV_SW:
                                max_bits = SW_MAX;
                                io = UI_SET_SWBIT;
                                break;
                        case EV_LED:
                                max_bits = LED_MAX;
                                io = UI_SET_LEDBIT;
                                break;
                        case EV_SND:
                                max_bits = SND_MAX;
                                io = UI_SET_SNDBIT;
                                break;
                        case EV_FF:
                                max_bits = FF_MAX;
                                io = UI_SET_FFBIT;
                                break;				
                        default:
                                break;
                        }
                        if (max_bits) {
                                int i;
                                uint8_t evbits[max_bits / 8 + 1];
                                if (ioctl(evdev_fd, EVIOCGBIT(evtype, max_bits),
                                          evbits) == -1) {
                                        goto err;
                                }
                                for (i = 0; i < max_bits; ++i) {
                                        if (bit_test(i, evbits)
                                            && ioctl(uinput_fd, io, i) == -1) {
                                                goto err;
                                        }
                                }
                        }
                }
        }

        memset(&user_dev, 0, sizeof(user_dev));
        strcpy(user_dev.name, devname);
        user_dev.id.bustype = id.bustype;
        user_dev.id.vendor = id.vendor;
        user_dev.id.product = id.product;
        user_dev.id.version = id.version;

        if (write(uinput_fd, &user_dev, sizeof(user_dev)) != sizeof(user_dev))
                goto err;

        if (ioctl(uinput_fd, UI_DEV_CREATE) == -1)
                goto err;

        return uinput_fd;
err:
        original_errno = errno;
        close(uinput_fd);
        errno = original_errno;
        return -1;
}

static int daemonize(void)
{
        int i;
        int fd;
        int daemon_errno = 0;

        signal(SIGINT, SIG_IGN);

        switch (fork()) {
        case 0:
                setsid();
                signal(SIGHUP, SIG_IGN);
                switch (fork()) {
                case 0:
                        break;
                case -1:
                        return -1;
                default:
                        exit(0);
                }
                break;
        case -1:
                return -1;
        default:
                exit(0);
        }
	
        umask(0);

        if (chdir("/") == -1)
                daemon_errno = errno;

        for (i = 0; i < 3; ++i)
                if (close(i) == -1 && !daemon_errno)
                        daemon_errno = errno;
        
        fd = open("/dev/null", O_RDWR);
        if (fd == -1 && !daemon_errno) {
                daemon_errno = errno;
        } else {
                if (dup(fd) == -1 && !daemon_errno)
                        daemon_errno = errno;
                if (dup(fd) == -1 && !daemon_errno)
                        daemon_errno = errno;
        }
     
        return daemon_errno ? -1 : 0;
}

static int handle_mouse(int mouse_fd, int uinput_fd, int *filter,
                        struct timeval *last_kbd)
{
        struct timeval     now;
        struct input_event event;

        if (read(mouse_fd, &event, sizeof(event)) == -1) {
                syslog(LOG_ERR, "%s: %s", "mouse read", strerror(errno));
                return -1;
        }
        if (gettimeofday(&now, NULL) == -1) {
                syslog(LOG_ERR, "%s: %s", "gettimeofday", strerror(errno));
                return -1;
        }

        if (timestamp(&now) - timestamp(last_kbd) >= arg_idle_time)
                *filter = 0;

        if (*filter) {
                /*
                  Filter only mouse buttons, motion could be filtered
                  by adding an identical rule to EV_REL.
                */
                if (event.type == EV_KEY)
                        return 0;
        }
        if (write(uinput_fd, &event, sizeof(event)) != sizeof(event))
                return -1;
        return 0;
}

static int handle_kbd(int mouse_fd, int kbd_fd, int *filter,
                      struct timeval *last_kbd)
{
        struct input_event event;

        if (read(kbd_fd, &event, sizeof(event)) == -1) {
                syslog(LOG_ERR, "%s: %s", "kbd read", strerror(errno));
                return -1;
        }

        /*
          Only key-events can set the filtering on.
          We are not interested in EV_MSC for example.
        */
        if (event.type != EV_KEY) {
                return 0;
        }

        /*
          If we are not monitoring modifier keys, we don't care if they
          are pressed or not.
        */
        if (!arg_monitor_modifiers && IS_MODIFIER_KEY(event.code))
                return 0;

        if (gettimeofday(last_kbd, NULL) == -1) {
                syslog(LOG_ERR, "%s: %s", "gettimeofday", strerror(errno));
                return -1;
        }
        *filter = 1;
        return 0;
}

int main(int argc, char **argv)
{
        int              filter = 0;
        struct timespec  timeout = {SELECT_TIMEOUT_SECONDS, 0};
        struct timeval   last_kbd;
        struct sigaction sigact;
        sigset_t         select_sigset;
        int              uinput_fd = -1;
        int              kbd_fd = -1;
        int              mouse_fd = -1;
        int              exitval = EXIT_UNDEFINED;
        int              syslog_options = LOG_ODELAY;

        parse_args(argc, argv);

        if (!arg_daemon)
                syslog_options |= LOG_PERROR; /* Log also to stderr. */

        openlog(program_invocation_short_name, syslog_options, LOG_DAEMON);

        if (arg_daemon && daemonize() == -1) {
                syslog(LOG_ERR, "daemonize: %s", strerror(errno));
                goto err;
        }

        syslog(LOG_INFO, "starting");

        sigact.sa_handler = &sigterm_handler;

        if (sigfillset(&sigact.sa_mask) == -1) {
                syslog(LOG_ERR, "sigfillset: %s", strerror(errno));
                goto err;
        }

        if (sigaction(SIGTERM, &sigact, NULL) == -1) {
                syslog(LOG_ERR, "sigaction SIGTERM: %s", strerror(errno));
                goto err;
        }

        if (!arg_daemon && sigaction(SIGINT, &sigact, NULL) == -1) {
                syslog(LOG_ERR, "sigaction SIGINT: %s", strerror(errno));
                goto err;
        }

        if (sigemptyset(&select_sigset) == -1) {
                syslog(LOG_ERR, "sigemptyset: %s", strerror(errno));
                goto err;
        }

        if (sigaddset(&select_sigset, SIGTERM) == -1) {
                syslog(LOG_ERR, "sigaddset SIGTERM: %s", strerror(errno));
                goto err;
        }

        if (!arg_daemon && sigaddset(&select_sigset, SIGINT) == -1) {
                syslog(LOG_ERR, "sigaddset SIGINT: %s", strerror(errno));
                goto err;
        }

        if ((kbd_fd = open(argv[optind], O_RDONLY)) == -1) {
                syslog(LOG_ERR, "open %s: %s", argv[optind], strerror(errno));
                goto err;
        }

        if ((mouse_fd = open(argv[optind + 1], O_RDONLY)) == -1) {
                syslog(LOG_ERR, "open %s: %s", argv[optind+1], strerror(errno));
                goto err;
        }

        if (ioctl(mouse_fd, EVIOCGRAB, 1) == -1) {
                syslog(LOG_ERR, "grab mouse: %s", strerror(errno));
                goto err;
        }

        if ((uinput_fd = clone_evdev(mouse_fd)) == -1) {
                syslog(LOG_ERR, "clone_evdev: %s", strerror(errno));
                goto err;
        }
	
        syslog(LOG_INFO, "started");

        while (running) {
                fd_set rfds;

                FD_ZERO(&rfds);
                FD_SET(kbd_fd, &rfds);
                FD_SET(mouse_fd, &rfds);

                switch (pselect(mouse_fd + 1, &rfds, NULL, NULL, &timeout,
                                &select_sigset)) {
                case 0:
                        break;
                case -1:
                        syslog(LOG_ERR, "select: %s", strerror(errno));
                        goto err;
                default:
                        if (FD_ISSET(mouse_fd, &rfds)) {
                                if (handle_mouse(mouse_fd, uinput_fd, &filter,
                                                 &last_kbd) == -1) {
                                        goto err;
                                }
                        } else if (FD_ISSET(kbd_fd, &rfds)) {
                                if (handle_kbd(mouse_fd, kbd_fd, &filter,
                                               &last_kbd) == -1) {
                                        goto err;
                                }
                        }
                        break;
                }
        }
        syslog(LOG_INFO, "stopped");
        syslog(LOG_INFO, "terminating");

        exitval = EXIT_SUCCESS;
err:
        exitval = exitval == EXIT_UNDEFINED ? EXIT_FAILURE : exitval;

        if (ioctl(uinput_fd, UI_DEV_DESTROY) == -1) {
                syslog(LOG_ERR, "destroy uinput: %s", strerror(errno));
                exitval = EXIT_FAILURE;
        }

        if (close(uinput_fd) == -1) {
                syslog(LOG_ERR, "close uinput: %s", strerror(errno));
                exitval = EXIT_FAILURE;
        }

        if (ioctl(mouse_fd, EVIOCGRAB, 0) == -1) {
                syslog(LOG_ERR, "release mouse: %s", strerror(errno));
                exitval = EXIT_FAILURE;
        }

        if (close(mouse_fd) == -1) {
                syslog(LOG_ERR, "close mouse: %s", strerror(errno));
                exitval = EXIT_FAILURE;
        }

        if (close(kbd_fd) == -1) {
                syslog(LOG_ERR, "close kbd: %s", strerror(errno));
                exitval = EXIT_FAILURE;
        }

        syslog(LOG_INFO, "terminated");
        return exitval;
}
