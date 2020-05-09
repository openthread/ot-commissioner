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

## This file defines constants and common functions for test cases.

readonly CUR_DIR=$(dirname "$(realpath -s $0)")
readonly TEST_ROOT_DIR=${CUR_DIR}

readonly RUNTIME_DIR=/tmp/test-ot-commissioner

readonly OTBR_REPO=https://github.com/openthread/ot-br-posix
readonly OTBR_BRANCH=master
readonly OTBR=${RUNTIME_DIR}/ot-br-posix

readonly OPENTHREAD_REPO=https://github.com/openthread/openthread
readonly OPENTHREAD_BRANCH=master
readonly OPENTHREAD=${RUNTIME_DIR}/openthread

readonly NON_CCM_CLI=${RUNTIME_DIR}/ot-cli-ftd-non-ccm
readonly NON_CCM_NCP=${RUNTIME_DIR}/ot-ncp-ftd-non-ccm

readonly WPANTUND_CONF=/etc/wpantund.conf

readonly WPANTUND_LOG=${RUNTIME_DIR}/wpantund.log
readonly OTBR_LOG=${RUNTIME_DIR}/otbr.log

## '/usr/local' is the by default installing directory.
readonly COMMISSIONER_CLI=/usr/local/bin/commissioner-cli
readonly COMMISSIONER_DAEMON=/usr/local/bin/commissionerd.py
readonly COMMISSIONER_CTL=/usr/local/bin/commissioner_ctl.py
readonly COMMISSIONER_DAEMON_LOG=${RUNTIME_DIR}/commissioner-daemon.log
readonly COMMISSIONER_LOG=./commissioner.log

readonly NON_CCM_CONFIG=${TEST_ROOT_DIR}/../../src/app/etc/commissioner/non-ccm-config.json

readonly JOINER_NODE_ID=2
readonly JOINER_EUI64=0x18b4300000000002
readonly JOINER_CREDENTIAL=ABCDEF

## Thread network parameters
readonly NETWORK_NAME=openthread-test
readonly CHANNEL=19
readonly PANID=0xface
readonly XPANID=dead00beef00cafe
readonly MASTERKEY=00112233445566778899aabbccddeeff
readonly PSKC=3aa55f91ca47d1e4e71a08cb35e91591

die() {
  echo " *** ERROR: " "$@"
  exit 1
}

executable_or_die() {
  [ -x "$1" ] || die "Missing executable: $1"
}

## Start otbr agent.
## Args: $1: the NCP firmware.
##       $2: the backbone interface.
start_otbr() {
    set -e
    sudo killall -9 wpantund || true
    sudo killall -9 otbr-agent || true

    sleep 1
    pidof wpantund && die "killing wpantund failed"
    pidof otbr-agent && die "killing otbr-agent failed"

    local ncp=$1

    sudo chmod 777 ${WPANTUND_CONF}
    sudo echo "Config:NCP:SocketPath \"system:${ncp} 1\"" > ${WPANTUND_CONF}

    ## Run wpantund in different directory, because it creates
    ## intermediate sub-directory leading to permission issue when
    ## creating flash file of CLI nodes which are started without
    ## root privilege.
    mkdir -p wpantund-tmp && cd wpantund-tmp
    sudo wpantund -c ${WPANTUND_CONF} -d 7 > "${WPANTUND_LOG}" 2>&1 &
    cd -

    sleep 1

    sudo otbr-agent -I wpan0 -d 7 -v > "${OTBR_LOG}" 2>&1 &

    sleep 1
}

## Start commissioner daemon.
## Args: $1: configuration file path.
start_commissioner() {
    set -e
    if [ -n "$(pgrep -f "${COMMISSIONER_DAEMON}")" ]; then
        sudo kill -9 "$(pgrep -f "${COMMISSIONER_DAEMON}")"
    fi

    local config=$1

    echo "starting commissioner daemon: [ ${COMMISSIONER_DAEMON} --cli ${COMMISSIONER_CLI} ]"
    python -u "${COMMISSIONER_DAEMON}" --cli "${COMMISSIONER_CLI}" --timeout 200 > "${COMMISSIONER_DAEMON_LOG}" 2>&1 &
    sleep 1

    pgrep -f "${COMMISSIONER_DAEMON}"

    init_commissioner "${config}"
}

## Initialize commissioner daemon with given configuration file.
## Args: $1: configuration file path.
init_commissioner() {
    set -e

    local config=$1

    echo "initializating commissioner daemon with configuration file: ${config}"
    ${COMMISSIONER_CTL} init "${config}"
}

## Stop commissioner
stop_commissioner() {
    set -e
    ${COMMISSIONER_CTL} exit || true
}

## Send command to commissioner.
## Args: $1: the command.
send_command_to_commissioner() {
    set -e
    local command=$1
    local result
    result=$(${COMMISSIONER_CTL} execute "${command}")

    echo "${result}"
    [ -n "${result}" ] || return 1

    echo "${result}" | grep -q "\[done\]" || return 1
}

## Start a joiner
## Args: $1: the type of the joiner, valid values are:
##           "meshcop", "ae", "nmkp";
## Return: "succeed" or "failed";
start_joiner() {
    set -e
    local joiner_type=$1
    local joiner_binary=""
    local joining_cmd=""
    local joiner_credential=""

    if [ "${joiner_type}" = "meshcop" ]; then
        joiner_binary=${NON_CCM_CLI}
        joining_cmd="start"
        joiner_credential=${JOINER_CREDENTIAL}
    elif [ "${joiner_type}" = "ae" ]; then
        joiner_binary=${CCM_CLI}
        joining_cmd="startae"
    elif [ "${joiner_type}" = "nmkp" ]; then
        joiner_binary=${CCM_CLI}
        joining_cmd="startnmkp"
    else
        die "invalid joiner type: ${joiner_type}"
    fi

    echo "starting ${joiner_type} joiner..."
    expect -f-  <<EOF || return 1
spawn ${joiner_binary} ${JOINER_NODE_ID}

send "factoryreset\r\n"
expect ">"

send "ifconfig up\r\n"
expect "Done"

send "channel ${CHANNEL}\r\n"
expect "Done"

send "joiner ${joining_cmd} ${joiner_credential}\r\n"
set timeout 20
expect {
    "Join success" {
        send "exit\r\n"
        send_user "joining secceed\n"
    }
    timeout {
        send_user "joining failed\n"
        exit 1
    }
}
EOF
}

## Form a Thread network with border agent as the leader.
## Args: $1: PSKc.
form_network() {
    set -e
    local pskc=$1

    sudo wpanctl leave

    sudo wpanctl -I wpan0 setprop Daemon:AutoAssociateAfterReset false
    sudo wpanctl -I wpan0 setprop Network:PSKc --data ${pskc}
    sudo wpanctl -I wpan0 setprop Network:Key --data ${MASTERKEY}
    sudo wpanctl -I wpan0 setprop Network:XPANID ${XPANID}
    sudo wpanctl -I wpan0 setprop Network:PANID ${PANID}
    sudo wpanctl -I wpan0 form ${NETWORK_NAME} -c ${CHANNEL}

    echo "======================================"
    sudo wpanctl status
}
