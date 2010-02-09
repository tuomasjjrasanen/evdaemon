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

#define SETTINGS_ERROR_NO_ERROR                   0
#define SETTINGS_ERROR_DIRTY_FILTER_DURATION_FILE 1

struct settings {
        char   *monitor_devnode;
        char   *filter_devnode;
        double filter_duration;
};

const char *settings_strerror(int settings_error);

int settings_read(struct settings *settings);

#endif /* SETTINGS_H */
