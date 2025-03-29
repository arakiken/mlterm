%define name mlterm
%define version 3.9.4
%define release 1
%define prefix /usr
%define bindir /usr/bin
%define libdir /usr/lib
%define mandir /usr/share/man
%define datadir /usr/share
%define sysconfdir /etc
%define libexecdir /usr/libexec/mlterm
%define pixmapdir  /usr/share/pixmaps

Summary:     Multi Lingual TERMinal emulator on X
Name:	     %{name}
Version:     %{version}
Release:     %{release}
License:     Modified BSD-style license
Group:	     User Interface/X
URL:         http://mlterm.sourceforge.net/
Source0:     http://prdownloads.sourceforge.net/mlterm/mlterm-%{version}.tar.gz
Packager:    The mlterm team
Requires:    gtk3
BuildRoot:   /var/tmp/%{name}-%{version}-root

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

%build
CFLAGS="$RPM_OPT_FLAGS" \
./configure --prefix=%{prefix} \
	    --bindir=%{bindir} \
	    --libdir=%{libdir} \
	    --mandir=%{mandir} \
	    --libexecdir=%{libexecdir} \
	    --datadir=%{datadir} \
	    --sysconfdir=%{sysconfdir} \
            --with-type-engines=xcore,xft,cairo
	    # --with-imagelib=gdk-pixbuf
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
%{bindir}/mlclient
%{bindir}/mlclientx
%{bindir}/mlcc
%{bindir}/mlfc
%{libdir}/libpobl.*
%{libdir}/libmef.*
#%{libdir}/libmlterm_core.*
%{libdir}/libmlterm_coreotl.*
%{libdir}/mef/
%{libdir}/mlterm/
%{libexecdir}/mlterm/
%{sysconfdir}/mlterm/
%{mandir}/man1/mlterm.1*
%{mandir}/man1/mlclient.1*
%{mandir}/man1/mlcc.1*
%{pixmapdir}/mlterm*
%{datadir}/locale/*/LC_MESSAGES/mlconfig.mo
%{datadir}/mlterm/scrollbars/sample3/

%changelog
* Sun Mar 30 2025 Araki Ken <arakiken@users.sf.net>
- Source version 3.9.4

* Sat Apr 1 2023 Araki Ken <arakiken@users.sf.net>
- Source version 3.9.3

* Sun Jan 16 2022 Araki Ken <arakiken@users.sf.net>
- Source version 3.9.2

* Sun Nov 08 2020 Araki Ken <arakiken@users.sf.net>
- Source version 3.9.1

* Sun Apr 26 2020 Araki Ken <arakiken@users.sf.net>
- Source version 3.9.0

* Fri Nov 22 2019 Araki Ken <arakiken@users.sf.net>
- Source version 3.8.9

* Sun Mar 31 2019 Araki Ken <arakiken@users.sf.net>
- Source version 3.8.8

* Sat Oct 27 2018 Araki Ken <arakiken@users.sf.net>
- Source version 3.8.7

* Tue Jun 12 2018 Araki Ken <arakiken@users.sf.net>
- Source version 3.8.6

* Wed Mar 07 2018 Araki Ken <arakiken@users.sf.net>
- Source version 3.8.5

* Sat Dec 02 2017 Araki Ken <arakiken@users.sf.net>
- Source version 3.8.4

* Sat Sep 23 2017 Araki Ken <arakiken@users.sf.net>
- Source version 3.8.3

* Mon Jul 17 2017 Araki Ken <arakiken@users.sf.net>
- Source version 3.8.2

* Sat Apr 29 2017 Araki Ken <arakiken@users.sf.net>
- Source version 3.8.1

* Sat Jan 7 2017 Araki Ken <arakiken@users.sf.net>
- Source version 3.8.0

* Mon Aug 15 2016 Araki Ken <arakiken@users.sf.net>
- Source version 3.7.2

* Sun May 8 2016 Araki Ken <arakiken@users.sf.net>
- Source version 3.7.1

* Sun Mar 13 2016 Araki Ken <arakiken@users.sf.net>
- Source version 3.7.0

* Sun Feb 14 2016 Araki Ken <arakiken@users.sf.net>
- Source version 3.6.3

* Sat Feb 13 2016 Araki Ken <arakiken@users.sf.net>
- Source version 3.6.2

* Tue Dec 15 2015 Araki Ken <arakiken@users.sf.net>
- Source version 3.6.1

* Sun Oct 25 2015 Araki Ken <arakiken@users.sf.net>
- Source version 3.6.0

* Sat Aug 29 2015 Araki Ken <arakiken@users.sf.net>
- Source version 3.5.1

* Sun Jun 7 2015 Araki Ken <arakiken@users.sf.net>
- Source version 3.5.0

* Sat Mar 14 2015 Araki Ken <arakiken@users.sf.net>
- Source version 3.4.5

* Tue Mar 10 2015 Araki Ken <arakiken@users.sf.net>
- Source version 3.4.4

* Wed Feb 11 2015 Araki Ken <arakiken@users.sf.net>
- Source version 3.4.3

* Tue Jan 6 2015 Araki Ken <arakiken@users.sf.net>
- Source version 3.4.2

* Wed Dec 3 2014 Araki Ken <arakiken@users.sf.net>
- Source version 3.4.1

* Tue Oct 28 2014 Araki Ken <arakiken@users.sf.net>
- Source version 3.4.0

* Sat Aug 16 2014 Araki Ken <arakiken@users.sf.net>
- Source version 3.3.8

* Sun Jul 06 2014 Araki Ken <arakiken@users.sf.net>
- Source version 3.3.7

* Sun May 25 2014 Araki Ken <arakiken@users.sf.net>
- Source version 3.3.6

* Sat Apr 26 2014 Araki Ken <arakiken@users.sf.net>
- Source version 3.3.5

* Sat Mar 22 2014 Araki Ken <arakiken@users.sf.net>
- Source version 3.3.4

* Sat Feb 22 2014 Araki Ken <arakiken@users.sf.net>
- Source version 3.3.3

* Sat Dec 21 2013 Araki Ken <arakiken@users.sf.net>
- Source version 3.3.2

* Sat Nov 23 2013 Araki Ken <arakiken@users.sf.net>
- Source version 3.3.1

* Sun Oct 27 2013 Araki Ken <arakiken@users.sf.net>
- Source version 3.3.0

* Tue Aug 06 2013 Araki Ken <arakiken@users.sf.net>
- Source version 3.2.2

* Sat Jun 29 2013 Araki Ken <arakiken@users.sf.net>
- Source version 3.2.1

* Sun May 26 2013 Araki Ken <arakiken@users.sf.net>
- Source version 3.2.0

* Sat Mar 23 2013 Araki Ken <arakiken@users.sf.net>
- Source version 3.1.9

* Sat Feb 23 2013 Araki Ken <arakiken@users.sf.net>
- Source version 3.1.8

* Tue Jan 22 2013 Araki Ken <arakiken@users.sf.net>
- Source version 3.1.7

* Sat Dec 01 2012 Araki Ken <arakiken@users.sf.net>
- Source version 3.1.6

* Tue Nov 06 2012 Araki Ken <arakiken@users.sf.net>
- Source version 3.1.5

* Fri Sep 28 2012 Araki Ken <arakiken@users.sf.net>
- Source version 3.1.4

* Fri Aug 10 2012 Araki Ken <arakiken@users.sf.net>
- Source version 3.1.3

* Sat May 19 2012 Araki Ken <arakiken@users.sf.net>
- Source version 3.1.2

* Sun Apr 29 2012 Araki Ken <arakiken@users.sf.net>
- Source version 3.1.1

* Sat Apr 21 2012 Araki Ken <arakiken@users.sf.net>
- Source version 3.1.0

* Wed Jan 18 2012 Araki Ken <arakiken@users.sf.net>
- Source version 3.0.11

* Sat Dec 17 2011 Araki Ken <arakiken@users.sf.net>
- Source version 3.0.10

* Sat Nov 19 2011 Araki Ken <arakiken@users.sf.net>
- Source version 3.0.9

* Sat Sep 24 2011 Araki Ken <arakiken@users.sf.net>
- Source version 3.0.8

* Mon Sep 19 2011 Araki Ken <arakiken@users.sf.net>
- Source version 3.0.7

* Sat Jul 23 2011 Araki Ken <arakiken@users.sf.net>
- Source version 3.0.6

* Sat Jun 04 2011 Araki Ken <arakiken@users.sf.net>
- Source version 3.0.5

* Sun May 29 2011 Araki Ken <arakiken@users.sf.net>
- Source version 3.0.4

* Sun Mar 20 2011 Araki Ken <arakiken@users.sf.net>
- Source version 3.0.3

* Sun Jan 02 2011 Araki Ken <arakiken@users.sf.net>
- Source version 3.0.2
 
* Mon Jun 07 2010 Araki Ken <arakiken@users.sf.net>
- Source version 3.0.1

* Sat Apr 10 2010 Araki Ken <arakiken@users.sf.net>
- Source version 3.0.0

* Fri Nov 30 2007 Seiichi SATO <me@seiichisato.jp>
- Source version 2.9.4

* Sun May 07 2006 Seiichi SATO <me@seiichisato.jp>
- Source version 2.9.3

* Fri Mar 04 2005 Seiichi SATO <ssato@sh.rim.or.jp>
- Source version 2.9.2

* Sun Nov 28 2004 Seiichi SATO <ssato@sh.rim.or.jp>
- Source version 2.9.1

* Thu Nov 25 2004 Seiichi SATO <ssato@sh.rim.or.jp>
- Fixed #1072304
- FHS compliance

* Sun Oct 24 2004 Seiichi SATO <ssato@sh.rim.or.jp>
- Source version 2.9.0

* Sun Oct 05 2003 Araki Ken <arakiken@users.sf.net>
- Source version 2.8.0

* Sat Jun 14 2003 Araki Ken <arakiken@users.sf.net>
- Source version 2.7.0

* Sun Jan 12 2003 Araki Ken <arakiken@users.sf.net>
- Source version 2.6.3

* Tue Oct 1 2002 Araki Ken <arakiken@users.sf.net>
- Source version 2.6.2

* Thu Sep 12 2002 Araki Ken <arakiken@users.sf.net>
- Source version 2.6.1

* Sat Sep 7 2002 Araki Ken <arakiken@users.sf.net>
- Source version 2.6.0

* Sun Jun 16 2002 Araki Ken <arakiken@users.sourceforge.net>
- Source version 2.5.0

* Sun Apr 14 2002 Araki Ken <arakiken@users.sourceforge.net>
- Source version 2.4.0

* Mon Feb 25 2002 Araki Ken <j00v0113@ip.media.kyoto-u.ac.jp>
- Source version 2.3.1

* Sat Feb 23 2002 Araki Ken <j00v0113@ip.media.kyoto-u.ac.jp>
- Source version 2.3.0

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
