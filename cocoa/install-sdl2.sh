#!/bin/sh

if [ $# != 1 ]; then
	echo "Usage: install-sdl2.sh [installed prefix]"
	exit
fi

if [ ! -d $HOME/mlterm-sdl2.app ]; then
	echo "Copy top_srcdir/cocoa/mlterm.app to $HOME/mlterm-sdl2"
	exit
fi

prefix="${1}"

cd $HOME/mlterm-sdl2.app
mkdir -p mlterm
cp -f $prefix/etc/mlterm/* mlterm/
rm mlterm/*aafont
rm mlterm/font-fb
rm mlterm/[tv]font

mkdir -p $HOME/mlterm-sdl2.app/Contents/MacOS
cd $HOME/mlterm-sdl2.app/Contents/MacOS

libs="lib/libpobl.[0-9]*.dylib lib/libmef.[0-9]*.dylib lib/libmlterm_core.dylib lib/libmlterm_coreotl.dylib lib/mlterm/libctl_bidi.so lib/mlterm/libctl_iscii.so lib/mlterm/libind_bengali.so lib/mlterm/libind_hindi.so lib/mlterm/libotl.so lib/mlterm/libim-skk-sdl2.so lib/mlterm/libim-kbd-sdl2.so lib/mlterm/libzmodem.so"

mkdir -p lib/mef
mkdir -p lib/mlterm
for file in $libs lib/mef/lib*.so ; do
	cp -f $prefix/$file `dirname $file`/
done

for file in bin/mlterm-sdl2 libexec/mlterm/mlconfig ; do
	cp -f $prefix/$file .
done

for file in $libs mlterm-sdl2 mlconfig ; do
	if [ -f $file ]; then
		echo "Update $file"
		for lib in $libs ; do
			install_name_tool -change $prefix/$lib @executable_path/$lib $file
		done
	fi
done

if [ -f lib/mlterm/libotl.so ]; then
	libhb=`otool -L lib/mlterm/libotl.so |sed -n 's/[^/]*\(.*harfbuzz.*dylib\).*/\1/p'`
	if [ "$libhb" != "" ]; then
		install_name_tool -change $libhb \
			@executable_path/lib/gtk/libharfbuzz.0.dylib lib/mlterm/libotl.so
	fi

	libotf=`otool -L lib/mlterm/libotl.so |sed -n 's/[^/]*\(.*otf.*dylib\).*/\1/p'`
	if [ "$libotf" != "" ]; then
		install_name_tool -change $libotf \
			@executable_path/lib/gtk/libotf.0.dylib lib/mlterm/libotl.so
	fi
fi

mv mlterm-sdl2 mlterm
