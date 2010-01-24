AC_INIT([evdaemon], [0.1.1], [tuos@codegrove.org], [evdaemon], [http://codegrove.org/evdaemon/])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall -Werror])
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_CC
PACKAGE_AUTHOR=$(cat AUTHORS | head -n1)
PACKAGE_DESCRIPTION="Monitor Linux event devices and modify their behavior."
AC_DEFINE_UNQUOTED([PACKAGE_DESCRIPTION], ["$PACKAGE_DESCRIPTION"], [Description])
AC_DEFINE_UNQUOTED([PACKAGE_AUTHOR], ["$PACKAGE_AUTHOR"], [Author])
AC_SUBST([PACKAGE_DESCRIPTION])
AC_SUBST([PACKAGE_AUTHOR])
AC_CHECK_HEADER([libudev.h])
if test "$ac_cv_header_libudev_h" == no
then
  AC_MSG_ERROR([This package needs libudev.h to get compiled.])
fi
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
        README
        Makefile
	src/Makefile
        etc/init/Makefile
        etc/init/evdaemon.conf
])
AC_OUTPUT
