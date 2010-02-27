#!/bin/sh

if [ ${#} -ne 1 ]; then
	echo "Usage: google.sh [keyword]"
	exit  1
fi

if [ -e /tmp/.mlterm-* ]; then
	cmd="mlclient"
else
	cmd="mlterm"
fi

mlclient -T google --initstr "													\n${1}\n	\n" -e w3m http://www.google.com
