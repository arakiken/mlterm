#!/bin/sh

reset_state()
{
	printf "\x1b]5379;search_$1=\x07"
	stty echo
	exit 0
}

if test ${#} = 0 ; then
	echo "Reset searching position."
	printf "\x1b]5379;search_$1=\x07"
	exit  0
elif test ${#} = 1 ; then
	pat=$1
	dir="prev"
elif test "$1" = "-h" -o ${#} != 2 ; then
	echo "Usage: mlsearch (prev|next) [pattern]"
	exit  0
else
	pat=$2
	dir=$1
fi

echo "Press Enter key to continue searching. Press ^C to exit."

trap "reset_state $dir" 2
stty -echo 
printf "\x1b]5379;search_$dir=$pat\x07"
read input
while  true
do printf "\x1b]5379;search_$dir=$pat\x07" ; read input
done

reset_state $dir
