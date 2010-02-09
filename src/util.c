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
#include <linux/uinput.h>
#include <libudev.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include "util.h"

/* Returns

    0 : Everything went just ok and buf is malloced or realloced, the caller is
        responsible for freeing the memory. Buf does not contain newline
        character.

   -1 : Syscall failed and errno is set: not changes were made.
*/
int freadln(char **buf, size_t *n, const char *path)
{
        FILE *file;
        ssize_t chars;
        int orig_errno;
        int retval = -1;
        
        if ((file = fopen(path, "r")) == NULL)
                return -1;

        if ((chars = getline(buf, n, file)) == -1)
                goto out;

        /* Remove trailing newline character if it exists. */
        if ((*buf)[chars-1] == '\n')
                (*buf)[chars-1] = '\0';

        retval = 0;
out:
        orig_errno = errno;
        fclose(file);
        errno = orig_errno;
        return retval;
}

int bit_test(int bit_i, const uint8_t *bytes)
{
        return (bytes[bit_i / 8] & (1 << (bit_i % 8))) && 1;
}

double timestamp(const struct timeval *tv)
{
        return tv->tv_sec + (tv->tv_usec / 1000000.0);
}

const char *get_dev_path()
{
        static char dev_path[_POSIX_PATH_MAX + 1];
        struct udev *udev; 
        const char *path;
        const char *retval = NULL;
        int orig_errno;

        if ((udev = udev_new()) == NULL)
                return NULL;
        
        if ((path = udev_get_dev_path(udev)) == NULL)
                goto out;

        if (strlen(path) > _POSIX_PATH_MAX) {
                errno = ENAMETOOLONG;
                goto out;
        }        

        strncpy(dev_path, path, _POSIX_PATH_MAX);
        retval = dev_path;
out:
        orig_errno = errno;
        udev_unref(udev);
        errno = orig_errno;
        return retval;
}

const char *get_uinput_devnode()
{
        static char uinput_devnode[_POSIX_PATH_MAX + 1];
        struct udev *udev;
        struct udev_device *udev_dev;
        const char *devnode;
        const char *retval = NULL;
        int orig_errno;

        if ((udev = udev_new()) == NULL)
                return NULL;

        udev_dev = udev_device_new_from_subsystem_sysname(udev, "misc",
                                                          "uinput");
        if (udev_dev == NULL)
                goto out;

        if ((devnode = udev_device_get_devnode(udev_dev)) == NULL)
                goto out;

        /* I'm on very defensive mood.. it's due the ignorance. :P */
        if (strlen(devnode) > _POSIX_PATH_MAX) {
                errno = ENAMETOOLONG;
                goto out;
        }

        strncpy(uinput_devnode, devnode, _POSIX_PATH_MAX);
        retval = uinput_devnode;
out:
        orig_errno = errno;
        udev_device_unref(udev_dev);
        udev_unref(udev);
        errno = orig_errno;
        return retval;
}

int clone_evdev(int evdev_fd)
{
        struct uinput_user_dev user_dev;
        struct input_id id;
        int orig_errno = 0;
        int clone_fd;
        int evtype;
        char devname[sizeof(user_dev.name)];
        uint8_t evdev_typebits[EV_MAX / 8 + 1];
        const char *uinput_devnode;
        int retval = -1;

        if (ioctl(evdev_fd, EVIOCGNAME(sizeof(devname)), devname) == -1)
                return -1;

        if (ioctl(evdev_fd, EVIOCGID, &id) == -1)
                return -1;

        if (ioctl(evdev_fd, EVIOCGBIT(0, EV_MAX), evdev_typebits) < 0)
                return -1;

        if ((uinput_devnode = get_uinput_devnode()) == NULL)
                return -1;

        if ((clone_fd = open(uinput_devnode, O_WRONLY)) == -1)
                return -1;

        /* From now on, resources has to released:
           goto out instead of plain return.*/

        for (evtype = 0; evtype < EV_MAX; ++evtype) {
                if (bit_test(evtype, evdev_typebits)) {
                        int max_bits = 0;
                        int io;
                        if (ioctl(clone_fd, UI_SET_EVBIT, evtype) == -1) {
                                goto out;
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
                                        goto out;
                                }
                                for (i = 0; i < max_bits; ++i) {
                                        if (bit_test(i, evbits)
                                            && ioctl(clone_fd, io, i) == -1) {
                                                goto out;
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

        if (write(clone_fd, &user_dev, sizeof(user_dev)) != sizeof(user_dev))
                goto out;

        if (ioctl(clone_fd, UI_DEV_CREATE) == -1)
                goto out;

        retval = 0;
out:
        orig_errno = errno;
        close(clone_fd);
        errno = orig_errno;
        return retval;
}
