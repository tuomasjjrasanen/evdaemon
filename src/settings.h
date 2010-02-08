#ifndef SETTINGS_H
#define SETTINGS_H

struct settings {
        char   *monitor_devnode;
        char   *filter_devnode;
        double filter_duration;
};

int settings_read(struct settings *settings);

#endif /* SETTINGS_H */
