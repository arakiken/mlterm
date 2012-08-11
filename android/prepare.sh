#!/bin/sh

if [ $# != 1 ]; then
	echo "Usage: prepare.sh [android project path]"
	exit 1
fi

PROJECT_PATH=$1

echo "Prepare to build for android. (project: ${PROJECT_PATH})"
echo "Press any key to continue."
read

mkdir -p ${PROJECT_PATH}/jni
cp Android.mk ${PROJECT_PATH}/jni/

mkdir -p ${PROJECT_PATH}/jni/kiklib/kiklib
cp kik_config.h ${PROJECT_PATH}/jni/kiklib/kiklib/
cp ../kiklib/src/*.[ch] ${PROJECT_PATH}/jni/kiklib/kiklib/

mkdir -p ${PROJECT_PATH}/jni/mkf/mkf
cp ../mkf/lib/*.[ch] ${PROJECT_PATH}/jni/mkf/mkf/
cp -R ../mkf/lib/table ${PROJECT_PATH}/jni/mkf/mkf/
cp -R ../mkf/libtbl ${PROJECT_PATH}/jni/mkf/

mkdir -p ${PROJECT_PATH}/jni/mlterm
cp ../mlterm/*.[ch] ${PROJECT_PATH}/jni/mlterm/

mkdir -p ${PROJECT_PATH}/jni/java
cp ../java/MLTermPty.c ${PROJECT_PATH}/jni/java/
(cd ../java ; javah -jni mlterm.MLTermPty ; mv mlterm_MLTermPty.h ${PROJECT_PATH}/jni/java/)

mkdir -p ${PROJECT_PATH}/jni/main
cp ../main/version.h.in ${PROJECT_PATH}/jni/main/version.h

mkdir -p ${PROJECT_PATH}/src/mlterm
for file in MLTermPty.java MLTermPtyListener.java RedrawRegion.java Style.java ; do
	cp ../java/mlterm/$file ${PROJECT_PATH}/src/mlterm/
done

echo "done."
