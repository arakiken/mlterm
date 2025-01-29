#!/bin/sh

if [ $# -lt 1 -o $# -gt 4 ]; then
	echo "Usage: prepare.sh [android project path]"
	echo "Usage: prepare.sh [android project path] [plugin version]"
	echo "       prepare.sh [android project path] [mosh src path] [openssl src path]"
	echo "       prepare.sh [android project path] [mosh src path] [openssl src path] [plugin version]"
	echo "(prepare.sh ~/work/mlterm-x.x.x/android => setup at ~/work/mlterm-x.x.x/android)"
	echo "(prepare.sh . => setup at the current directory)"
	exit 1
fi

PROJECT_PATH=$1

echo "Prepare to build for android. (project: ${PROJECT_PATH})"
echo "Press enter key to continue."
read IN

if [ $# = 3 -o $# = 4 ]; then
	MOSH_SRC_PATH=$2
	OPENSSL_SRC_PATH=$3

	if [ ! -d ${MOSH_SRC_PATH} ]; then
		echo "${MOSH_SRC_PATH} doesn't exist."
		exit 1
	elif [ ! -d ${OPENSSL_SRC_PATH}/include/openssl ]; then
		echo "${OPENSSL_SRC_PATH}/include/openssl doesn't exist."
		exit 1
	fi

	mkdir -p ${PROJECT_PATH}/app/src/main/jni/vtemu/libptymosh

	CRYPTO_FILES="ae.h base64.cc base64.h byteorder.h crypto.cc crypto.h prng.h"
	STATESYNC_FILES="completeterminal.cc completeterminal.h user.cc user.h"
	NETWORK_FILES="compressor.cc compressor.h network.cc network.h networktransport.h \
			networktransport-impl.h transportfragment.cc transportfragment.h \
			transportsender.h transportsender-impl.h transportstate.h"
	UTIL_FILES="dos_assert.h fatal_assert.h shared.h timestamp.cc timestamp.h"
	PROTOBUFS_FILES="hostinput.pb.cc hostinput.pb.h transportinstruction.pb.cc \
			transportinstruction.pb.h userinput.pb.cc userinput.pb.h"
	TERMINAL_FILES="parseraction.cc parseraction.h parser.cc parser.h parserstate.cc \
			parserstatefamily.h parserstate.h parsertransition.h terminal.cc \
			terminaldispatcher.cc terminaldispatcher.h terminaldisplay.cc terminaldisplay.h \
			terminalframebuffer.cc terminalframebuffer.h terminalfunctions.cc \
			terminal.h terminaluserinput.cc terminaluserinput.h"
	FRONTEND_FILES="terminaloverlay.cc terminaloverlay.h"

	if [ -f ${MOSH_SRC_PATH}/config.h ]; then
		# 1.3.2 or before
		cp ${MOSH_SRC_PATH}/config.h ${PROJECT_PATH}/app/src/main/jni/vtemu/libptymosh
		CRYPTO_FILES="$CRYPTO_FILES ocb.cc"
	else
		# 1.4.0 or later
		cp ${MOSH_SRC_PATH}/src/include/config.h ${PROJECT_PATH}/app/src/main/jni/vtemu/libptymosh
		CRYPTO_FILES="$CRYPTO_FILES ocb_internal.cc"
	fi
	(cd ${PROJECT_PATH}/app/src/main/jni/vtemu/libptymosh; cat config.h | sed 's/#define HAVE_TR1_MEMORY 1//' > config.h.2; mv config.h.2 config.h)

	for f in $CRYPTO_FILES; do
		cp ${MOSH_SRC_PATH}/src/crypto/$f ${PROJECT_PATH}/app/src/main/jni/vtemu/libptymosh
	done

	for f in $STATESYNC_FILES; do
		cp ${MOSH_SRC_PATH}/src/statesync/$f ${PROJECT_PATH}/app/src/main/jni/vtemu/libptymosh
	done

	for f in $NETWORK_FILES; do
		cp ${MOSH_SRC_PATH}/src/network/$f ${PROJECT_PATH}/app/src/main/jni/vtemu/libptymosh
	done

	for f in $UTIL_FILES; do
		cp ${MOSH_SRC_PATH}/src/util/$f ${PROJECT_PATH}/app/src/main/jni/vtemu/libptymosh
	done

	for f in $PROTOBUFS_FILES; do
		cp ${MOSH_SRC_PATH}/src/protobufs/$f ${PROJECT_PATH}/app/src/main/jni/vtemu/libptymosh
	done

	for f in $TERMINAL_FILES; do
		cp ${MOSH_SRC_PATH}/src/terminal/$f ${PROJECT_PATH}/app/src/main/jni/vtemu/libptymosh
	done

	for f in $FRONTEND_FILES; do
		cp ${MOSH_SRC_PATH}/src/frontend/$f ${PROJECT_PATH}/app/src/main/jni/vtemu/libptymosh
	done

	cp -r ${OPENSSL_SRC_PATH}/include/openssl ${PROJECT_PATH}/app/src/main/jni/vtemu/libptymosh
else
	mkdir -p ${PROJECT_PATH}/app/src/main
fi

(cd ${PROJECT_PATH}; rm build.gradle settings.gradle app/build.gradle; gradle init --overwrite --scan)
if [ $? != 0 ]; then
	echo "Install and setup gradle in advance."
	exit 1
fi

cp build-gradle.sh ${PROJECT_PATH}/
chmod 755 ${PROJECT_PATH}/build-gradle.sh
cp AndroidManifest.xml ${PROJECT_PATH}/app/src/main

mkdir -p ${PROJECT_PATH}/app/src/main/jni/baselib/pobl ${PROJECT_PATH}/app/src/main/jni/baselib/src
cp ../baselib/src/*.[ch] ${PROJECT_PATH}/app/src/main/jni/baselib/src/
cp jni/bl_*.h ${PROJECT_PATH}/app/src/main/jni/baselib/src/
cp ${PROJECT_PATH}/app/src/main/jni/baselib/src/*.h ${PROJECT_PATH}/app/src/main/jni/baselib/pobl/

mkdir -p ${PROJECT_PATH}/app/src/main/jni/encodefilter/mef
cp -R ../encodefilter/src ../encodefilter/module ${PROJECT_PATH}/app/src/main/jni/encodefilter
cp ${PROJECT_PATH}/app/src/main/jni/encodefilter/src/*.h ${PROJECT_PATH}/app/src/main/jni/encodefilter/mef/

cp -R ../libind ${PROJECT_PATH}/app/src/main/jni/

cp -R ../vtemu ${PROJECT_PATH}/app/src/main/jni/

mkdir -p ${PROJECT_PATH}/app/src/main/jni/uitoolkit/fb
mkdir -p ${PROJECT_PATH}/app/src/main/jni/uitoolkit/libotl
cp ../uitoolkit/*.[ch] ${PROJECT_PATH}/app/src/main/jni/uitoolkit
cp ../uitoolkit/fb/*.[ch] ${PROJECT_PATH}/app/src/main/jni/uitoolkit/fb
cp ../uitoolkit/libotl/*.[ch] ${PROJECT_PATH}/app/src/main/jni/uitoolkit/libotl

mkdir -p ${PROJECT_PATH}/app/src/main/jni/main
cp ../main/*.[ch] ${PROJECT_PATH}/app/src/main/jni/main/

mkdir -p ${PROJECT_PATH}/app/src/main/jni/common
cp ../common/c_imagelib.c ${PROJECT_PATH}/app/src/main/jni/common/
cp ../common/c_sixel.c ${PROJECT_PATH}/app/src/main/jni/common/
cp ../common/c_animgif.c ${PROJECT_PATH}/app/src/main/jni/common/

cp jni/Android.mk ${PROJECT_PATH}/app/src/main/jni/
cp jni/ui_event_source.c ${PROJECT_PATH}/app/src/main/jni/uitoolkit/
cp jni/ui.h jni/ui_display.[ch] jni/ui_connect_dialog.c ${PROJECT_PATH}/app/src/main/jni/uitoolkit/fb/
cp jni/main.c jni/version.h ${PROJECT_PATH}/app/src/main/jni/main/

mkdir -p ${PROJECT_PATH}/app/src/main/java/mlterm/native_activity
cp src/mlterm/native_activity/*.java ${PROJECT_PATH}/app/src/main/java/mlterm/native_activity/

cp -R res ${PROJECT_PATH}/app/src/main

if [ $# = 2 ]; then
	PLUGIN_VERSION=$2
elif [ $# = 4 ]; then
	PLUGIN_VERSION=$4
else
	# gradle version and plugin version don't necessarily match.
	# -> https://developer.android.com/build/releases/gradle-plugin?hl=ja#updating-gradle
	PLUGIN_VERSION=`gradle --version|sed -n 's/Gradle \([0-9.]*\)/\1/p'`
fi

cat << END > ${PROJECT_PATH}/build.gradle
buildscript {
    repositories {
        jcenter()
        google()
        maven {
            url 'https://maven.google.com/'
            name 'Google'
        }
    }

    dependencies {
        classpath 'com.android.tools.build:gradle:$PLUGIN_VERSION'
    }
}

allprojects {
   repositories {
       jcenter()
       google()
       maven {
           url 'https://maven.google.com/'
           name 'Google'
       }
   }
}
END

echo "include ':app'" > ${PROJECT_PATH}/settings.gradle

cp build.gradle ${PROJECT_PATH}/app

echo "done."
