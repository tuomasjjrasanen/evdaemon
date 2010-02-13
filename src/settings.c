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
#include <limits.h>

#include "config.h"
#include "settings.h"
#include "util.h"

#define SETTINGS_ERROR_COUNT 9
static const char *SETTINGS_ERROR_STRS[SETTINGS_ERROR_COUNT] = {
        "",
        "unknown settings error",
        "dirty or empty filter duration file",
        "dirty or empty clone id bustype file",
        "dirty or empty clone id vendor file",
        "dirty or empty clone id product file",
        "dirty or empty clone id version file",
        "dirty or empty monitor key file",
        "dirty or empty filter key file",
};

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
                retval = SETTINGS_ERROR_FILTER_DURATION;
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

static int read_clone_name(struct settings *settings)
{
        FILE *file;
        size_t chars_fread;
        int retval = -1;
        int orig_errno;

        if ((file = fopen(PATH_CLONE_NAME, "r")) == NULL)
                return -1;

        chars_fread = fread(settings->clone_name, sizeof(char),
                            UINPUT_MAX_NAME_SIZE - 1, file);

        if (ferror(file))
                goto out;

        /* Remove trailing newline character if it exists. */
        if (settings->clone_name[chars_fread-1] == '\n')
                settings->clone_name[chars_fread-1] = '\0';

        retval = 0;
out:
        orig_errno = errno;
        fclose(file);
        errno = orig_errno;
        return retval;
}

static int read_clone_id_member(uint16_t *valuep, const char *path,
                                int errretval)
{
        char *line = NULL;
        size_t line_size;
        char *strtoul_endptr = NULL;
        unsigned long int value;
        int retval = -1;
    
        if (readln(&line, &line_size, path) == -1)
                return -1;

        errno = 0; /* Needed to distinguish errors from real return values. */
        value = strtoul(line, &strtoul_endptr, 10);
        if (errno != 0)
                goto out;

        /* No conversion was made because the file was empty or dirty. */
        if (value == 0 && line == strtoul_endptr) {
                retval = errretval;
                goto out;
        }

        if (value > USHRT_MAX) {
                errno = ERANGE;
                goto out;
        }

        *valuep = (uint16_t) value;
        retval = 0;
out:
        free(line);
        line = NULL;
        return retval;
}

static int read_clone_id(struct input_id *clone_id)
{
        int retval;
        struct input_id tmp_clone_id;

        memset(&tmp_clone_id, 0, sizeof(struct input_id));

        retval = read_clone_id_member(&tmp_clone_id.bustype,
                                      PATH_CLONE_ID_BUSTYPE,
                                      SETTINGS_ERROR_CLONE_ID_BUSTYPE);
        if (retval != 0)
                return retval;

        retval = read_clone_id_member(&tmp_clone_id.vendor,
                                      PATH_CLONE_ID_VENDOR,
                                      SETTINGS_ERROR_CLONE_ID_VENDOR);
        if (retval != 0)
                return retval;

        retval = read_clone_id_member(&tmp_clone_id.product,
                                      PATH_CLONE_ID_PRODUCT,
                                      SETTINGS_ERROR_CLONE_ID_PRODUCT);
        if (retval != 0)
                return retval;

        retval = read_clone_id_member(&tmp_clone_id.version,
                                      PATH_CLONE_ID_VERSION,
                                      SETTINGS_ERROR_CLONE_ID_VERSION);
        if (retval != 0)
                return retval;

        memcpy(clone_id, &tmp_clone_id, sizeof(struct input_id));
        return 0;
}

static int read_monitor_keys(uint64_t *valuev)
{
        char *key_line = NULL;
        size_t key_line_size;
        int retval = -1;

        if (readln(&key_line, &key_line_size,
                   PATH_MONITOR_CAPABILITIES_KEY) == -1)
                return -1;

        switch (strtovaluev(valuev, KEY_VALUEC, key_line)) {
        case 0:
                break;
        case -1:
                goto out;
        case -2:
                retval = SETTINGS_ERROR_DIRTY_MONITOR_KEY;
                goto out;
        case -3:
                retval = SETTINGS_ERROR_DIRTY_MONITOR_KEY;
                goto out;
        default:
                retval = SETTINGS_ERROR_UNKNOWN;
                goto out;
        }
        retval = 0;
out:
        free(key_line);
        return retval;
}

static int read_filter_keys(uint64_t *valuev)
{
        char *key_line = NULL;
        size_t key_line_size;
        int retval = -1;

        if (readln(&key_line, &key_line_size,
                   PATH_FILTER_CAPABILITIES_KEY) == -1)
                return -1;

        switch (strtovaluev(valuev, KEY_VALUEC, key_line)) {
        case 0:
                break;
        case -1:
                goto out;
        case -2:
                retval = SETTINGS_ERROR_DIRTY_FILTER_KEY;
                goto out;
        case -3:
                retval = SETTINGS_ERROR_DIRTY_FILTER_KEY;
                goto out;
        default:
                retval = SETTINGS_ERROR_UNKNOWN;
                goto out;
        }
        retval = 0;
out:
        free(key_line);
        return retval;
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
        if ((retval = read_clone_name(&tmp_settings)) != 0)
                return retval;
        if ((retval = read_clone_id(&tmp_settings.clone_id)) != 0)
                return retval;
        if ((retval = read_monitor_keys(tmp_settings.monitor_key_valuev)) != 0)
                return retval;
        if ((retval = read_filter_keys(tmp_settings.filter_key_valuev)) != 0)
                return retval;

        /* Safe to copy fresh settings because no error was detected.*/
        memcpy(settings, &tmp_settings, sizeof(struct settings));
        return 0;
}

const char *settings_strerror(int settings_error)
{
        if (settings_error < SETTINGS_ERROR_NO_ERROR
            && settings_error >= SETTINGS_ERROR_COUNT)
                settings_error = SETTINGS_ERROR_UNKNOWN;
        return SETTINGS_ERROR_STRS[settings_error];
}

void settings_free(struct settings *settings)
{
        free(settings->filter_name);
        free(settings->monitor_name);
        memset(settings, 0, sizeof(struct settings));
}
