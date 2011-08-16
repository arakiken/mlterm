#!/bin/sh

if test ${#} != 2 ; then
	echo "Usage: mlscp.sh [src path] [dst path]"
	echo " <path> = (remote:|local:)/dir/file"
	exit  0
else
	src=$1
	dst=$2
fi

printf "\x1b]5379;scp $src $dst\x07"
