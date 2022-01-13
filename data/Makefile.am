include $(GLIB_MAKEFILE)

if HAVE_SYSTEMD

systemdsystemunit_DATA = \
	thermald.service

thermald.service: thermald.service.in
	@$(service_edit) $< >$@

servicedir = $(datadir)/dbus-1/system-services
service_in_files = org.freedesktop.thermald.service.in
service_DATA = $(service_in_files:.service.in=.service)

$(service_DATA): $(service_in_files) Makefile
	@$(service_edit) $< >$@

service_edit = sed \
	-e 's|@bindir[@]|$(bindir)|g' \
	-e 's|@sbindir[@]|$(sbindir)|g' \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@localstatedir[@]|$(localstatedir)|g'

endif # HAVE_SYSTEMD

dbusservice_edit = sed \
	-e 's|@dbusservicepowergrp[@]|$(dbusservicepowergrp)|g'

org.freedesktop.thermald.conf: org.freedesktop.thermald.conf.in
	@$(dbusservice_edit) $< >$@

dbusservicepowergrp = $(DBUS_POWER_GROUP)
dbusservicedir = $(DBUS_SYS_DIR)
dbusservice_in_files = org.freedesktop.thermald.conf.in
dbusservice_DATA = $(dbusservice_in_files:.conf.in=.conf)

tdconfigdir = $(tdconfdir)
tdconfig_DATA = \
	thermal-cpu-cdev-order.xml

EXTRA_DIST = \
	thermald.service.in \
	org.freedesktop.thermald.service.in \
	$(dbusservice_DATA) \
	$(tdconfig_DATA)

CLEANFILES = thermald.service org.freedesktop.thermald.service
