#!/bin/sh

if [ $# != 1 ]; then
	echo "Usage: install-ios.sh [installed prefix]"
	exit
fi

if [ ! -d $HOME/mlterm-ios.app ]; then
	echo "Copy top_srcdir/cocoa/mlterm-ios.app to $HOME/"
	exit
fi

prefix="${1}"

cd $HOME/mlterm-ios.app
mkdir -p etc/mlterm
cp -f $prefix/etc/mlterm/* etc/mlterm/
rm mlterm/*aafont
rm mlterm/font-fb
rm mlterm/[tv]font
cp $prefix/bin/mlterm .
