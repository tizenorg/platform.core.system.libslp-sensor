
Name:       sensor
Summary:    Sensor framework client library
Version:    0.5.6
Release:    1
Group:      TO_BE/FILLED_IN
License:    TO BE FILLED IN
Source0:    sensor-%{version}.tar.gz
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  cmake
BuildRequires:  pkgconfig(vconf)
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
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}


make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%defattr(-,root,root,-)
#%doc COPYING
%{_libdir}/libsensor.so.*


%files devel
%defattr(-,root,root,-)
%{_includedir}/sensor/*.h
%{_libdir}/libsensor.so
%{_libdir}/pkgconfig/sensor.pc

