#!/bin/sh

if [ -f ~/.config/mlterm/main ]; then
	FILE=~/.config/mlterm/main
elif [ -f ~/.mlterm/main ]; then
	FILE=~/.mlterm/main
else
	echo "~/.mlterm/main is not found"
	exit 1
fi
cat $FILE | sed 's/\([^ =]*\)*[ =]*\(.*\)/\x1b]5379;\1=\2\x07/'
