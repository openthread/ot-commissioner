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

## This file bootstrap dependencies of integration tests.
##
## Accepted environment variables:
##   - OT_COMM_SKIP_BUILDING_OTBR  Control if skip building ot-br-posix from source,
##                                 This is useful when ot-br-posix is pre-installed.
##

[ -z "${TEST_ROOT_DIR}" ] && . "$(dirname "$0")"/common.sh

readonly SKIP_BUILDING_OTBR=${OT_COMM_SKIP_BUILDING_OTBR:=0}

setup_otbr() {
    set -e
    git clone "${OTBR_REPO}" "${OTBR}" --branch "${OTBR_BRANCH}" --depth=1

    cd "${OTBR}"

    ./script/bootstrap
    ./script/setup

    ## Stop otbr-agent
    sudo service otbr-agent stop

    cd -
}

setup_openthread() {
    set -e

    if [[ ! -d ${OPENTHREAD} ]]; then
        git clone "${OPENTHREAD_REPO}" "${OPENTHREAD}" --branch "${OPENTHREAD_BRANCH}" --depth=1
    fi

    cd "${OPENTHREAD}"

    git clean -xfd
    ./bootstrap

    local ot_build_dir=$(VIRTUAL_TIME=0 ./script/test clean build | grep 'Build files have been written to: ' | cut -d: -f2 | tr -d ' ')

    cp ${ot_build_dir}/examples/apps/ncp/ot-rcp "${NON_CCM_RCP}"
    cp ${ot_build_dir}/examples/apps/cli/ot-cli-ftd "${NON_CCM_CLI}"

    cmake -DOT_PLATFORM=posix -DOT_DAEMON=ON -DOT_COVERAGE=ON -S . -B build
    cmake --build build
    cp build/src/posix/ot-ctl "${OT_CTL}"

    cd -
}

setup_commissioner() {
    set -e
    pip install --user -r "${TEST_ROOT_DIR}"/../../tools/commissioner_thci/requirements.txt
}

main() {
    set -e
    mkdir -p "${RUNTIME_DIR}"

    if (( SKIP_BUILDING_OTBR == 0 )) && [ ! -d "${OTBR}" ]; then
        setup_otbr
    fi

    setup_openthread

    setup_commissioner
}

main
