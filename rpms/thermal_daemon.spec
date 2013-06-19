Name:           thermal_daemon
Version:        1.02
Release:        3%{?dist}
Summary:        The "Linux Thermal Daemon" program from 01.org

License:        GPLv2+
URL:            https://github.com/01org/thermal_daemon
%global commit  78b9dd1343bf7350740eb76e3bf87dc207078d9d
%global shortcommit %(c=%{commit}; echo ${c:0:7})
Source0:        https://github.com/01org/thermal_daemon/archive/%{commit}/%{name}-v%{version}-%{shortcommit}.tar.gz

BuildRequires:  automake
BuildRequires:  autoconf
BuildRequires:  glib-devel
BuildRequires:  dbus-glib-devel
BuildRequires:  libxml2-devel
BuildRequires:  systemd
Requires(post): systemd-units
Requires(preun): systemd-units
Requires(postun): systemd-units

%description
Thermal Daemon monitors and controls platform temperature.

%prep
%setup -qn %{name}-%{version}

%build
autoreconf -f -i
%configure prefix=/usr
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
%make_install DESTDIR=$RPM_BUILD_ROOT

%post
%systemd_post thermald.service

%preun
%systemd_preun thermald.service

%postun
%systemd_postun_with_restart thermald.service 

%files
%{_bindir}/thermald
%config(noreplace) %{_sysconfdir}/dbus-1/system.d/org.freedesktop.thermald.conf 
%{_datadir}/dbus-1/system-services/org.freedesktop.thermald.service
%config(noreplace) %{_sysconfdir}/thermald/thermal-conf.xml
%config(noreplace) %{_sysconfdir}/thermald/thermal-cdev-order.xml
%doc COPYING README.txt
%{_mandir}/man1/thermald.1.gz
%{_unitdir}/thermald.service

%changelog
* Wed Jun 19 2013 Srinivas Pandruvada <srinivas.pandruvada@linux.intel.com> 1.02-3
- Removed libxml2 requirement and uses shortcommit in the Source0
* Tue Jun 18 2013 Srinivas Pandruvada <srinivas.pandruvada@linux.intel.com> 1.02-2
- Update spec file after first review
* Fri Jun 14 2013 Srinivas Pandruvada <srinivas.pandruvada@linux.intel.com> 1.02-1
- Initial package
