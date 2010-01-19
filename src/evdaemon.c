/*
  evdaemon - Blocks mouse button events during keyboard activity.
  Copyright © 2010 Tuomas Räsänen <tuos@codegrove.org>

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

extern char *program_invocation_name;
extern char *program_invocation_short_name;

static int kbd_fd;
static int mouse_fd;
static int running = 1;

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
	syslog(LOG_INFO, "terminating");
	running = 0;
}

static int clone_evdev(int evdev_fd)
{
	int original_errno = 0;
	int ui_fd;
	struct uinput_user_dev user_dev;
	int i;
	int evtype;
	struct input_id id;
	char devname[UINPUT_MAX_NAME_SIZE];
	uint8_t evdev_typebits[EV_MAX / 8 + 1];
	struct udev *udev;
	struct udev_device *udev_dev;

	if (ioctl(evdev_fd, EVIOCGNAME(sizeof(devname)), devname) == -1)
		return -1;

	if (ioctl(evdev_fd, EVIOCGID, &id) == -1)
		return -1;

	if ((udev = udev_new()) == NULL)
		return -1;
	udev_dev = udev_device_new_from_subsystem_sysname(udev, "misc",
							  "uinput");
	if (udev_dev == NULL) {
		udev_unref(udev);
		return -1;
	}
	ui_fd = open(udev_device_get_devnode(udev_dev), O_WRONLY);
	udev_device_unref(udev_dev);
	udev_unref(udev);

	if (ui_fd == -1)
		return -1;

	if (ioctl(evdev_fd, EVIOCGBIT(0, EV_MAX), evdev_typebits) < 0)
		goto err;

	for (evtype = 0; evtype < EV_MAX; ++evtype) {
		if (bit_test(evtype, evdev_typebits)) {
			int max_bits = 0;
			int io;
			if (ioctl(ui_fd, UI_SET_EVBIT, evtype) == -1) {
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
				uint8_t evbits[max_bits / 8 + 1];
				if (ioctl(evdev_fd, EVIOCGBIT(evtype, max_bits),
					  evbits) == -1) {
					goto err;
				}
				for (i = 0; i < max_bits; ++i) {
					if (bit_test(i, evbits)) {
						if (ioctl(ui_fd, io, i) == -1) {
							goto err;
						}
					}
				}
			}
		}
	}

	memset(&user_dev, 0, sizeof(user_dev));

	strncpy(user_dev.name, devname, sizeof(devname));
	user_dev.id.bustype = id.bustype;
	user_dev.id.vendor = id.vendor;
	user_dev.id.product = id.product;
	user_dev.id.version = id.version;

	if (write(ui_fd, &user_dev, sizeof(user_dev)) != sizeof(user_dev))
		goto err;

	if (ioctl(ui_fd, UI_DEV_CREATE) == -1)
		goto err;

	return ui_fd;
err:
	original_errno = errno;
	close(ui_fd);
	errno = original_errno;
	return -1;
}

static int daemonize(void)
{
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	openlog(program_invocation_short_name, LOG_ODELAY, LOG_USER);

	switch (fork()) {
	case 0:
		setsid();
		signal(SIGHUP, SIG_IGN);
		switch (fork()) {
		case 0:
			break;
		case -1:
			syslog(LOG_ERR, "%s: %s", "second fork",
			       strerror(errno));
			return -1;
		default:
			_exit(0);
		}
		break;
	case -1:
		syslog(LOG_ERR, "%s: %s", "first fork", strerror(errno));
		return -1;
	default:
		_exit(0);
	}
	
	chdir("/");
	umask(0);

	return 0;
}

static int handle_mouse(int mouse_fd, int ui_fd, int *filter,
			struct timeval *last_kbd, double idle)
{
	struct timeval now;
	struct input_event event;

	if (read(mouse_fd, &event, sizeof(event)) == -1) {
		syslog(LOG_ERR, "%s: %s", "mouse read", strerror(errno));
		return -1;
	}
	if (gettimeofday(&now, NULL) == -1) {
		syslog(LOG_ERR, "%s: %s", "gettimeofday", strerror(errno));
		return -1;
	}

	if (timestamp(&now) - timestamp(last_kbd) >= idle)
		*filter = 0;

	if (*filter) {
		/*
		  Filter only mouse buttons, motion could be filtered
		  by addin an identical rule to EV_REL.
		*/
		if (event.type == EV_KEY)
			return 0;
	}
	if (write(ui_fd, &event, sizeof(event)) != sizeof(event))
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
	  Allow modifier keys to be used simultaenously with mouse.
	*/
	if (event.code == KEY_LEFTCTRL || event.code == KEY_RIGHTCTRL
	    || event.code == KEY_LEFTSHIFT
	    || event.code == KEY_RIGHTSHIFT
	    || event.code == KEY_LEFTALT
	    || event.code == KEY_RIGHTALT) {
		return 0;
	}

	if (gettimeofday(last_kbd, NULL) == -1) {
		syslog(LOG_ERR, "%s: %s", "gettimeofday", strerror(errno));
		return -1;
	}
	*filter = 1;
	return 0;
}

void more_help()
{
	fprintf(stderr, "Try `%s --help' for more information.\n",
		program_invocation_name);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int filter = 0;
	struct timeval last_kbd;
	int ui_fd;
	fd_set rfds;
	struct sigaction sa;
	sigset_t set;
	double idle = 0.75;

	const struct option options[] = {
		{"idle-time", required_argument, NULL, 'i'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{}
	};

	while (1) {
		int option;

		option = getopt_long(argc, argv, "i:hV", options, NULL);

		if (option == -1)
			break;

		switch (option) {
		case 'i':
			idle = strtod(optarg, NULL);
			if (idle <= 0) {
				fprintf(stderr, "%s: incorrect idle time\n",
					program_invocation_name);
				more_help();
			}
			break;
		case 'V':
			printf("evdaemon 0.1\n"
			       "Copyright © 2010 Tuomas Räsänen.\n"
			       "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
			       "This is free software: you are free to change and redistribute it.\n"
			       "There is NO WARRANTY, to the extent permitted by law.\n"
				);
			return EXIT_SUCCESS;
		case 'h':
			printf("Usage: %s [OPTIONS] EVDEV1 EVDEV2\n"
			       "Filters out events of an event device during event activity in another event device.\n"
			       "\n"
			       " -i SECONDS, --idle-time=SECONDS  wait SECONDS after last key or button event\n"
			       "                                  before turning off the filter. Default=%.2lf\n"
			       " -h --help                        display this help and exit\n"
			       " -V --version                     output version infromation and exit\n"
			       "\n"
			       " EVDEV1\n"
			       "   Event device to be listened for activity, e.g. /dev/input/event3 .\n"
			       "\n"
			       " EVDEV2\n"
			       "   Event device to be filtered, e.g. /dev/input/event4 .\n"
			       "\n"
			       "Event device numbers for EVDEV1 and EVDEV2 are listed in HANDLERS-properties\n"
			       "in /proc/bus/input/devices .\n",
			       program_invocation_name, idle);
			return EXIT_SUCCESS;
		case '?':
			more_help();
		default:
			errx(EXIT_FAILURE, "Argument parsing failed.");
		}		
	}

	if (optind + 2 != argc) {
		fprintf(stderr, "%s: wrong number of arguments\n",
			program_invocation_name);
		more_help();
	}

	if ((kbd_fd = open(argv[optind], O_RDONLY)) == -1) {
		error(0, errno, "opening %s", argv[optind]);
		goto err;
	}

	if ((mouse_fd = open(argv[optind + 1], O_RDONLY)) == -1) {
		error(0, errno, "opening %s", argv[optind + 1]);
		goto err;
	}

	if (ioctl(mouse_fd, EVIOCGRAB, 1) == -1) {
		perror("mouse grab");
		goto err;
	}

	ui_fd = clone_evdev(mouse_fd);

	if (daemonize() == -1)
		goto err;

	sigfillset(&sa.sa_mask);
	sa.sa_handler = &sigterm_handler;
	if (sigaction(SIGTERM, &sa, NULL) == -1) {
		perror("sigaction SIGTERM");
		goto err;
	}

	sigemptyset(&set);
	sigaddset(&set, SIGTERM);

	while (running) {
		FD_ZERO(&rfds);
		FD_SET(kbd_fd, &rfds);
		FD_SET(mouse_fd, &rfds);

		switch (pselect(mouse_fd + 1, &rfds, NULL, NULL, NULL, &set)) {
		case -1:
			syslog(LOG_ERR, "%s: %s", "select", strerror(errno));
			goto err;
		default:
			if (FD_ISSET(mouse_fd, &rfds)) {
				if (handle_mouse(mouse_fd, ui_fd, &filter,
						 &last_kbd, idle) == -1) {
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
	if (close(mouse_fd) == -1) {
		syslog(LOG_ERR, "%s: %s", "mouse close", strerror(errno));
		goto err;
	}
	if (close(kbd_fd) == -1) {
		syslog(LOG_ERR, "%s: %s", "kbd close", strerror(errno));
		goto err;
	}
	return EXIT_SUCCESS;
err:
	close(mouse_fd);
	close(kbd_fd);
	return EXIT_FAILURE;
}
