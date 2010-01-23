AC_INIT([evdaemon], [0.1.1], [tuos@codegrove.org], [evdaemon], [http://codegrove.org/evdaemon/])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_CC
if test -z "`lsmod | grep uinput`"
then
  AC_MSG_ERROR([This package needs uinput module.
  To install, run `modprobe -i uinput' with root privileges.
  It is probably wise to add uinput to the list of modules
  loaded at boot time if it is not there yet:
  `grep uinput /etc/modules || echo uinput >> /etc/modules'])
fi
if test -z "`grep uinput /etc/modules`"
then
  AC_MSG_WARN([It is probably wise to add uinput to the list of modules
  loaded at boot time: `echo uinput >> /etc/modules'])
fi
AC_DEFINE([PACKAGE_DESCRIPTION], ["Input event filtering daemon."], [Description])
AC_DEFINE([PACKAGE_AUTHOR], ["Tuomas Räsänen <tuos@codegrove.org>"], [Author])
AC_SUBST([PACKAGE_DESCRIPTION], ["Input event filtering daemon."])
AC_SUBST([PACKAGE_AUTHOR], ["Tuomas Räsänen <tuos@codegrove.org>"])
AC_CHECK_HEADER([libudev.h])
if test "$ac_cv_header_libudev_h" == no
then
  AC_MSG_ERROR([This package needs libudev.h to get compiled.
  Perhaps run `apt-get install libudev-dev' with root privileges?])
fi
EVDEV1=`find /dev/input/by-path/ -regex .*-event-kbd* | tail -n1 | xargs readlink -e || echo /dev/input/eventX`
EVDEV2=`find /dev/input/by-path/ -regex .*-event-mouse* | tail -n1 | xargs readlink -e || echo /dev/input/eventY`
AC_SUBST([EVDEV1])
AC_SUBST([EVDEV2])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
        README
        Makefile
	src/Makefile
        etc/init/Makefile
	etc/init/evdaemon.conf                
])
AC_OUTPUT
