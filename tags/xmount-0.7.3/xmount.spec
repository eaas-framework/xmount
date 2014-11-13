%define debug_package %{nil}

Name:			xmount
Summary:		Tool to crossmount between multiple input and output harddisk images
Version:		0.7.0
Release:		1%{?dist}
License:		GPL
Group:			Applications/System
URL:			https://www.pinguin.lu/
Source0:		%{name}-%{version}.tar.gz
Buildroot:		%{_tmppath}/%{name}-%{version}-%{release}-root
Requires:		fuse openssl zlib libewf afflib
BuildRequires:		cmake fuse-devel zlib-devel libewf-devel afflib-devel

%description
xmount allows you to convert on-the-fly between multiple input and output
harddisk image formats. xmount creates a virtual file system using FUSE
(Filesystem in Userspace) that contains a virtual representation of the input
harddisk image. The virtual representation can be in raw DD, Apple's Disk Image
format (DMG), VirtualBox's virtual disk file format (VDI), Microsoft's Virtual
Hard Disk Image format (VHD) or in VmWare's VMDK file format. Input images can
be raw DD, EWF (Expert Witness Compression Format) or AFF (Advanced Forensic
Format) files. In addition, xmount also supports virtual write access to the
output files that is redirected to a cache file. This makes it for example
possible to boot acquired harddisk images using QEMU, KVM, VirtualBox, VMware
or alike.

%build
%cmake -DCMAKE_INSTALL_PREFIX=%{_prefix} .
make %{?_smp_mflags}

%install
rm -fr %{buildroot}
make install DESTDIR=%{buildroot}

%makeinstall

%clean
rm -fr %{buildroot}

%post

%preun

%postun

%files
%defattr(-,root,root) 
%{_bindir}/*
%{_mandir}/*
%doc AUTHORS COPYING INSTALL NEWS README ROADMAP

%changelog
* Wed Aug 13 2014 Daniel Gillen <gillen.dan@pinguin.lu> 0.7.0-1
* Release 0.7.0-1
  See ChangeLog for details

