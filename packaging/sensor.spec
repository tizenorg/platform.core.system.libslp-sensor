
Name:       sensor
Summary:    Sensor framework client library
Version:    0.5.26
Release:    1
Group:      TO_BE/FILLED_IN
License:    Apache 2.0
Source0:    sensor-%{version}.tar.gz
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  cmake
BuildRequires:  vconf-keys-devel
BuildRequires:  pkgconfig(sf_common)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(glib-2.0)

%description
Sensor framework client library



%package devel
Summary:    Sensor framework library (devel)
Group:      System/Sensor Framework
Requires:   %{name} = %{version}-%{release}

%description devel
Sensor framework client library (devel)


%prep
%setup -q -n %{name}-%{version}


%build
%cmake .


make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}/usr/share/license
cp LICENSE %{buildroot}/usr/share/license/%{name}


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig





%files
%manifest libslp-sensor.manifest
%defattr(-,root,root,-)
%{_libdir}/libsensor.so.*
/usr/share/license/%{name}

%files devel
%defattr(-,root,root,-)
%{_includedir}/sensor/*.h
%{_libdir}/libsensor.so
%{_libdir}/pkgconfig/sensor.pc

