#!/bin/sh

if [ $# -ne 1 ]; then
	echo "Usage: build-iphonesim.sh [install prefix]"
	exit 0
fi

dir=`dirname $0`

#export CFLAGS="-g -fembed-bitcode-marker"
# Without -fobjc-weak, compiling error happens at "@synthesize inputDelegate".
export CFLAGS="-g -fno-objc-arc -fobjc-weak"

PKG_CONFIG_PATH=$HOME/libressl/lib/pkgconfig:$HOME/libssh2/lib/pkgconfig $dir/../configure --prefix=$1 --with-gui=quartz --host=x86_64-apple-darwin --disable-shared --disable-fribidi --disable-otl --without-tools --enable-debug --with-ios-sdk=iphonesimulator12.1 --with-ios-arch=x86_64
