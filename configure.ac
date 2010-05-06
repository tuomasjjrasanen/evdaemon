AC_INIT(
        [evdaemon],
        [0.3.2],
        [tuos@codegrove.org],
        [evdaemon],
        [http://codegrove.org/evdaemon/])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall -Werror])
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_CC
PACKAGE_AUTHOR=$(cat AUTHORS | head -n1)
PACKAGE_DESCRIPTION="Monitor Linux event devices and modify their behavior."
AX_DEFINE_DIR(
        [PATH_CONFIG_DIR],
        [sysconfdir/evdaemon],
        [Path to configuration directory.])
AX_DEFINE_DIR(
        [PATH_CLONE_NAME],
        [sysconfdir/evdaemon/clone/name],
        [Path to name file of the clone devie.])
AX_DEFINE_DIR(
        [PATH_CLONE_ID_BUSTYPE],
        [sysconfdir/evdaemon/clone/id/bustype],
        [Path to bustype file of the clone device.])
AX_DEFINE_DIR(
        [PATH_CLONE_ID_VENDOR],
        [sysconfdir/evdaemon/clone/id/vendor],
        [Path to vendor file of the clone device.])
AX_DEFINE_DIR(
        [PATH_CLONE_ID_VERSION],
        [sysconfdir/evdaemon/clone/id/version],
        [Path to version file of the clone device.])
AX_DEFINE_DIR(
        [PATH_CLONE_ID_PRODUCT],
        [sysconfdir/evdaemon/clone/id/product],
        [Path to product file of the clone device.])
AX_DEFINE_DIR(
        [PATH_FILTER_NAME],
        [sysconfdir/evdaemon/filter/name],
        [Path to name file of the filter device.])
AX_DEFINE_DIR(
        [PATH_FILTER_CAPABILITIES_KEY],
        [sysconfdir/evdaemon/filter/capabilities/key],
        [Path to key capabilities file of the filter device.])
AX_DEFINE_DIR(
        [PATH_FILTER_CAPABILITIES_REL],
        [sysconfdir/evdaemon/filter/capabilities/rel],
        [Path to rel capabilities file of the filter device.])
AX_DEFINE_DIR(
        [PATH_FILTER_DURATION],
        [sysconfdir/evdaemon/filter/duration],
        [Path to duration file of the filter device.])
AX_DEFINE_DIR(
        [PATH_MONITOR_NAME],
        [sysconfdir/evdaemon/monitor/name],
        [Path to name file of the monitor device.])
AX_DEFINE_DIR(
        [PATH_MONITOR_CAPABILITIES_KEY],
        [sysconfdir/evdaemon/monitor/capabilities/key],
        [Path to key capabilities file of the monitor device.])
AX_DEFINE_DIR(
        [PATH_MONITOR_CAPABILITIES_REL],
        [sysconfdir/evdaemon/monitor/capabilities/rel],
        [Path to rel capabilities file of the monitor device.])
AC_DEFINE_UNQUOTED(
        [PACKAGE_DESCRIPTION],
        ["$PACKAGE_DESCRIPTION"],
        [Package description.])
AC_DEFINE_UNQUOTED([PACKAGE_AUTHOR], ["$PACKAGE_AUTHOR"], [Author.])
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
        etc/Makefile
        etc/evdaemon/Makefile
])
AC_OUTPUT
