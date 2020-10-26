# Build OpenThread Commissioner App

This documents describes instructions to build the OT Commissioner (Android) App.

## Prerequisites

- macOS or Linux computer.
- Install latest Android Studio from [here](https://developer.android.com/studio).

## Build shared library

The Commissioner Android App is built atop of the native OT Commissioner library. There is a `build-commissioner-libs.sh` script in current directory to help building the required libraries:

```shell
ANDROID_ABI=arm64-v8a ANDROID_NDK_HOME=$HOME/Library/Android/sdk/ndk/21.3.6528147 ./build-commissioner-libs.sh
```

This script creates a build directory (`.build-${ANDROID_ABI}`) in current directory and copies generated libraries into target sub-folders in `openthread_commissioner`.

_Note: you need to set env `ANDROID_ABI` to the ABI of your phone. This value can be got with command `adb shell getprop ro.product.cpu.abi` after connecting the phone to your computer._

## Build the App with Android Studio

Connect the phone to your computer and we can now open the `openthread_commissioner` directory with Android Studio. Click the `Gradle Sync` and `Run` buttons to run the Commissioner App on your phone.
