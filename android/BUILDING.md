# Build OpenThread Commissioner App

This document describes how to build the OT Commissioner Android App.

## Prerequisites

- macOS or Linux computer.
- Install latest Android Studio from [here](https://developer.android.com/studio).

## Build shared library

The Commissioner Android App is built on top of the native OT Commissioner library. Run the `build-commissioner-libs.sh` script in this directory to build the required libraries:

```shell
ANDROID_ABI=arm64-v8a ANDROID_NDK_HOME=$HOME/Library/Android/sdk/ndk/21.3.6528147 ./build-commissioner-libs.sh
```

This script creates a build directory (`.build-${ANDROID_ABI}`) in the current directory and copies generated libraries into target sub-folders in `openthread_commissioner`.

> Note: you need to set env `ANDROID_ABI` to the ABI of your phone. This value can be got with command `adb shell getprop ro.product.cpu.abi` after connecting the phone to your computer.

## Build the App with Android Studio

Connect the phone to your computer and open the `openthread_commissioner` directory with Android Studio. Click the **Gradle Sync** and **Run** buttons to run the Commissioner App on your phone.
