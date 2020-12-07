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

[ -z "${TEST_ROOT_DIR}" ] && . "$(dirname "$0")"/common.sh

setup_openthread() {
    set -e

    if [[ ! -d ${OPENTHREAD} ]]; then
        git clone "${OPENTHREAD_REPO}" "${OPENTHREAD}" --branch "${OPENTHREAD_BRANCH}" --depth=1
    fi

    cd "${OPENTHREAD}"

    OT_CMAKE_NINJA_TARGET="package" OT_CMAKE_BUILD_DIR="${OPENTHREAD}/build/package/openthread-sim" "${OPENTHREAD}/script/cmake-build" simulation -DOT_COVERAGE=ON
    OT_CMAKE_NINJA_TARGET="package" OT_CMAKE_BUILD_DIR="${OPENTHREAD}/build/package/openthread-daemon" "${OPENTHREAD}/script/cmake-build" posix -DOT_DAEMON=ON -DOT_PLATFORM_NETIF=ON -DOT_PLATFORM_UDP=ON -DOT_COVERAGE=ON
    sudo dpkg -i "${OPENTHREAD}/build/package/openthread-sim"/openthread-simulation-*.deb
    sudo dpkg -i "${OPENTHREAD}/build/package/openthread-daemon"/openthread-daemon-*.deb

    rm "${OT_CTL}" "${OT_DAEMON}" "${NON_CCM_RCP}" "${NON_CCM_CLI}" || true

    ln -s "$(which ot-ctl)" "${OT_CTL}"
    ln -s "$(which ot-daemon)" "${OT_DAEMON}"
    ln -s "$(which ot-rcp)" "${NON_CCM_RCP}"
    ln -s "$(which ot-cli-ftd)" "${NON_CCM_CLI}"

    cd -
}

setup_commissioner() {
    set -e
    sudo apt-get install -y --no-install-recommends expect
    pip install --user -r "${TEST_ROOT_DIR}"/../../tools/commissioner_thci/requirements.txt
}

setup_border_agent_mdns_service() {
    sudo apt-get update
    sudo apt-get install -y --no-install-recommends dbus avahi-daemon

    ## Install the Border Agent service configure file to
    ## the default directory read by avahi-daemon.
    sudo cp "${CUR_DIR}/../etc/avahi/services/border-agent.service" /etc/avahi/services
}

main() {
    set -e
    mkdir -p "${RUNTIME_DIR}"

    setup_openthread
    setup_border_agent_mdns_service
    setup_commissioner
}

main
