#!/bin/sh

if [ ${#} -ne 1 ]; then
	echo "Usage: mltrachet.sh [log file]"
	exit  1
fi

file="${1}"

if [ ! -f "$file" ]; then
	echo "Not found $file"
	exit  1
fi

_trachet=`which trachet`
_printf=`which printf`

if [ -z "$_trachet" -o "$_printf" != "/usr/bin/printf" ]; then
	echo "Not found trachet/printf"
	exit  1
fi

# /usr/bin/ is specified to avoid to use built-in printf.
/usr/bin/printf "\e]5380;%s;pty_list\a" `cat $HOME/.mlterm/challenge`
read oldlist

/usr/bin/printf "\e]5379;mlclient -e cat\a"

/usr/bin/printf "\e]5380;%s;pty_list\a" `cat $HOME/.mlterm/challenge`
read newlist

for newdev in `echo $newlist | tr ';' ' ' | tr '=' ' '` ; do
	match=0
	for olddev in `echo $oldlist | tr ';' ' ' | tr '=' ' '` ; do
		if [ $newdev = $olddev ]; then
			match=1
			break
		fi
	done

	if [ $match = 0 ]; then
		newdev=`echo $newdev | tr ':1' ' ' | tr ':0' ' '`
		trachet -o $newdev "cat $file -"
		break
	fi
done
