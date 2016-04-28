include $(GLIB_MAKEFILE)

if HAVE_SYSTEMD
systemdsystemunit_DATA = \
	thermald.service

servicedir = $(datadir)/dbus-1/system-services
service_in_files = org.freedesktop.thermald.service.in thermald.service.in
service_DATA = $(service_in_files:.service.in=.service)

$(service_DATA): $(service_in_files) Makefile
	$(edit) $< >$@

dbusservicedir = $(DBUS_SYS_DIR)
dbusservice_DATA = org.freedesktop.thermald.conf
endif

edit = sed \
	-e 's|@bindir[@]|$(bindir)|g' \
	-e 's|@sbindir[@]|$(sbindir)|g' \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@localstatedir[@]|$(localstatedir)|g'

tdconfigdir = $(tdconfdir)
tdconfig_DATA = thermal-conf.xml \
	thermal-cpu-cdev-order.xml

upstartconfdir = $(sysconfdir)/init
upstartconf_DATA = thermald.conf

EXTRA_DIST = \
	$(service_in_files) \
	$(dbusservice_DATA) \
	$(tdconfig_DATA) \
	$(upstartconf_DATA)

CLEANFILES = thermald.service org.freedesktop.thermald.service
