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
cp *.xml ${PROJECT_PATH}/

mkdir -p ${PROJECT_PATH}/jni/kiklib/kiklib ${PROJECT_PATH}/jni/kiklib/src
cp ../kiklib/src/*.[ch] ${PROJECT_PATH}/jni/kiklib/src/
cp jni/kik_*.h ${PROJECT_PATH}/jni/kiklib/src/
cp ${PROJECT_PATH}/jni/kiklib/src/*.h ${PROJECT_PATH}/jni/kiklib/kiklib/

mkdir -p ${PROJECT_PATH}/jni/mkf/mkf
cp -R ../mkf/lib ../mkf/libtbl ${PROJECT_PATH}/jni/mkf/
cp ${PROJECT_PATH}/jni/mkf/lib/*.h ${PROJECT_PATH}/jni/mkf/mkf/

cp -R ../mlterm ${PROJECT_PATH}/jni/

mkdir -p ${PROJECT_PATH}/jni/xwindow/fb
cp ../xwindow/*.[ch] ${PROJECT_PATH}/jni/xwindow
cp ../xwindow/fb/*.[ch] ${PROJECT_PATH}/jni/xwindow/fb

mkdir -p ${PROJECT_PATH}/jni/main
cp ../main/*.[ch] ${PROJECT_PATH}/jni/main/

mkdir -p ${PROJECT_PATH}/jni/common
cp ../common/c_imagelib.c ${PROJECT_PATH}/jni/common/

cp jni/Android.mk ${PROJECT_PATH}/jni/
cp jni/x_event_source.c ${PROJECT_PATH}/jni/xwindow/
cp jni/x.h jni/x_display.[ch] ${PROJECT_PATH}/jni/xwindow/fb/
cp jni/main.c jni/version.h ${PROJECT_PATH}/jni/main/

mkdir -p ${PROJECT_PATH}/src/mlterm/native_activity
cp src/mlterm/native_activity/MLActivity.java ${PROJECT_PATH}/src/mlterm/native_activity/

cp -R res ${PROJECT_PATH}/

echo "done."
