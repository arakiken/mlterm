#!/bin/bash

#
# for Linux
#
# Usage: ~/.mlterm/key
# Button3 = gedit.sh
#

if [ -f $1 ];then
	echo "gedit.sh: Open $1 by gedit." >> ~/.mlterm/msg.log
	gedit $1
	exit
fi

SH_PID=`ps --ppid $PPID --no-heading|grep pts/|awk '{ print $1 }'`
for pid in $SH_PID ; do
	DIR=`readlink /proc/$pid/cwd`
	if [ -f $DIR/$1 ]; then
		echo "gedit.sh: Open $DIR/$1 by gedit." >> ~/.mlterm/msg.log
		gedit $DIR/$1
		break
	fi
done

zenity --error --text=\\"Failed to open $1.\\"
