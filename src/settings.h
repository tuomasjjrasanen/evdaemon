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
#ifndef SETTINGS_H
#define SETTINGS_H

#include <linux/uinput.h>
#include <stdint.h>

#define SETTINGS_ERROR_NO_ERROR          0
#define SETTINGS_ERROR_UNKNOWN           1
#define SETTINGS_ERROR_FILTER_DURATION   2
#define SETTINGS_ERROR_CLONE_ID_BUSTYPE  3
#define SETTINGS_ERROR_CLONE_ID_VENDOR   4
#define SETTINGS_ERROR_CLONE_ID_PRODUCT  5
#define SETTINGS_ERROR_CLONE_ID_VERSION  6
#define SETTINGS_ERROR_DIRTY_MONITOR_KEY 7
#define SETTINGS_ERROR_DIRTY_MONITOR_REL 8
#define SETTINGS_ERROR_DIRTY_FILTER_KEY  9
#define SETTINGS_ERROR_DIRTY_FILTER_REL  10

#define KEY_VALUEC (KEY_MAX / 64 + 1)
#define REL_VALUEC (REL_MAX / 64 + 1)

struct settings {
        char *monitor_name;
        size_t monitor_name_size;
        char *filter_name;
        size_t filter_name_size;
        double filter_duration;
        char clone_name[UINPUT_MAX_NAME_SIZE];
        struct input_id clone_id;
        uint64_t monitor_key_valuev[KEY_VALUEC];
        uint64_t monitor_rel_valuev[KEY_VALUEC];
        uint64_t filter_key_valuev[KEY_VALUEC];
        uint64_t filter_rel_valuev[REL_VALUEC];
};

const char *settings_strerror(int settings_error);

int settings_read(struct settings *settings);

void settings_free(struct settings *settings);

#endif /* SETTINGS_H */
