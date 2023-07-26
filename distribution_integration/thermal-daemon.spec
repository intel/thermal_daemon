Name:           thermald
Version:        1.6
Release:        1%{?dist}
Summary:        The "Linux Thermal Daemon" program from 01.org

License:        GPLv2+
URL:            https://github.com/intel/thermal_daemon
%global         pkgname thermal_daemon
Source0:        https://github.com/intel/thermal_daemon/archive/v%{version}.tar.gz

BuildRequires:    automake
BuildRequires:    autoconf
BuildRequires:    glib-devel
BuildRequires:    dbus-glib-devel
BuildRequires:    libxml2-devel
BuildRequires:    systemd
Requires(post):   systemd-units
Requires(preun):  systemd-units
Requires(postun): systemd-units

%description
Thermal Daemon monitors and controls platform temperature.

%prep
%setup -qn %{pkgname}-%{version}

%build
autoreconf -f -i
%configure prefix=%{_prefix}
make %{?_smp_mflags}

# Although there is a folder test in the upstream repo, this is not for self tests.
# Hence check section is not present.

%install
%make_install DESTDIR=%{buildroot}

%post
%systemd_post thermald.service

%preun
%systemd_preun thermald.service

%postun
%systemd_postun_with_restart thermald.service

%files
%{_sbindir}/thermald
%{_datadir}/dbus-1/system.d/org.freedesktop.thermald.conf
%{_datadir}/dbus-1/system-services/org.freedesktop.thermald.service
%config(noreplace) %{_sysconfdir}/thermald/thermal-conf.xml
%config(noreplace) %{_sysconfdir}/thermald/thermal-cpu-cdev-order.xml
%doc COPYING README.txt
%{_mandir}/man8/thermald.8.gz
%{_mandir}/man5/thermal-conf.xml.5.gz
%{_unitdir}/thermald.service
%exclude %{_sysconfdir}/init

%changelog
* Mon Apr 03 2017 James Ye <jye836@gmail.com> 1.6-1
- Updated to thermal daemon 1.6
* Tue Oct 25 2016 Michael P. Moran <> 1.5-4
- Updated spec file for version 1.5.4
* Tue Mar 29 2016 Alexey Slaykovsky <alexey@slaykovsky.com> 1.5-3
- Updated spec file for a Thermal Daemon 1.5.3 version
* Fri Oct 17 2014 António Meireles <antonio.meireles@reformi.st> 1.3-3
- update spec file
* Tue Oct 01 2013 Srinivas Pandruvada <srinivas.pandruvada@linux.intel.com> 1.03-1
- Upgraded to thermal daemon 1.03
* Mon Jun 24 2013 Srinivas Pandruvada <srinivas.pandruvada@linux.intel.com> 1.02-5
- Replaced underscore with dash in the package name
* Thu Jun 20 2013 Srinivas Pandruvada <srinivas.pandruvada@linux.intel.com> 1.02-4
- Resolved prefix and RPM_BUILD_ROOT as per review comments
* Wed Jun 19 2013 Srinivas Pandruvada <srinivas.pandruvada@linux.intel.com> 1.02-3
- Removed libxml2 requirement and uses shortcommit in the Source0
* Tue Jun 18 2013 Srinivas Pandruvada <srinivas.pandruvada@linux.intel.com> 1.02-2
- Update spec file after first review
* Fri Jun 14 2013 Srinivas Pandruvada <srinivas.pandruvada@linux.intel.com> 1.02-1
- Initial package
