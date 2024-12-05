#!/bin/bash
#
#  Copyright (c) 2019, The OpenThread Commissioner Authors.
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

set -e

# Bootstrap with CCM dependencies.
readonly WITH_CCM="${WITH_CCM:-1}"
readonly MIN_CMAKE_VERSION="3.20.1"

## Match the version to see if current version is greater than or euqal to required version.
## Args: $1 current version
##       $2 required version
match_version() {
    local current_version=$1
    local required_version=$2
    local min_version

    min_version="$(printf '%s\n' "$required_version" "$current_version" | sort -V | head -n1)"
    if [ "${min_version}" = "${required_version}" ]; then
        return 0
    else
        return 1
    fi
}

## Args: $1 CMake Version. for example, "3.16.1". Reference to
##       https://cmake.org/files/ for all available versions.
install_cmake() {
    local version=$1
    local short_version=${version%.*}

    if [ ! -d cmake-${version} ]; then
        wget "https://cmake.org/files/v${short_version}/cmake-${version}.tar.gz"
        tar -xvzf cmake-${version}.tar.gz
    fi

    local cpu_num=$(grep -c ^processor /proc/cpuinfo)
    if [ ${cpu_num} -gt 1 ]; then
        concurrency=$((cpu_num/2))
    else
        concurrency=1
    fi

    cd cmake-${version}
    ./bootstrap --prefix=/usr --parallel=${concurrency} -- -DCMAKE_BUILD_TYPE:STRING=Release
    make -j${concurrency}
    sudo make install
    cd -

    rm -rf cmake-${version}.tar.gz cmake-${version}
}

## Install packages
if [ "$(uname)" = "Linux" ]; then
    echo "OS is Linux"

    ## FIXME(wgtdkp): this is only apt install
    ## Install packages
    sudo apt-get update
    sudo apt-get install -y \
                         build-essential \
                         libreadline-dev \
                         cmake \
                         ninja-build \
                         swig \
                         lcov \
                         jsonlint

    sudo apt-get --no-install-recommends install -y clang-format-14 clang-tidy-14 || echo 'WARNING: could not install clang-format-14 and clang-tidy-14, which is useful if you plan to contribute C/C++ code to the OpenThread project.'
    python3 -m pip install yapf==0.29.0 || echo 'WARNING: could not install yapf, which is useful if you plan to contribute python code to the OpenThread project.'

    ## Install newest CMake
    match_version "$(cmake --version | grep -E -o '[0-9].*')" "${MIN_CMAKE_VERSION}" || {
        install_cmake "3.16.1"
        cmake --version
    }
    match_version "$(cmake --version | grep -E -o '[0-9].*')" "${MIN_CMAKE_VERSION}" || {
        echo "error: cmake version($(cmake --version)) < ${MIN_CMAKE_VERSION}."
        echo "Did you forget to add '/usr/bin' to beginning of your PATH?"
        exit 1
    }
elif [ "$(uname)" = "Darwin" ]; then
    echo "OS is Darwin"

    ## Install packages
    brew update
    brew install coreutils \
                 readline \
                 cmake \
                 ninja \
                 swig  \
                 lcov && true

    brew install llvm@14 && \
    sudo ln -s "$(brew --prefix llvm@14)/bin/clang-format" /usr/local/bin/clang-format-14 && \
    sudo ln -s "$(brew --prefix llvm@14)/bin/clang-tidy" /usr/local/bin/clang-tidy-14 && \
    sudo ln -s "$(brew --prefix llvm@14)/bin/clang-apply-replacements" /usr/local/bin/clang-apply-replacements-14 && \
    sudo ln -s "$(brew --prefix llvm@14)/bin/run-clang-tidy" /usr/local/bin/run-clang-tidy-14 || \
    echo 'WARNING: could not install clang-format-14, which is useful if you plan to contribute C/C++ code to the OpenThread project.'

    ## Install latest cmake
    match_version "$(cmake --version | grep -E -o '[0-9].*')" "${MIN_CMAKE_VERSION}" || {
        brew unlink cmake
        brew install cmake --HEAD
    }
else
    echo "platform $(uname) is not fully supported"
    exit 1
fi

readonly CUR_DIR="$(dirname "$(realpath "$0")")"

cd "${CUR_DIR}/.."
if [ "${WITH_CCM}" = "1" ]; then
    git submodule update --init --recursive --depth=1 --progress
else
    git submodule update --init --recursive --depth=1 --progress -- third_party/mbedtls/repo
    git submodule update --init --recursive --depth=1 --progress -- third_party/json/repo
    git submodule update --init --recursive --depth=1 --progress -- third_party/libevent/repo
    git submodule update --init --recursive --depth=1 --progress -- third_party/mdns/repo
    git submodule update --init --recursive --depth=1 --progress -- third_party/fmtlib/repo
fi
cd -
