#!/bin/sh

which wayland-scanner >/dev/null
if test $? -ne 0; then
	echo "Not found: wayland-scanner"
	exit 1
fi

VERSION=`wayland-scanner --version 2>&1`
MAJOR=`echo $VERSION|sed 's/wayland-scanner \([0-9]*\).*/\1/'`
MINOR=`echo $VERSION|sed 's/wayland-scanner [0-9]*.\([0-9]*\).*/\1/'`

if test "$MAJOR" -gt 1 -o "$MINOR" -gt 14; then
	# 1.15.0 or later
	CODE="private-code"
else
	CODE="code"
fi

if test -f /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml; then
	XML_FILE=/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml
else
	curl https://cgit.freedesktop.org/wayland/wayland-protocols/plain/stable/xdg-shell/xdg-shell.xml > xdg-shell.xml
	XML_FILE=xdg-shell.xml
fi
wayland-scanner $CODE $XML_FILE xdg-shell-client-protocol.c
wayland-scanner client-header $XML_FILE xdg-shell-client-protocol.h

if test -f /usr/share/wayland-protocols/unstable/xdg-shell/xdg-shell-unstable-v6.xml; then
	XML_FILE=/usr/share/wayland-protocols/unstable/xdg-shell/xdg-shell-unstable-v6.xml 
else
	curl https://cgit.freedesktop.org/wayland/wayland-protocols/plain/unstable/xdg-shell/xdg-shell-unstable-v6.xml > xdg-shell-unstable-v6.xml
	XML_FILE=xdg-shell-unstable-v6.xml
fi
wayland-scanner $CODE $XML_FILE xdg-shell-unstable-v6-client-protocol.c
wayland-scanner client-header $XML_FILE xdg-shell-unstable-v6-client-protocol.h

if test -f /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml; then
	XML_FILE=/usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
else
	curl https://cgit.freedesktop.org/wayland/wayland-protocols/plain/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml > xdg-decoration-unstable-v1.xml
	XML_FILE=xdg-decoration-unstable-v1.xml
fi
wayland-scanner $CODE $XML_FILE xdg-decoration-unstable-v1-client-protocol.c
wayland-scanner client-header $XML_FILE xdg-decoration-unstable-v1-client-protocol.h
