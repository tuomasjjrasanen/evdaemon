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
#ifndef SETTINGS_H
#define SETTINGS_H

#include <linux/uinput.h>

#define SETTINGS_ERROR_NO_ERROR         0
#define SETTINGS_ERROR_FILTER_DURATION  1
#define SETTINGS_ERROR_CLONE_ID_BUSTYPE 2
#define SETTINGS_ERROR_CLONE_ID_VENDOR  3
#define SETTINGS_ERROR_CLONE_ID_PRODUCT 4
#define SETTINGS_ERROR_CLONE_ID_VERSION 5

struct settings {
        char *monitor_name;
        size_t monitor_name_size;
        char *filter_name;
        size_t filter_name_size;
        double filter_duration;
        char clone_name[UINPUT_MAX_NAME_SIZE];
        struct input_id clone_id;
};

const char *settings_strerror(int settings_error);

int settings_read(struct settings *settings);

void settings_free(struct settings *settings);

#endif /* SETTINGS_H */
