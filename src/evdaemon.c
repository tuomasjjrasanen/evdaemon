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
#include <errno.h>
#include <stdlib.h>
#include <linux/uinput.h>

#include "config.h"
#include "util.h"
#include "settings.h"

static const int SELECT_TIMEOUT_SECONDS = 1;

extern char *program_invocation_name;
extern char *program_invocation_short_name;

static int            is_daemon        = 0;
static int            is_running       = 1;
static int            is_filtering     = 0;
static int            filter_fd        = -1;
static int            clone_fd         = -1;
static int            monitor_fd       = -1;
static struct timeval last_monitor_tv;
static struct settings settings;

void help_and_exit(void)
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
                        is_daemon = 1;
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

static void sigterm_handler(int signum)
{
        syslog(LOG_INFO, "stopping");
        is_running = 0;
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

static int handle_filter(void)
{
        struct timeval     now;
        struct input_event event;

        if (read(filter_fd, &event, sizeof(struct input_event)) == -1) {
                syslog(LOG_ERR, "filter read: %s", strerror(errno));
                return -1;
        }
        if (gettimeofday(&now, NULL) == -1) {
                syslog(LOG_ERR, "gettimeofday: %s", strerror(errno));
                return -1;
        }

        if (timestamp(&now) - timestamp(&last_monitor_tv)
            >= settings.filter_duration)
                is_filtering = 0;

        if (is_filtering) {
                if (event.type == EV_KEY
                    && bit_test64(event.code, settings.filter_key_valuev))
                        return 0;
                if (event.type == EV_REL
                    && bit_test64(event.code, settings.filter_rel_valuev))
                        return 0;
        }
        if (write(clone_fd, &event, sizeof(struct input_event))
            != sizeof(struct input_event))
                return -1;
        return 0;
}

static int handle_monitor(void)
{
        struct input_event event;

        if (read(monitor_fd, &event, sizeof(struct input_event)) == -1) {
                syslog(LOG_ERR, "monitor read: %s", strerror(errno));
                return -1;
        }

        if (event.type != EV_KEY) {
                return 0;
        }

        if (!bit_test64(event.code, settings.monitor_key_valuev))
                return 0;

        if (gettimeofday(&last_monitor_tv, NULL) == -1) {
                syslog(LOG_ERR, "gettimeofday: %s", strerror(errno));
                return -1;
        }
        is_filtering = 1;
        return 0;
}

int main(int argc, char **argv)
{
        struct timespec  timeout = {SELECT_TIMEOUT_SECONDS, 0};
        struct sigaction sigact;
        sigset_t         select_sigset;
        int              exitval = EXIT_FAILURE;
        int              syslog_options = LOG_ODELAY;
        int              settings_retval;

        parse_args(argc, argv);

        if (!is_daemon)
                syslog_options |= LOG_PERROR; /* Log also to stderr. */

        openlog(program_invocation_short_name, syslog_options, LOG_DAEMON);

        if (is_daemon && daemonize() == -1) {
                syslog(LOG_ERR, "daemonize: %s", strerror(errno));
                goto out;
        }

        syslog(LOG_INFO, "starting");

        memset(&sigact, 0, sizeof(struct sigaction));
        sigact.sa_handler = &sigterm_handler;

        if (sigfillset(&sigact.sa_mask) == -1) {
                syslog(LOG_ERR, "sigfillset: %s", strerror(errno));
                goto out;
        }

        if (sigaction(SIGTERM, &sigact, NULL) == -1) {
                syslog(LOG_ERR, "sigaction SIGTERM: %s", strerror(errno));
                goto out;
        }

        if (!is_daemon && sigaction(SIGINT, &sigact, NULL) == -1) {
                syslog(LOG_ERR, "sigaction SIGINT: %s", strerror(errno));
                goto out;
        }

        if (sigemptyset(&select_sigset) == -1) {
                syslog(LOG_ERR, "sigemptyset: %s", strerror(errno));
                goto out;
        }

        if (sigaddset(&select_sigset, SIGTERM) == -1) {
                syslog(LOG_ERR, "sigaddset SIGTERM: %s", strerror(errno));
                goto out;
        }

        if (!is_daemon && sigaddset(&select_sigset, SIGINT) == -1) {
                syslog(LOG_ERR, "sigaddset SIGINT: %s", strerror(errno));
                goto out;
        }
        
        settings_retval = settings_read(&settings);
        switch (settings_retval) {
        case 0:
                break;
        case -1:
                syslog(LOG_ERR, "settings_read: %s", strerror(errno));
                goto out;
        default:
                syslog(LOG_ERR, "settings_read: %s",
                       settings_strerror(settings_retval));
                goto out;
        }

        if ((monitor_fd = open_evdev_by_name(settings.monitor_name)) == -1) {
                syslog(LOG_ERR, "open monitor %s: %s", settings.monitor_name,
                       strerror(errno));
                goto out;
        }

        if ((filter_fd = open_evdev_by_name(settings.filter_name)) == -1) {
                syslog(LOG_ERR, "open filter %s: %s", settings.filter_name,
                       strerror(errno));
                goto out;
        }

        if (ioctl(filter_fd, EVIOCGRAB, 1) == -1) {
                syslog(LOG_ERR, "grab filter: %s", strerror(errno));
                goto out;
        }

        if ((clone_fd = clone_evdev(filter_fd, &settings.clone_id,
                                    settings.clone_name)) == -1) {
                syslog(LOG_ERR, "clone_evdev: %s", strerror(errno));
                goto out;
        }
	
        syslog(LOG_INFO, "started");

        while (is_running) {
                fd_set rfds;

                FD_ZERO(&rfds);
                FD_SET(monitor_fd, &rfds);
                FD_SET(filter_fd, &rfds);

                switch (pselect(filter_fd + 1, &rfds, NULL, NULL, &timeout,
                                &select_sigset)) {
                case 0:
                        break;
                case -1:
                        syslog(LOG_ERR, "select: %s", strerror(errno));
                        goto out;
                default:
                        if (FD_ISSET(filter_fd, &rfds)) {
                                if (handle_filter() == -1) {
                                        goto out;
                                }
                        } else if (FD_ISSET(monitor_fd, &rfds)) {
                                if (handle_monitor() == -1) {
                                        goto out;
                                }
                        }
                        break;
                }
        }
        syslog(LOG_INFO, "stopped");
        syslog(LOG_INFO, "terminating");

        exitval = EXIT_SUCCESS;
out:
        settings_free(&settings);

        if (clone_fd != -1) {
                if (ioctl(clone_fd, UI_DEV_DESTROY) == -1) {
                        syslog(LOG_ERR, "destroy clone: %s", strerror(errno));
                        exitval = EXIT_FAILURE;
                }
        
                if (close(clone_fd) == -1) {
                        syslog(LOG_ERR, "close clone: %s", strerror(errno));
                        exitval = EXIT_FAILURE;
                }
        }

        if (filter_fd != -1) {
                if (ioctl(filter_fd, EVIOCGRAB, 0) == -1) {
                        syslog(LOG_ERR, "release filter: %s", strerror(errno));
                        exitval = EXIT_FAILURE;
                }

                if (close(filter_fd) == -1) {
                        syslog(LOG_ERR, "close filter: %s", strerror(errno));
                        exitval = EXIT_FAILURE;
                }
        }

        if (monitor_fd != -1) {
                if (close(monitor_fd) == -1) {
                        syslog(LOG_ERR, "close monitor: %s", strerror(errno));
                        exitval = EXIT_FAILURE;
                }
        }

        syslog(LOG_INFO, "terminated");
        return exitval;
}
