# Build Minimum Thread Commissioner

## CMake Build

```shell
cd ~/ot-commissioner/example
mkdir -p build && cd build
cmake -GNinja -DBUILD_SHARED_LIBS=ON ..
ninja
```

A `mini_commissioner` binary will be generated in `build/`.

## Standalone build

Build and install the OT Commissioner library:

```c++
cd ~/ot-commissioner
mkdir -p build && cd build
cmake -GNinja -DBUILD_SHARED_LIBS=ON ..
sudo ninja install
```

Build the `mini_commissioner` app:

```c++
cd ~/ot-commissioner/example
clang++ -std=c++11 -Wall -g mini_commissioner.cpp -o mini-commissioner -lcommissioner -lcommissioner-common
```

The `mini-commissioner` binary will be generated in current directory.
