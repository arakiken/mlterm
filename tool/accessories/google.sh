#!/bin/sh

if [ ${#} -ne 1 ]; then
	echo "Usage: google.sh [keyword]"
	exit  1
fi

if [ -e ~/.mlterm/socket ]; then
	cmd="mlclient"
else
	cmd="mlterm"
fi

$cmd -T google --initstr "													\n${1}\n	\n" -e w3m http://www.google.com
