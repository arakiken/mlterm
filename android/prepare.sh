#!/bin/sh

if [ $# != 1 ]; then
	echo "Usage: prepare.sh [android project path]"
	echo "(prepare.sh ~/work/mlterm-x.x.x/android => setup at ~/work/mlterm-x.x.x/android)"
	echo "(prepare.sh . => setup at the current directory)"
	exit 1
fi

PROJECT_PATH=$1

echo "Prepare to build for android. (project: ${PROJECT_PATH})"
echo "Press enter key to continue."
read

mkdir -p ${PROJECT_PATH}
cp build.sh ${PROJECT_PATH}/
chmod 755 ${PROJECT_PATH}/build.sh
cp *.xml ${PROJECT_PATH}/

mkdir -p ${PROJECT_PATH}/jni/baselib/pobl ${PROJECT_PATH}/jni/baselib/src
cp ../baselib/src/*.[ch] ${PROJECT_PATH}/jni/baselib/src/
cp jni/bl_*.h ${PROJECT_PATH}/jni/baselib/src/
cp ${PROJECT_PATH}/jni/baselib/src/*.h ${PROJECT_PATH}/jni/baselib/pobl/

mkdir -p ${PROJECT_PATH}/jni/encodefilter/mef
cp -R ../encodefilter/src ../encodefilter/module ${PROJECT_PATH}/jni/encodefilter
cp ${PROJECT_PATH}/jni/encodefilter/src/*.h ${PROJECT_PATH}/jni/encodefilter/mef/

cp -R ../libind ${PROJECT_PATH}/jni/

cp -R ../vtemu ${PROJECT_PATH}/jni/

mkdir -p ${PROJECT_PATH}/jni/uitoolkit/fb
mkdir -p ${PROJECT_PATH}/jni/uitoolkit/libotl
cp ../uitoolkit/*.[ch] ${PROJECT_PATH}/jni/uitoolkit
cp ../uitoolkit/fb/*.[ch] ${PROJECT_PATH}/jni/uitoolkit/fb
cp ../uitoolkit/libotl/*.[ch] ${PROJECT_PATH}/jni/uitoolkit/libotl

mkdir -p ${PROJECT_PATH}/jni/main
cp ../main/*.[ch] ${PROJECT_PATH}/jni/main/

mkdir -p ${PROJECT_PATH}/jni/common
cp ../common/c_imagelib.c ${PROJECT_PATH}/jni/common/
cp ../common/c_sixel.c ${PROJECT_PATH}/jni/common/
cp ../common/c_animgif.c ${PROJECT_PATH}/jni/common/

cp jni/Android.mk ${PROJECT_PATH}/jni/
cp jni/ui_event_source.c ${PROJECT_PATH}/jni/uitoolkit/
cp jni/ui.h jni/ui_display.[ch] jni/ui_connect_dialog.c ${PROJECT_PATH}/jni/uitoolkit/fb/
cp jni/main.c jni/version.h ${PROJECT_PATH}/jni/main/

mkdir -p ${PROJECT_PATH}/src/mlterm/native_activity
cp src/mlterm/native_activity/*.java ${PROJECT_PATH}/src/mlterm/native_activity/

cp -R res ${PROJECT_PATH}/

echo "done."
