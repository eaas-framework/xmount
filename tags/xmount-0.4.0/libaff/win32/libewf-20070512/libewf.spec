Name: libewf
Version: 20070512
Release: 1
Summary: Library and tools to support the Expert Witness Compression Format
Group: Applications/System
License: BSD
Source: %{name}-%{version}.tar.gz
URL: https://libewf.uitwisselplatform.nl/
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires: zlib openssl
BuildRequires: zlib-devel openssl-devel

%description
libewf is library for support of the Expert Witness Compression Format (EWF).
libewf allows you to read media information of EWF files in the SMART (EWF-S01)
format and the EnCase (EWF-E01) format. libewf allows to read files created by
EnCase 1 to 5, linen and FTK Imager.

Several tools for reading and writing EWF files are included in this package.

%package devel
Summary: Header files and libraries for developing applications which will use libewf
Group: Development/Libraries
Requires: libewf = %{version}-%{release}

%description devel
Header files and libraries for developing applications which will use libewf.

%prep
%setup -q

%build
%configure --prefix=/usr --libdir=%{_libdir} --mandir=%{_mandir}
make %{?_smp_mflags}

%install
rm -rf ${RPM_BUILD_ROOT}
make DESTDIR=${RPM_BUILD_ROOT} install

%clean
rm -rf ${RPM_BUILD_ROOT}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(644,root,root,755)
%doc AUTHORS COPYING NEWS README
%attr(755,root,root) %{_libdir}/*.so.*
%attr(755,root,root) %{_bindir}/ewfacquire
%attr(755,root,root) %{_bindir}/ewfacquirestream
%attr(755,root,root) %{_bindir}/ewfexport
%attr(755,root,root) %{_bindir}/ewfinfo
%attr(755,root,root) %{_bindir}/ewfverify
%{_mandir}/man1/*

### Exclude expirimental files ###
%exclude %{_bindir}/ewfalter
%exclude %{_libdir}/pkgconfig/libewf.pc

%files devel
%defattr(644,root,root,755)
%doc AUTHORS COPYING NEWS README ChangeLog
%{_libdir}/*.a
%{_libdir}/*.la
%{_libdir}/*.so
%{_includedir}/*
%{_mandir}/man3/*

%changelog
* Mon Jan 15 2007 Joachim Metz <forensics@hoffmannbv.nl> 20070115-1
- Added ewfacquirestream to package

* Fri Dec 29 2006 Joachim Metz <forensics@hoffmannbv.nl> 20061229-1
- Added exclusion of new expirimental addtitions

* Tue Dec 26 2006 Christophe Grenier <grenier@cgsecurity.org> 20061223-2
- Made small correction to the spec file, removed abundant Requires line

* Sat Dec 23 2006 Joachim Metz <forensics@hoffmannbv.nl> 20061223-1
- Made small corrections to the spec file input by Christophe Grenier
- Added --libdir to ./configure to correct for /usr/lib64

* Sat Dec 19 2006 Joachim Metz <forensics@hoffmannbv.nl> 20061219-1
- Made small corrections to the spec file input by Christophe Grenier
- The library source package no longer contains a release number

* Sat Dec 16 2006 Christophe Grenier <grenier@cgsecurity.org> 20061213-2
- Fixed the spec file

* Sat Dec 9 2006 Joachim Metz <forensics@hoffmannbv.nl> 20061213-1
- Initial version

