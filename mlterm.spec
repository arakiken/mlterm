%define name mlterm
%define version 2.2.0
%define release 1
%define prefix /usr
%define bindir /usr/X11R6/bin
%define libdir /usr/lib
%define mandir /usr/man
%define sysconfdir /etc/X11
%define libexecdir /usr/libexec/mlterm
%define pixmapdir  /usr/share/pixmaps

Summary:     Multi Lingual TERMinal emulator for X
Name:	     %{name}
Version:     %{version}
Release:     %{release}
License:     Modified BSD-style license
Group:	     User Interface/X
URL:         http://mlterm.sourceforge.net/
Source0:     http://prdownloads.sourceforge.net/mlterm/mlterm-%{version}.tar.gz
Packager:    Araki Ken <j00v0113@ip.media.kyoto-u.ac.jp>
Requires:    imlib, gtk+
BuildRoot:   /var/tmp/%{name}-%{version}-root
BuildPreReq: imlib-devel, gtk+-devel

%description
mlterm is a multi-lingual terminal emulator written from
scratch, which supports various character sets and encodings
in the world.  It also supports various unique feature such as
anti-alias using FreeType, multiple windows, scrollbar API,
scroll by mouse wheel, automatic selection of encoding,
and so on. Multiple xims are also supported. 
You can dynamically change various xims.

%prep
%setup
rm -rf doc/{en,ja}/CVS

%build
CFLAGS="$RPM_OPT_FLAGS" \
./configure --prefix=%{prefix} \
            --bindir=%{bindir} \
	    --libdir=%{libdir} \
	    --mandir=%{mandir} \
            --libexecdir=%{libexecdir} \
	    --sysconfdir=%{sysconfdir}\
	    --enable-imlib # --enable-anti-alias
make

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

mkdir -p $RPM_BUILD_ROOT%{pixmapdir}
install -m 644 $RPM_BUILD_DIR/%{name}-%{version}/doc/icon/mlterm* \
               $RPM_BUILD_ROOT%{pixmapdir}
	       
%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc ChangeLog LICENCE README doc/{en,ja}
%{bindir}/mlterm
%{libdir}/libkik.*
%{libdir}/libmkf.*
%{libdir}/mlterm/
%{libexecdir}/
%{sysconfdir}/mlterm/
%{mandir}/man1/mlterm.1*
%{pixmapdir}/mlterm*

%changelog
* Tue Jan 29 2002 Araki Ken <j00v0113@ip.media.kyoto-u.ac.jp>
- Source version 2.2.0

* Wed Jan 2 2002 Araki Ken <j00v0113@ip.media.kyoto-u.ac.jp>
- Source version 2.1.2

* Sun Dec 30 2001 Araki Ken <j00v0113@ip.media.kyoto-u.ac.jp>
- Source version 2.1.1

* Sat Dec 29 2001 Araki Ken <j00v0113@ip.media.kyoto-u.ac.jp>
- Source version 2.1.0

* Thu Nov 29 2001 Araki Ken <j00v0113@ip.media.kyoto-u.ac.jp>
- Source version 2.0.0

* Mon Nov 26 2001 Araki Ken <j00v0113@ip.media.kyoto-u.ac.jp>
- Source version 1.9.47

* Sat Nov 24 2001 Araki Ken <j00v0113@ip.media.kyoto-u.ac.jp>
- Source version 1.9.46

* Fri Nov 23 2001 Araki Ken <j00v0113@ip.media.kyoto-u.ac.jp>
- Source version 1.9.45

* Sat Nov 17 2001 Araki Ken <j00v0113@ip.media.kyoto-u.ac.jp>
- Source version 1.9.44

* Wed Nov 14 2001 Araki Ken <j00v0113@ip.media.kyoto-u.ac.jp>
- Source version 1.9.43

* Tue Nov 13 2001 Araki Ken <j00v0113@ip.media.kyoto-u.ac.jp>
- Source version 1.9.42pl6
