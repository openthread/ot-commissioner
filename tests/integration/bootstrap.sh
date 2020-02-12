#!/bin/bash
#
#  Copyright (c) 2019, The OpenThread Authors.
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

[ -z ${TEST_ROOT_DIR} ] && . $(dirname $0)/common.sh

setup_otbr() {
    set -e
    git clone ${OTBR_REPO} ${OTBR} --branch ${OTBR_BRANCH}

    cd ${OTBR}

    ./script/bootstrap
    ./script/setup

    ## Stop otbr-agent and wpantund
    sudo service otbr-agent stop
    sudo service wpantund stop

    cd -
}

setup_openthread() {
    set -e
    git clone ${OPENTHREAD_REPO} ${OPENTHREAD} --branch ${OPENTHREAD_BRANCH}

    cd ${OPENTHREAD}

    git clean -xfd
    ./bootstrap

    make -f examples/Makefile-posix \
        COAP=1 \
        COAPS=1 \
        ECDSA=1 \
        BORDER_ROUTER=1 \
        SERVICE=1 \
        DHCP6_CLIENT=1 \
        DHCP6_SERVER=1 \
        JOINER=1 \
        COMMISSIONER=1 \
        MAC_FILTER=1 \
        REFERENCE_DEVICE=1 \
        THREAD_VERSION=1.2 \
        CSL_RECEIVER=1 \
        CSL_TRANSMITTER=1 \
        LINK_PROBE=1 \
        DUA=1 \
        MLR=1 \
        BBR=1 \
        MTD=0 \
        BORDER_AGENT=1 \
        UDP_FORWARD=1 \
        DEBUG=1

    cp output/x86_64-unknown-linux-gnu/bin/ot-cli-ftd ${NON_CCM_CLI}
    cp output/x86_64-unknown-linux-gnu/bin/ot-ncp-ftd ${NON_CCM_NCP}

    executable_or_die "${NON_CCM_CLI}"
    executable_or_die "${NON_CCM_NCP}"

    cd -
}

setup_commissioner() {
    set -e
    pip install --user -r ../../tools/commissioner_thci/requirements.txt
}

main() {
    set -e
    mkdir -p ${RUNTIME_DIR}

    if [ ! -d ${OTBR} ]; then
        setup_otbr
    fi

    if [ ! -d ${OPENTHREAD} ]; then
        setup_openthread
    fi

    setup_commissioner
}

main
