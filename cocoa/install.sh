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
libs="lib/libkik.[0-9]*.dylib lib/libmkf.[0-9]*.dylib lib/libmlterm_core.dylib lib/mlterm/lib*.so"

mkdir -p lib/mkf
mkdir -p lib/mlterm
for file in $libs lib/mkf/lib*.so ; do
	cp -f $prefix/$file `dirname $file`/
done

for file in bin/mlterm libexec/mlterm/mlconfig ; do
	cp -f $prefix/$file .
done

for file in $libs mlterm mlconfig ; do
	if [ -f $file ]; then
		echo "Update $file"
		for lib in $libs ; do
			install_name_tool -change $prefix/$lib @executable_path/$lib $file
		done
	fi
done
