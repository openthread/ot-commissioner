#!/bin/bash
#
#  Copyright (c) 2020, The OpenThread Commissioner Authors.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. Neither the name of the copyright holder nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#

readonly CUR_DIR=$(cd "$(dirname "$0")" && pwd)

set -e

if [ -z "${ANDROID_ABI}" ]; then
    echo "ANDROID_ABI not set! Candidates: armeabi-v7a, arm64-v8a, x86 and x86_64"
    exit 1
fi

if [ -z "${ANDROID_NDK_HOME}" ]; then
    echo "ANDROID_NDK_HOME not set! Please set it to your Android NDK location!"
    exit 1
fi

cd "${CUR_DIR}"

readonly BUILD_DIR=".build-$ANDROID_ABI"

mkdir -p "$BUILD_DIR" && cd "$BUILD_DIR"
cmake -GNinja \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME"/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI="$ANDROID_ABI" \
    -DANDROID_ARM_NEON=ON \
    -DANDROID_NATIVE_API_LEVEL=21 \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_CXX_STANDARD=11 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DOT_COMM_ANDROID=ON \
    -DOT_COMM_JAVA_BINDING=ON \
    -DOT_COMM_APP=OFF \
    -DOT_COMM_TEST=OFF \
    -DOT_COMM_CCM=OFF \
    ../..

ninja commissioner-java
cd ../

rm -rf "$BUILD_DIR"/libs && mkdir -p "$BUILD_DIR"/libs

## Check JNI contains any raw swig wrapper files

if find "$BUILD_DIR"/src/java/io/openthread/commissioner -name "SWIGTYPE_*" -print -quit | grep -q .; then
  echo "***********  please check SWIG file \"commissioner.i\"  ***********"
  echo "Failed to create JAR library due to raw swig wrapper files under $BUILD_DIR/src/java/io/openthread/commissioner"
  exit -1
fi

## Create JAR library
javac -source 8 -target 8 "$BUILD_DIR"/src/java/io/openthread/commissioner/*.java

cd "$BUILD_DIR"/src/java
find ./io/openthread/commissioner -name "*.class" | xargs jar cvf ../../libs/libotcommissioner.jar
cd ../../../

## Copy shared native libraries
cp "$BUILD_DIR"/src/java/libcommissioner-java.so "$BUILD_DIR"/libs

mkdir -p openthread_commissioner/app/libs
mkdir -p openthread_commissioner/app/src/main/jniLibs/"${ANDROID_ABI}"
cp "$BUILD_DIR"/libs/libotcommissioner.jar openthread_commissioner/app/libs
cp "$BUILD_DIR"/libs/*.so openthread_commissioner/app/src/main/jniLibs/"${ANDROID_ABI}"
