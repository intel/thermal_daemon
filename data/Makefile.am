include $(GLIB_MAKEFILE)

if HAVE_SYSTEMD

systemdsystemunit_DATA = \
	thermald.service

thermald.service: thermald.service.in
	@$(service_edit) $< >$@

servicedir = $(datadir)/dbus-1/system-services
service_in_files = org.freedesktop.thermald.service.in
service_DATA = $(service_in_files:.service.in=.service)

udev_in_files = 99-thermald-powercap.rules.in
udevrules_DATA = $(udev_in_files:.rules.in=.rules)

%.rules: %.rules.in Makefile
	@$(service_edit) $< >$@

$(service_DATA): $(service_in_files) Makefile
	@$(service_edit) $< >$@

service_edit = sed \
	-e 's|@bindir[@]|$(bindir)|g' \
	-e 's|@sbindir[@]|$(sbindir)|g' \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@localstatedir[@]|$(localstatedir)|g'

endif # HAVE_SYSTEMD

dbusservicedir = $(DBUS_SYS_DIR)
dbusservice_DATA = org.freedesktop.thermald.conf

tdconfigdir = $(tdconfdir)
tdconfig_DATA = \
	thermal-cpu-cdev-order.xml \
	thermald-features.xml

# thermal-conf.xml ships example/default platform policy rules (including
# the Qualcomm Glymur block), itmt-pid.xml the documented default ITMT PID
# gains. Install them only if they don't already exist, so make install
# never clobbers an admin's customized config.
install-data-hook:
	$(MKDIR_P) $(DESTDIR)$(tdconfigdir)
	test -f $(DESTDIR)$(tdconfigdir)/thermal-conf.xml || \
		$(INSTALL_DATA) $(srcdir)/thermal-conf.xml $(DESTDIR)$(tdconfigdir)/thermal-conf.xml
	test -f $(DESTDIR)$(tdconfigdir)/itmt-pid.xml || \
		$(INSTALL_DATA) $(srcdir)/itmt-pid.xml $(DESTDIR)$(tdconfigdir)/itmt-pid.xml

EXTRA_DIST = \
	thermald.service.in \
	org.freedesktop.thermald.service.in \
	99-thermald-powercap.rules.in \
	$(dbusservice_DATA) \
	$(tdconfig_DATA) \
	thermal-conf.xml \
	itmt-pid.xml

CLEANFILES = thermald.service org.freedesktop.thermald.service 99-thermald-powercap.rules
