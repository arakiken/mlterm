%define name mlterm
%define version 2.1.1
%define release 1
%define prefix /usr
%define bindir /usr/X11R6/bin
%define libdir /usr/lib
%define mandir /usr/man
%define sysconfdir /etc/X11
%define libexecdir /usr/libexec/mlterm

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
and so on.

Supported encodings are:
ISO-8859-[1-10], ISO-8859-[13-16], TCVN5712, KOI8-R, KOI8-U, VISCII,
EUC-JP, EUC-JISX0213, ISO-2022-JP[1-3], Shift_JIS, Shift_JISX0213,
ISO-2022-KR, EUC-KR, UHC, JOHAB, GB2312 (EUC-CN), GBK, ISO-2022-CN, BIG5, BIG5HKSCS
EUC-TW, HZ, TIS-620, UTF-8, and GB18030.
If you have already set locale (for example LANG variable;
see locale(7) for detail) mlterm will automatically select
proper encoding.

%prep
%setup

%build
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

cd $RPM_BUILD_ROOT%{bindir}
ln -sf %{libexecdir}/mlconfig mlconfig
# if [ ! -f mlterm -a -f mlterm-aa ]; then 
#  ln -sf mlterm-aa mlterm
# fi

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc LICENCE README doc/en doc/ja
%{bindir}/mlconfig
%{bindir}/mlterm*
%{libdir}/libkik.*
%{libdir}/libmkf.*
%{libdir}/mlterm/
%{libexecdir}/
%{sysconfdir}/mlterm/
%{mandir}/man1/mlterm.1*

%changelog
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
