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
read IN

mkdir -p ${PROJECT_PATH}/app/src/main

(cd ${PROJECT_PATH}; rm build.gradle settings.gradle app/build.gradle; gradle init)
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

GRADLE_VER=`gradle --version|sed -n 's/Gradle \([0-9.]*\)/\1/p'`
cat << END > ${PROJECT_PATH}/build.gradle
buildscript {
    repositories {
        jcenter()
        //google()
        maven {
            url 'https://maven.google.com/'
            name 'Google'
        }
    }

    dependencies {
        classpath 'com.android.tools.build:gradle:$GRADLE_VER'
    }
}

allprojects {
   repositories {
       jcenter()
       //google()
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
