#!/bin/sh

# for mlterm 3.8.2 or later

get_challenge ()
{
	if [ "$challenge" = "" ]; then
		stty -echo
		/usr/bin/printf "\e]5380;challenge\a"
		read challenge
		challenge=`echo $challenge|sed 's/^#challenge=//'`
		stty echo
	fi
}

get_dev_list ()
{
	stty -echo
	/usr/bin/printf "\e]5380;%s;pty_list\a" $challenge
	read dev_list
	dev_list=`echo $dev_list|sed 's/^#pty_list=//'|tr ';' ' '`
	stty echo
}

get_dev_num ()
{
	get_challenge
	get_dev_list
	dev_num=0
	for dev in $dev_list; do
		dev_num=`expr $dev_num + 1`
	done
}

set_config ()
{
	get_challenge
	get_dev_list

	count=0
	for dev in $dev_list; do
		dev=`echo $dev|sed 's/\([^:]*\):.*/\1/'`
		if [ $count -eq $1 ]; then
			echo "$dev: $2"
			/usr/bin/printf "\e]5379;$dev:$2\a"
		fi
		count=`expr $count + 1`;
	done
	stty echo
}

get_dev_num

# maximize window
/usr/bin/printf "\x1b[9;1t"

mlcc exec hsplit_screen 101
mlcc exec next_screen
mlcc exec vsplit_screen 30%

set_config $dev_num "encoding=utf8"
set_config `expr $dev_num - 1` "input_method=none"

emacsclient -t
#$HOME/.sayaka/sayaka.sh s &
