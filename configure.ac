AC_PREREQ(1.0)

m4_define([td_major_version], [2])
m4_define([td_minor_version], [4.8])
m4_define([td_version],
          [td_major_version.td_minor_version])

AC_INIT([thermald], [td_version], [], [thermald])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR(build-aux)
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.11 foreign no-define subdir-objects])
AM_MAINTAINER_MODE([enable])

GTK_DOC_CHECK([1.11],[--flavour no-tmpl])

AC_ARG_WITH(dbus-sys-dir, AS_HELP_STRING([--with-dbus-sys-dir=DIR], [where D-BUS system.d directory is]))
if test -n "$with_dbus_sys_dir" ; then
    DBUS_SYS_DIR="$with_dbus_sys_dir"
else
    DBUS_SYS_DIR="/etc/dbus-1/system.d"
fi
AC_SUBST(DBUS_SYS_DIR)

# paths
AC_SUBST(tdbinary, "$sbindir/$PACKAGE", [Binary executable])
AC_SUBST(tdconfdir, "$sysconfdir/$PACKAGE", [Configuration directory])
AC_SUBST(tdrundir, "$localstatedir/run/$PACKAGE", [Runtime state directory])

PKG_PROG_PKG_CONFIG
AC_ARG_WITH([systemdsystemunitdir],
        AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
        [], [with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)])
if test "x$with_systemdsystemunitdir" != xno; then
        AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
fi
AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "$with_systemdsystemunitdir" -a "x$with_systemdsystemunitdir" != xno ])

# print configuration
echo
echo "System paths:"
echo "  prefix: $prefix"
echo "  exec_prefix: $exec_prefix"
echo "  systemdunitdir: $with_systemdsystemunitdir"
echo "  tdbinary: $tdbinary"
echo "  tdconfdir: $tdconfdir"
echo "  tdrundir: $tdrundir"
echo

GETTEXT_PACKAGE=thermald
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package])

dnl
dnl Checks for new dbus-glib property access function
dnl
AC_CHECK_LIB([dbus-glib-1], [dbus_glib_global_set_disable_legacy_property_access], ac_have_dg_prop="1", ac_have_dg_prop="0")
AC_DEFINE_UNQUOTED(HAVE_DBUS_GLIB_DISABLE_LEGACY_PROP_ACCESS, $ac_have_dg_prop, [Define if you have a dbus-glib with dbus_glib_global_set_disable_legacy_property_access()])

PKG_CHECK_MODULES(DBUS, dbus-1 >= 1.1 dbus-glib-1 >= 0.94)
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

GLIB_VERSION_DEFINES="-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_26"
DBUS_CFLAGS="$DBUS_CFLAGS $GLIB_VERSION_DEFINES"

PKG_CHECK_MODULES(GLIB, gio-unix-2.0 >= 2.22 gmodule-2.0)
GLIB_CFLAGS="$GLIB_CFLAGS $GLIB_VERSION_DEFINES"
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(XML, libxml-2.0 >= 2.4)

AC_PATH_PROG([GDBUS_CODEGEN],[gdbus-codegen])
PKG_CHECK_MODULES(LZMA, liblzma)

PKG_CHECK_MODULES(UPOWER, upower-glib)

PKG_CHECK_MODULES(EVDEV, libevdev)

AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_INSTALL
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T

m4_ifdef([AX_CHECK_COMPILE_FLAG], [
# Check for -Werror flag support.
for flag in \
    -Wall \
    -Wclobbered \
    -Wempty-body \
    -Wformat \
    -Wignored-qualifiers \
    -Wmissing-field-initializers \
    -Wreorder \
    -Wreturn-type \
    -Wsign-compare \
    -Wtype-limits \
    -Wuninitialized \
    -Wunused-but-set-variable \
; do
    AX_CHECK_COMPILE_FLAG([-Werror ${flag}],
                          [CXXFLAGS="$CXXFLAGS ${flag}"])
done

# Check for c++0x support.
AX_CHECK_COMPILE_FLAG([-std=c++0x],
                      [CXXFLAGS="$CXXFLAGS -std=c++0x"])
], [AC_MSG_ERROR([AX_CHECK_COMPILE_FLAG not found, you'll need to install autoconf-archive])])

AC_ARG_ENABLE(werror, AS_HELP_STRING([--disable-werror], [Disable -Werror]))
AS_IF([test "x$enable_werror" != "xno"], [CXXFLAGS="$CXXFLAGS -Werror"])

AC_CONFIG_FILES([Makefile
                 docs/Makefile
                 docs/version.xml
                 data/Makefile])

AC_OUTPUT
