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

LOCAL=/usr/share/wayland-protocols
REMOTE=https://gitlab.freedesktop.org/wayland/wayland-protocols/-/raw/main

if test -f $LOCAL/stable/xdg-shell/xdg-shell.xml; then
	XML_FILE=$LOCAL/stable/xdg-shell/xdg-shell.xml
else
	curl -O $REMOTE/stable/xdg-shell/xdg-shell.xml
	XML_FILE=xdg-shell.xml
fi
wayland-scanner $CODE $XML_FILE xdg-shell-client-protocol.c
wayland-scanner client-header $XML_FILE xdg-shell-client-protocol.h

if test -f $LOCAL/unstable/xdg-shell/xdg-shell-unstable-v6.xml; then
	XML_FILE=$LOCAL/unstable/xdg-shell/xdg-shell-unstable-v6.xml
else
	curl -O $REMOTE/unstable/xdg-shell/xdg-shell-unstable-v6.xml
	XML_FILE=xdg-shell-unstable-v6.xml
fi
wayland-scanner $CODE $XML_FILE xdg-shell-unstable-v6-client-protocol.c
wayland-scanner client-header $XML_FILE xdg-shell-unstable-v6-client-protocol.h

if test -f $LOCAL/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml; then
	XML_FILE=$LOCAL/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
else
	curl -O $REMOTE/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
	XML_FILE=xdg-decoration-unstable-v1.xml
fi
wayland-scanner $CODE $XML_FILE xdg-decoration-unstable-v1-client-protocol.c
wayland-scanner client-header $XML_FILE xdg-decoration-unstable-v1-client-protocol.h

if test -f $LOCAL/unstable/primary-selection/primary-selection-unstable-v1.xml; then
	XML_FILE=$LOCAL/unstable/primary-selection/primary-selection-unstable-v1.xml
else
	curl -O $REMOTE/unstable/primary-selection/primary-selection-unstable-v1.xml
	XML_FILE=primary-selection-unstable-v1.xml
fi
wayland-scanner $CODE $XML_FILE primary-selection-unstable-v1-client-protocol.c
wayland-scanner client-header $XML_FILE primary-selection-unstable-v1-client-protocol.h

if test -f $LOCAL/unstable/text-input/text-input-unstable-v3.xml; then
	XML_FILE=$LOCAL/unstable/text-input/text-input-unstable-v3.xml
else
	curl -O $REMOTE/unstable/text-input/text-input-unstable-v3.xml
	XML_FILE=text-input-unstable-v3.xml
fi
wayland-scanner $CODE $XML_FILE text-input-unstable-v3-client-protocol.c
wayland-scanner client-header $XML_FILE text-input-unstable-v3-client-protocol.h
