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

readonly MIN_SWIG_VERSION="4.0.0"
readonly MIN_SWIG_LINK="https://nchc.dl.sourceforge.net/project/swig/swig/swig-${MIN_SWIG_VERSION}/swig-${MIN_SWIG_VERSION}.tar.gz"

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

if [ "$(uname)" = "Linux" ]; then
    echo "OS is Linux"

    ## Install swig4
    match_version "$(swig -version | grep -m 1 -E -o '[0-9].*')" "${MIN_SWIG_VERSION}" || {
        sudo apt-get install libpcre3-dev

        wget "${MIN_SWIG_LINK}"
        tar -xzf "swig-${MIN_SWIG_VERSION}.tar.gz"
        cd "swig-${MIN_SWIG_VERSION}"
        ./configure --prefix=/usr
        make
        sudo make install
        swig -version
        cd ..
        rm -rf "swig-${MIN_SWIG_VERSION}"*
    }

elif [ "$(uname)" = "Darwin" ]; then
    echo "OS is Darwin"

    ## Install swig4
    match_version "$(swig -version | grep -m 1 -E -o '[0-9].*')" "${MIN_SWIG_VERSION}" || {
        brew unlink swig
        brew install swig@4
    }
else
    echo "platform $(uname) is not supported, please install SWIG4.0 manually by following the guide: \
          http://www.swig.org/Doc4.0/SWIGDocumentation.html#Preface_installation"
    exit 1
fi
