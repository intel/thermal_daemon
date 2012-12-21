include $(GLIB_MAKEFILE)

INCLUDES= -I@top_srcdir@

ACLOCAL_AMFLAGS = 

# Global C Flags
AM_CFLAGS = ${DBUS_CFLAGS}
AM_CXXFLAGS = ${DBUS_CFLAGS}\
		$(XML_CFLAGS) \
		-DTDRUNDIR=\"$(tdrundir)\" \
		-DTDCONFDIR=\"$(tdconfdir)\" \
		-I netlink-if \
		-I src \
		-fpermissive \
		-fopenmp

EXTRA_DIST=Makefile.glib \
	thermald.pc.in

# Programs to build
bin_PROGRAMS = thermald

# Evaluate Table Application
thermald_CPPFLAGS = \
	-DTDLOCALEDIR=\"$(datadir)/locale\"
thermald_includedir = @top_srcdir@
thermald_LDADD = \
        $(DBUS_LIBS) \
        $(GLIB_LIBS) \
        $(LIBNL_LIBS) \
        $(LIBM) \
        $(LIBDL) \
		$(XML_LIBS)

BUILT_SOURCES = \
	thd-dbus-interface.h

thermald_SOURCES = \
		src/main.cpp \
		src/thd_preference.cpp \
		src/thd_sys_fs.cpp \
		src/thd_engine.cpp \
		src/thd_engine_custom.cpp \
		src/thd_zone.cpp \
		src/thd_zone_custom.cpp \
		src/thd_trip_point.cpp \
		src/thd_cdev.cpp \
		src/thd_cdev_custom.cpp \
		src/thd_parse.cpp \
		src/thd_pid.cpp \
		netlink-if/libnetlink.cpp \
		netlink-if/thd_nl_wrapper.cpp

thd-dbus-interface.h: $(top_srcdir)/src/thd-dbus_interface.xml
	$(AM_V_GEN) dbus-binding-tool --prefix=thd_dbus_interface --mode=glib-server --output=$@ $<

CLEANFILES = $(BUILT_SOURCES)

install: install-am
	echo "copy default xml config"
	cp thermal-conf.xml ${tdconfdir} 
