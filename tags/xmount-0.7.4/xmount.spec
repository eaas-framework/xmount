%define debug_package %{nil}

Name:			xmount
Summary:		Tool to crossmount between multiple input and output harddisk images
Version:		0.7.4
Release:		1%{?dist}
License:		GPL
Group:			Applications/System
URL:			https://www.pinguin.lu/xmount
Source0:		%{name}-%{version}.tar.gz
Buildroot:		%{_tmppath}/%{name}-%{version}-%{release}-root
Requires:		fuse zlib libewf afflib
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

%prep
%setup -q

%build
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_SKIP_RPATH=ON -DCMAKE_INSTALL_PREFIX=%{_prefix} ..
make %{?_smp_mflags}

%install
cd build
%{__make} DESTDIR=%{buildroot} install

%clean
rm -fr %{buildroot}

%post

%preun

%postun

%files
%defattr(-,root,root) 
%{_bindir}/*
%{_mandir}/*
%{_exec_prefix}/lib/%{name}/*.so
%doc AUTHORS COPYING INSTALL NEWS README ROADMAP

%changelog
* Wed Aug 20 2015 Daniel Gillen <gillen.dan@pinguin.lu> 0.7.4-1
* Release 0.7.4-1
  See NEWS for details
— build package
