include $(GLIB_MAKEFILE)

SUBDIRS = data

INCLUDES= -I@top_srcdir@/src

ACLOCAL_AMFLAGS = 

# Global C Flags
AM_CFLAGS = ${DBUS_CFLAGS}
AM_CXXFLAGS = ${DBUS_CFLAGS}\
		$(XML_CFLAGS) \
		-DTDRUNDIR=\"$(tdrundir)\" \
		-DTDCONFDIR=\"$(tdconfdir)\" \
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
		src/thd_engine.cpp \
		src/thd_cdev.cpp \
		src/thd_engine_therm_sys_fs.cpp \
		src/thd_engine_dts.cpp \
		src/thd_cdev_pstates.cpp \
		src/thd_cdev_msr_pstates.cpp \
		src/thd_cdev_turbo_states.cpp \
		src/thd_cdev_therm_sys_fs.cpp \
		src/thd_cdev_tstates.cpp \
		src/thd_cdev_intel_pstate_driver.cpp \
		src/thd_cdev_rapl.cpp \
		src/thd_msr.cpp \
		src/thd_sys_fs.cpp \
		src/thd_trip_point.cpp \
		src/thd_zone.cpp \
		src/thd_zone_dts.cpp \
		src/thd_zone_dts_sensor.cpp \
		src/thd_zone_therm_sys_fs.cpp \
		src/thd_preference.cpp \
		src/thd_model.cpp \
		src/thd_parse.cpp \
		src/thd_topology.cpp \
		src/thd_kobj_uevent.cpp \
		src/thd_rapl_interface.cpp \
		src/thd_cdev_msr_rapl.cpp \
		src/thd_cdev_order_parser.cpp


man1_MANS = man/thermald.1

thd-dbus-interface.h: $(top_srcdir)/src/thd-dbus_interface.xml
	$(AM_V_GEN) dbus-binding-tool --prefix=thd_dbus_interface --mode=glib-server --output=$@ $<

CLEANFILES = $(BUILT_SOURCES)
