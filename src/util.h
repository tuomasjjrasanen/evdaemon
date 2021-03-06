/*
  Copyright © 2011 Tuomas Jorma Juhani Räsänen

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
#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <sys/time.h>

int strtovaluev(uint64_t *valuev, size_t len, const char *line);

int open_evdev_by_name(const char *name);

int readln(char **buf, size_t *n, const char *path);

int bit_test64(int bit_i, const uint64_t *bitarray);

int bit_test8(int bit_i, const uint8_t *bytes);

double timestamp(const struct timeval *tv);

const char *get_uinput_devnode();

int clone_evdev(int evdev_fd, const struct input_id *clone_id,
                const char *clone_name);

const char *get_devroot_path();

#endif /* UTIL_H */
