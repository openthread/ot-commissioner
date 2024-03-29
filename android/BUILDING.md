# Build OpenThread Commissioner App

This document describes how to build the OT Commissioner Android App.

## Prerequisites

- macOS or Linux computer.
- Install latest Android Studio from [here](https://developer.android.com/studio).
- Install Android NDK by following https://developer.android.com/studio/projects/install-ndk#default-version.

## Bootstrap

```shell
../script/bootstrap.sh
```

## Build shared library

The Commissioner Android App is built on top of the native OT Commissioner library. Run the `build-commissioner-libs.sh` script in this directory to build the required libraries:

```shell
ANDROID_ABI=arm64-v8a ANDROID_NDK_HOME=$HOME/Android/Sdk/ndk/26.2.11394342 ./build-commissioner-libs.sh
```

> Note:
>
> 1.  You need to set `ANDROID_ABI` to the ABI of your phone. This value can be retrived using the command `adb shell getprop ro.product.cpu.abi` after connecting the phone to your computer.
> 2.  You need to set `ANDROID_NDK_HOME` to your Android NDK directory. This is typically `$HOME/Android/sdk/ndk/xx.x.xxxxx` (where `xx.x.xxxxx` is the NDK version) on Linux and Mac OS or `$HOME/Android/sdk/ndk-bundle` on older OS versions.

This script creates a build directory (`.build-${ANDROID_ABI}`) in the current directory and copies generated libraries into target sub-folders in `openthread_commissioner`.

## Build the App with Android Studio

Connect the phone to your computer and open the `openthread_commissioner` directory with Android Studio. Click the **Gradle Sync** and **Run** buttons to run the Commissioner App on your phone.
