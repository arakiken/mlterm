MLTERM - Multi Lingual TERMinal emulator


COMPILATION
===========

  $ ./configure
  $ make

Options for configure
---------------------

  --prefix
    Installation prefix

  --disable-dl-table
    Disable dynamic loading of character mapping or property table,
    which is statically linked to libmef.
  
  --without-map-table
    Remove character mapping table

  --without-prop-table
    Remove unicode character property table

  --with-iconv
    Use iconv to convert between different character sets.
    (If --with-iconv is specified, --disable-dl-table is the default.)

  --with-gui=(xlib|win32|fb|quartz|console|wayland|sdl2|beos)
    Use specified gui library
    See "Supported platforms" below.

  --with-imagelib=(gdk-pixbuf)
    Link specified image library to mlterm for image processing.
    Note that this option is not necessary for image processing, because
    'mlimgloader' program (which is compiled with gdk-pixbuf if
    --with-tools=mlimgloader option is specified) can process images and
    pass them to mlterm.
    This option is recommended only if you build libvte which depends
    on gdk-pixbuf anyway.
    (Imlib is not supported since mlterm 3.0.2)

  --with-libltdl
    Load modules with libltdl

  --with-type-engines=(xcore|xft|cairo)
    Use specified type engines for rendering text.
    xcore is possible to disable only when --with-type-engines=(except xcore)
    is specified with --disable-dl-type option.

  --disable-anti-alias
    Same as --with-type-engines=xcore on xlib.
    (--enable-anti-alias is the same as --with-type-engines=xcore,xft,cairo)
    This option is ignored if --with-type-engines is specified on xlib.

    Disable truetype fonts on framebuffer or wayland.

  --disable-fontconfig
    Disable fontconfig (use ~/.mlterm/*aafont) on framebuffer or wayland.

  --disable-dl-type
    Disable dynamic loading of type engine modules,
    which are statically linked to mlterm.

  --disable-fribidi
    Disable BiDi rendering
    (see https://github.com/arakiken/mlterm/blob/master/doc/en/README.bidi)

  --disable-ind
    Use libind for Indic rendering
    (see https://github.com/arakiken/mlterm/blob/master/doc/en/README.indic)

  --disable-dl-ctl
    Disable dynamic loading of BiDi and Indic rendering modules,
    which are statically linked to mlterm.
  
  --disable-ssh2
    Disable to connect ssh2 server.
    If enabled, libssh2 (https://www.libssh2.org) is used to connect ssh2 server.
    (Usage: mlterm(mlclient) --serv ssh://user@xxx.xxx.xxx)
    If you want to use camellia as common key cryptography or SSH agent forwarding,
    install alternative libssh2 of camellia-agentfwd branch at
    https://github.com/arakiken/libssh2/archive/camellia-agentfwd.zip
    (see https://github.com/arakiken/mlterm/blob/master/doc/en/README.ssh and
         https://github.com/arakiken/mlterm/blob/master/doc/en/README.transfer)

  --with-mosh=(mosh source directory)
    Enable mosh and specify its source directory, where ./configure script should
    be executed in advance.
    This option is available only if libssh2 is enabled.
    Don't distribute libptymosh in binary format.

  --without-pthread
    Don't depend on pthread for secure copy (scp) over ssh.
    (Furthermore, don't link libpthread to mlterm in NetBSD even if --enable-ibus
     or --with-type-engines=cairo is specified.)

  --enable-debug
    Build debug version

  --enable-optimize-redrawing
    Optimize redrawing a line

  --with-scrollbars
    Build specified scrollbars in scrollbar/ and contrib/scrollbar/ directories

  --with-tools
    Build specified tools in tool/ and contrib/tool/ directories
    mlimgloader is necessary to show images if --with-imagelib option is not specified.
    registobmp is necessary to process ReGIS.

  --disable-use-tools
    Don't build and use external tools.
    If this option is specified, --with-tools option is ignored.

  --disable-dnd
    Disable XDnD

  --disable-kbd
    Disable kbd input method for Arabic, Hebrew and Indic

  --disable-iiimf
    Disaable IIIMF (Internet/Intranet Input Method Framework)

  --disable-uim
    Disable uim (Universal? Input Method)

  --disable-m17nlib
    Disable m17n library

  --disable-scim
    Disable SCIM (Smart Common Input Method platform)

  --disable-ibus
    Disable iBUS (Intelligent Input Bus)

  --disable-fcitx
    Disable Fcitx
    If Fcitx is enabled, fcitx-gclient or Fcitx5GClient is necessary.

  --disable-canna
    Disable Canna

  --disable-wnn
    Disable Freewnn

  --disable-skk
    Disable SKK

  --enable-pty-helper
    Support gnome-pty-helper. (Note that it is impossible to enable
    gnome-pty-helper in BSD style pty system and that gnome-pty-helper must be
    placed at ${libexecdir}/mlterm directory.)

  --with-gtk=(2.0|3.0|4.0)
    Use specified version of GTK+
    (4.0 is available for mlconfig. libvte compatible library and mlterm-menu continue
     to need 2.0 or 3.0.)

  --enable-vt52
    Support vt52 emulation

  --disable-image
    Disable wall picture, icon picture, pseudo transparency and sixel graphics.

  --disable-otl
    Disable Open Type Layout with the use of harfbuzz or libotf.

  --enable-brlapi
    Enable brltty with the use of brlapi library. (Experimental)
    (see https://github.com/arakiken/mlterm/blob/master/doc/en/README.brltty)

  --enable-utmp-suid
    Install mlterm binary with root setuid or utmp setgid to access to utmp
    database directly.
    The default is disabled.

  --with-utmp=(utempter|sysv|login|bsd|none)
    Specify the way of reading from or writing to utmp database.

  --disable-daemon
    Disable daemon mode

  --disable-split
    Disable to split screen except on MacOSX/Cocoa and HaikuOS.

  --disable-zmodem
    Disable zmodem

  --disable-compact-truecolor
    Disable to reuse 240 palettes in displaying true color.
    Disabling this option supports true color completely, but sizeof(vt_char_t)
    is changed from 8 bytes to 12 bytes.

  --disable-shared
    Disable shared libraries

  --disable-static
    Disable static libraries

  If you want to minimize mlterm binary, execute configure with following options.
  --without-map-table --without-prop-table --disable-image --disable-dl-ctl \
  --disable-fribidi --disable-dl-type --disable-otl \
  --disable-ssh2 --disable-utmp --disable-dnd --disable-kbd --disable-uim \
  --disable-m17nlib --disable-ibus --disable-fcitx --disable-scim --disable-canna \
  --disable-wnn --disable-skk --disable-iiimf --disable-ind --with-type-engines=xcore \
  --disable-anti-alias --disable-fontconfig (--disable-shared)

What you need for compilation
-----------------------------

  All you need for basic compilation is libc and Xlib.  You don't need
  internationalization support of OS because mlterm has own i18n code.

  XFree86 4.0.2 or above and FreeType 2.0.2 or above are needed for
  anti-alias.

  gdk-pixbuf (Gtk+ 2.0.1 or higher) for background image.

  Gtk+ (2.x ?) for GUI configurator "mlconfig" and "mlterm-menu".

  Gtk+ (2.x ?) for libvte compatible library. (see gtk/INSTALL)

  Fribidi (0.9.0 - ?) for Bidi.

  libssh2 (https://www.libssh2.org) for ssh2 client and scp.
  (If you want to use camellia as common key cryptography or SSH agent forwarding,
   install alternative libssh2 of camellia-agentfwd branch at
   https://github.com/arakiken/libssh2/archive/camellia-agentfwd.zip)

Compilation of library based on mlterm
-----------------------

  vte compatible library

  $ make vte
  (see https://github.com/arakiken/mlterm/blob/master/gtk/README)

  libvterm compatible library

  $ make vterm
  (see https://github.com/arakiken/mlterm/blob/master/libvterm/README)

Supported platforms
-------------------

  Platforms tested by developers.

  * NetBSD 3.0.1 / X (x86)
  * NetBSD teokure 9.0 / X, frmebuffer (x86)
  * NetBSD 5.2 / framebuffer (hpcmips)
  * NetBSD 6.1 / framebuffer (xm6i 0.55)
  * OpenSolaris 2009.06 / X (CC=cc) (x86)
  * Solaris 11.1 / X (x86)
  * Arch Linux / X, framebuffer, wayland, SDL2 (x86)
  * CentOS 5 / X (x86)
  * FreeBSD 9.0 / framebuffer (x86)
  * FreeBSD 2.2.9 / X (x86)
  * FreeBSD(98) 4.11 / framebuffer (np21w ver.0.86 rev.42)
  * OpenBSD 5.3 / framebuffer (x86)
  * MS Windows 10 + Cygwin 3.1.7 / GDI, SDL2 (CC=gcc, CC="i686-pc-mingw32-gcc") (x86_64)
  * MS Windows 10 + MSYS2 3.1.7 / GDI (x86)
  * MacOSX 10.6.8 / X, Cocoa, SDL2 (x86)
  * MacOSX 10.8.5 / X, Cocoa, SDL2 (x86)
  * iOS Simulator 4.3 / CocoaTouch (x86)
  * iOS Simulator 7.1 / CocoaTouch (x86)
  * Android 4.0 (x86)
  * Android 9.0 (x86)
  * Java 1.8.0 / SWT (x86)
  * Haiku R1 Beta3 / Interface Kit, SDL2 (x86)

  See README.fb, README.win32, README.android, README.cocoa, README.cocoatouch,
  README.console, README.wayland, README.sdl2 and README.beos in
  https://github.com/arakiken/mlterm/blob/master/doc/en and
  README in https://github.com/arakiken/mlterm/blob/master/java in detail.


USAGE
=====

  $ mlterm

  Read the manpage (and documentations in mlterm-x.x.x/doc/{ja,en}) for detail.
  See ~/.mlterm/msg.log on error.


CONTACT
=======

  Subscribe mlterm-dev-en ML
  (https://lists.sourceforge.net/lists/listinfo/mlterm-dev-en).


COPYRIGHT AND LICENSE
=====================

  Modified BSD-style license.  See LICENCE file for detail.

