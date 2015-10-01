#!/bin/sh

if [ $# != 1 ]; then
	echo "Usage: rename.sh [installed prefix]"
	exit
fi

if [ ! -d $HOME/mlterm.app ]; then
	echo "Copy top_srcdir/cocoa/mlterm.app to $HOME/"
	exit
fi

cd $HOME/mlterm.app/Contents/MacOS

prefix="${1}"
libs="lib/libkik.[0-9]*.dylib lib/libmkf.[0-9]*.dylib lib/libmlterm_core.dylib"

cp $prefix/bin/mlterm .
cp -R $prefix/lib .

for lib in $libs ; do
	for file in $libs mlterm ; do
		install_name_tool -change $prefix/$lib @executable_path/$lib $file
	done
done
