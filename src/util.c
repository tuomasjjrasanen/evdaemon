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
#include <glob.h>
#include <stdlib.h>

#include "util.h"

/*
  Returns:
   0: success
  -1: sycall failed
  -2: conversion error
  -3: line has more than len values
 */
int strtovaluev(uint64_t *valuev, size_t len, const char *line)
{
        const char *nptr = line;
        char *endptr;
        size_t valuec = 0;
        int i;

        do {
                uint64_t value;
                errno = 0;
                value = strtoul(nptr, &endptr, 16);
                if (value == 0 && endptr == nptr)
                        return -2;
                if (errno)
                        return -1;
                ++valuec;
                nptr = endptr;
        } while (*endptr != '\0');

        if (valuec > len)
                return -3;

        nptr = line;
        for (i = valuec - 1; i >= 0; --i) {
                valuev[i] = strtoul(nptr, &endptr, 16);
                nptr = endptr;
        }
        return 0;
}

static int open_matching(const char *path, const char *name)
{
        int cmp_result = -2;
        int fd = -1;
        char *other_name;
        int orig_errno;
        size_t name_size = strlen(name) + 1;
        other_name = (char *) calloc(name_size, sizeof(char));

        if (other_name == NULL)
                return -1;

        if ((fd = open(path, O_RDONLY)) == -1)
                goto out;

        if (ioctl(fd, EVIOCGNAME(name_size - 1), other_name) == -1)
                goto out;

        if ((cmp_result = strcmp(name, other_name)) != 0)
                errno = ENOENT;
out:
        orig_errno = errno;
        if (fd != -1 && cmp_result != 0) {
                close(fd);
                fd = -1;
        }
        free(other_name);
        errno = orig_errno;
        return fd;
}

int open_evdev_by_name(const char *name)
{
        int i;
        glob_t g;
        const char *devroot_path;
        int fd = -1;
        char *pattern;
        const char pattern_tail[] = "/input/event*";
        int orig_errno;

        if ((devroot_path = get_devroot_path()) == NULL)
                return -1;

        pattern = (char *) calloc(strlen(devroot_path) + sizeof(pattern_tail),
                                  sizeof(char));
        if (pattern == NULL)
                goto out;

        strcat(strcpy(pattern, devroot_path), pattern_tail);
        switch (glob(pattern, GLOB_ERR | GLOB_NOSORT, NULL, &g)) {
        case 0:
                break;
        case GLOB_NOMATCH:
                errno = ENOENT;
                /* Fall trough. */
        default:
                goto out;
        }

        for (i = 0; i < g.gl_pathc; ++i) {
                if ((fd = open_matching(g.gl_pathv[i], name)) != -1)
                        break;
        }
out:
        orig_errno = errno;
        free(pattern);
        pattern = NULL;
        globfree(&g);
        errno = orig_errno;
        return fd;
}

/* Returns

   0 : Everything went just ok and buf is malloced or realloced, the caller is
   responsible for freeing the memory. Buf does not contain newline
   character.

   -1 : Syscall failed and errno is set: not changes were made.
*/
int readln(char **buf, size_t *n, const char *path)
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

int bit_test(int bit_i, const uint64_t *bytes)
{
        return (bytes[bit_i / 64] & (1 << (bit_i % 64))) && 1;
}

double timestamp(const struct timeval *tv)
{
        return tv->tv_sec + (tv->tv_usec / 1000000.0);
}

const char *get_devroot_path()
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

int clone_evdev(int evdev_fd, const struct input_id *clone_id,
                const char *clone_name)
{
        struct uinput_user_dev user_dev;
        struct input_id id;
        int orig_errno = 0;
        int clone_fd;
        int evtype;
        uint64_t evdev_typebits[EV_MAX / 64 + 1];
        const char *uinput_devnode;
        int got_err = 1;

        memset(&evdev_typebits, 0, sizeof(evdev_typebits));

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
                        int max_bit = 0;
                        int io;
                        if (ioctl(clone_fd, UI_SET_EVBIT, evtype) == -1) {
                                goto out;
                        }
                        switch (evtype) {
                        case EV_REL:
                                max_bit = REL_MAX;
                                io = UI_SET_RELBIT;
                                break;
                        case EV_MSC:
                                max_bit = MSC_MAX;
                                io = UI_SET_MSCBIT;
                                break;
                        case EV_KEY:
                                max_bit = KEY_MAX;
                                io = UI_SET_KEYBIT;
                                break;
                        case EV_ABS:
                                max_bit = ABS_MAX;
                                io = UI_SET_ABSBIT;
                                break;
                        case EV_SW:
                                max_bit = SW_MAX;
                                io = UI_SET_SWBIT;
                                break;
                        case EV_LED:
                                max_bit = LED_MAX;
                                io = UI_SET_LEDBIT;
                                break;
                        case EV_SND:
                                max_bit = SND_MAX;
                                io = UI_SET_SNDBIT;
                                break;
                        case EV_FF:
                                max_bit = FF_MAX;
                                io = UI_SET_FFBIT;
                                break;				
                        default:
                                break;
                        }
                        if (max_bit) {
                                int i;
                                uint64_t evbits[max_bit / 64 + 1];
                                memset(&evbits, 0, sizeof(evbits));
                                if (ioctl(evdev_fd, EVIOCGBIT(evtype, max_bit),
                                          evbits) == -1) {
                                        goto out;
                                }
                                for (i = 0; i < max_bit; ++i) {
                                        if (bit_test(i, evbits)
                                            && ioctl(clone_fd, io, i) == -1) {
                                                goto out;
                                        }
                                }
                        }
                }
        }

        memset(&user_dev, 0, sizeof(struct uinput_user_dev));
        strncpy(user_dev.name, clone_name, UINPUT_MAX_NAME_SIZE - 1);
        user_dev.id.bustype = clone_id->bustype;
        user_dev.id.vendor = clone_id->vendor;
        user_dev.id.product = clone_id->product;
        user_dev.id.version = clone_id->version;

        if (write(clone_fd, &user_dev, sizeof(struct uinput_user_dev))
            != sizeof(struct uinput_user_dev))
                goto out;

        if (ioctl(clone_fd, UI_DEV_CREATE) == -1)
                goto out;

        got_err = 0;
out:
        orig_errno = errno;
        if (got_err && clone_fd != -1) {
                close(clone_fd);
                clone_fd = -1;
        }
        errno = orig_errno;
        return clone_fd;
}
