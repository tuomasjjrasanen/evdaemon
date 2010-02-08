#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <sys/time.h>

int bit_test(int bit_i, const uint8_t *bytes);

double timestamp(const struct timeval *tv);

const char *get_uinput_devnode();

int clone_evdev(int evdev_fd);

#endif /* UTIL_H */
