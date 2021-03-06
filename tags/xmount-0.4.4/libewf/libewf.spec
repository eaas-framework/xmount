Name: libewf
Version: 20100226
Release: 1
Summary: Library to support the Expert Witness Compression Format
Group: System Environment/Libraries
License: LGPL
Source: %{name}-%{version}.tar.gz
URL: http://libewf.sourceforge.net
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires:   zlib
BuildRequires:   zlib-devel

%description
libewf is a library for support of the Expert Witness Compression Format (EWF).
libewf allows you to read media information of EWF files in the SMART (EWF-S01)
format and the EnCase (EWF-E01) format. libewf allows to read files created by
EnCase 1 to 6, linen and FTK Imager.

%package devel
Summary: Header files and libraries for developing applications for libewf
Group: Development/Libraries
Requires: libewf = %{version}-%{release}

%description devel
Header files and libraries for developing applications for libewf.

%package tools
Summary: Several tools for reading and writing EWF files
Group: Applications/System
Requires: openssl e2fsprogs-devel libewf = %{version}-%{release}
BuildRequires: zlib-devel openssl-devel e2fsprogs-devel

%description tools
Several tools for reading and writing EWF files.
It contains tools to acquire, verify and export EWF files.

%prep
%setup -q

%build
%configure --prefix=/usr --libdir=%{_libdir} --mandir=%{_mandir} --enable-v2-api=yes
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

%files devel
%defattr(644,root,root,755)
%doc AUTHORS COPYING NEWS README ChangeLog
%{_libdir}/*.a
%{_libdir}/*.la
%{_libdir}/*.so
%{_libdir}/pkgconfig/libewf.pc
%{_includedir}/*
%{_mandir}/man3/*

%files tools
%defattr(644,root,root,755)
%doc AUTHORS COPYING NEWS README
%attr(755,root,root) %{_bindir}/ewfacquire
%attr(755,root,root) %{_bindir}/ewfacquirestream
%attr(755,root,root) %{_bindir}/ewfexport
%attr(755,root,root) %{_bindir}/ewfinfo
%attr(755,root,root) %{_bindir}/ewfverify
%{_mandir}/man1/*

%changelog
* Sat Nov 14 2009 Joachim Metz <forensics@hoffmannbv.nl> 20091114-1
- Removed ewfalter and ewfdebug exclusion for stable release

* Fri Aug 28 2009 Joachim Metz <forensics@hoffmannbv.nl> 20090828-1
- Added dynamic support for libuuid requires and build requires

* Sun Apr 5 2009 Joachim Metz <forensics@hoffmannbv.nl> 20090405-1
- Added exclusion for ewfdebug
- Added default v2 API support

* Sat Mar 7 2009 Joachim Metz <forensics@hoffmannbv.nl> 20090307-1
- Changed libuca into libuna
- Added support for libbfio

* Tue Sep 2 2008 Joachim Metz <forensics@hoffmannbv.nl> 20080902-1
- Changed project website
- Added support for libuca

* Sat Aug 9 2008 Joachim Metz <forensics@hoffmannbv.nl> 20080809-1
- Changed license

* Sun May 11 2008 Joachim Metz <forensics@hoffmannbv.nl> 20080511-1
- Fixed a typo

* Thu May 1 2008 Joachim Metz <forensics@hoffmannbv.nl> 20080501-1
- Added some addition text to the description of the tools package

* Wed Mar 12 2008 Joachim Metz <forensics@hoffmannbv.nl> 20080312-1
- Added requirement for e2fsprogs-devel package for libuuid

* Sat Dec 29 2007 Joachim Metz <forensics@hoffmannbv.nl> 20071229-1
- Updated URL

* Sun Dec 9 2007 Joachim Metz <forensics@hoffmannbv.nl> 20071209-1
- Moved pkgconfig file from excluded to development
- Adjustments to Requires and BuildRequires
- Adjusted description of library package removed tools
- Corrected groups

* Sat Sep 15 2007 Joachim Metz <forensics@hoffmannbv.nl> 20070915-1
- Adjustment to text
- library and tools are now stored in seperate packages

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

