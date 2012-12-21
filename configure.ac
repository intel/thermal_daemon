AC_PREREQ(1.0)

m4_define([td_major_version], [0])
m4_define([td_minor_version], [0])
m4_define([td_version],
          [td_major_version.td_minor_version])

AC_INIT([thermald], [td_version], [http://bugzilla.gnome.org/enter_bug_cgi?product=thermald], [thermald])

AC_CONFIG_AUX_DIR(build-aux)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 foreign no-define])
AM_MAINTAINER_MODE([enable])
AM_CONFIG_HEADER(config.h)

# paths
AC_SUBST(tdbinary, "$sbindir/$PACKAGE", [Binary executable])
AC_SUBST(tdconfdir, "$sysconfdir/$PACKAGE", [Configuration directory])
AC_SUBST(tdrundir, "$localstatedir/run/$PACKAGE", [Runtime state directory])

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

GLIB_VERSION_DEFINES="-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_26 '-DGLIB_VERSION_MAX_ALLOWED=G_ENCODE_VERSION(2,34)'"
DBUS_CFLAGS="$DBUS_CFLAGS $GLIB_VERSION_DEFINES"

PKG_CHECK_MODULES(GLIB, gio-unix-2.0 >= 2.22 gmodule-2.0)
GLIB_CFLAGS="$GLIB_CFLAGS $GLIB_VERSION_DEFINES"
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(XML, libxml-2.0 >= 2.4)

AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_INSTALL
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
