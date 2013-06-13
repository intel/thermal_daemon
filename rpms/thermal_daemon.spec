Name:           thermal_daemon
Version:        1.0
Release:        1%{?dist}
Summary:        The "Linux Thermal Daemon" program from 01.org

License:        GPLv2+
URL:            https://github.com/01org/%{name}
Source0:        https://github.com/01org/%{name}/rpms/%{name}-%{version}.tar.gz

BuildRequires:  glib-devel
BuildRequires:  dbus-glib-devel
BuildRequires:  libxml2-devel
BuildRequires:  automake
BuildRequires:  autoconf
BuildRequires:  gcc-c++
Requires:       libxml2

%description
Thermal Daemon monitors and controls platform temperature.

%prep
%setup -q


%build
autoreconf -f -i
%configure prefix=/usr
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
%make_install DESTDIR=$RPM_BUILD_ROOT


%files
%defattr(-,root,root)
%{_bindir}/thermald
%config(noreplace) %{_sysconfdir}/dbus-1/system.d/org.freedesktop.thermald.conf 
%{_datadir}/dbus-1/system-services/org.freedesktop.thermald.service
%config(noreplace) %{_sysconfdir}/thermald/thermal-conf.xml
%config(noreplace) %{_sysconfdir}/thermald/thermal-cdev-order.xml
%doc COPYING README.txt
%{_mandir}/man1/thermald.1.gz
%{_unitdir}/thermald.service

%changelog
* Wed May 8 2013 Base version 1.0-1
- Base version
