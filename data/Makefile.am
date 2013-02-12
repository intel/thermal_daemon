include $(GLIB_MAKEFILE)

if HAVE_SYSTEMD
systemdsystemunit_DATA = \
	thermald.service

thermald.service: thermald.service.in
	$(edit) $< >$@

servicedir = $(datadir)/dbus-1/system-services
service_in_files = org.freedesktop.thermald.service.in
service_DATA = $(service_in_files:.service.in=.service)

$(service_DATA): $(service_in_files) Makefile
	$(edit) $< >$@
endif

edit = sed \
	-e 's|@bindir[@]|$(bindir)|g' \
	-e 's|@sbindir[@]|$(sbindir)|g' \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@localstatedir[@]|$(localstatedir)|g'

EXTRA_DIST = \
	thermald.service.in \
	org.freedesktop.thermald.service.in

CLEANFILES = thermald.service org.freedesktop.thermald.service

install-data:
	cp thermald.service.in /usr/lib/systemd/system/thermald.service
	cp org.freedesktop.thermald.service.in /usr/share/dbus-1/system-services/org.freedesktop.thermald.service
	cp org.freedesktop.thermald.conf /etc/dbus-1/system.d/