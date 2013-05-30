Name:       sensor
Summary:    Sensor framework client library
Version:    0.5.27
Release:    1
Group:      System/Sensor Framework
License:    Apache-2.0
Source0:    sensor-%{version}.tar.gz
BuildRequires:  cmake
BuildRequires:  vconf-keys-devel
BuildRequires:  pkgconfig(sf_common)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(glib-2.0)

%description
Sensor framework client library

%package devel
Summary:    Sensor framework library (devel)
Group:      System/Development
Requires:   %{name} = %{version}-%{release}

%description devel
Sensor framework client library (devel)


%prep
%setup -q -n %{name}-%{version}

%build
%cmake .


make %{?jobs:-j%jobs}

%install
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%license LICENSE
%manifest libslp-sensor.manifest
%defattr(-,root,root,-)
%{_libdir}/libsensor.so.*

%files devel
%defattr(-,root,root,-)
%{_includedir}/sensor/*.h
%{_libdir}/libsensor.so
%{_libdir}/pkgconfig/sensor.pc

