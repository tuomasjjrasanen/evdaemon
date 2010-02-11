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
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "settings.h"
#include "util.h"

#define SETTINGS_ERROR_COUNT 2

static const char *SETTINGS_ERROR_STRS[] = {
        "",
        "dirty or empty filter duration file",
};

#define SETTINGS_ERROR_UNKNOWN "unknown settings error"

/* Returns

   0 : Everything went just ok and settings were updated.

   -1 : Syscall failed and errno is set: no changes were made.

   >0 : One of the SETTINGS_ERROR_-prefixed values defined in settings.h.
*/
static int read_filter_duration(struct settings *settings)
{
        char *filter_duration_line = NULL;
        size_t filter_duration_line_size;
        char *strtod_endptr = NULL;
        double duration;
        int retval = -1;
    
        if (readln(&filter_duration_line, &filter_duration_line_size,
                   PATH_FILTER_DURATION) == -1)
                return -1;

        errno = 0; /* Needed to distinguish errors from real return values. */
        duration = strtod(filter_duration_line, &strtod_endptr);
        if (errno != 0)
                goto out;

        /* No conversion was made because the file was empty or dirty. */
        if (duration == 0 && filter_duration_line == strtod_endptr) {
                retval = SETTINGS_ERROR_DIRTY_FILTER_DURATION_FILE;
                goto out;
        }

        settings->filter_duration = duration;
        retval = 0;
out:
        free(filter_duration_line);
        filter_duration_line = NULL;
        return retval;
}

static int read_filter_name(struct settings *settings)
{
        if (readln(&settings->filter_name, &settings->filter_name_size,
                   PATH_FILTER_NAME) == -1)
                return -1;
        return 0;
}

static int read_monitor_name(struct settings *settings)
{
        if (readln(&settings->monitor_name, &settings->monitor_name_size,
                   PATH_MONITOR_NAME) == -1)
                return -1;
        return 0;
}

int settings_read(struct settings *settings)
{
        int retval;
        struct settings tmp_settings;

        memset(&tmp_settings, 0, sizeof(struct settings));

        if ((retval = read_filter_duration(&tmp_settings)) != 0)
                return retval;
        if ((retval = read_filter_name(&tmp_settings)) != 0)
                return retval;
        if ((retval = read_monitor_name(&tmp_settings)) != 0)
                return retval;

        /* Safe to copy fresh settings because no error was detected.*/
        memcpy(settings, &tmp_settings, sizeof(struct settings));
        return 0;
}

const char *settings_strerror(int settings_error)
{
        if (SETTINGS_ERROR_NO_ERROR <= settings_error
            && settings_error < SETTINGS_ERROR_COUNT) {
                return SETTINGS_ERROR_STRS[settings_error];
        }
        return SETTINGS_ERROR_UNKNOWN;
}

void settings_free(struct settings *settings)
{
        free(settings->filter_name);
        free(settings->monitor_name);
        memset(settings, 0, sizeof(struct settings));
}
