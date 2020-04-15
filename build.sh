#!/bin/sh

rm -rf _build
mkdir _build
cd _build

conan install .. --build missing
cmake .. -DOT_COMM_USE_VENDORED_LIBS=OFF
cmake --build . -j 
