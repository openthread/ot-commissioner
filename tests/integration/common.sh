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

## This controls with which test suite to run.
## Set environment variable `OT_COMM_TEST_SUITE`
## to 1.1 or 1.2 to override this.
readonly TEST_SUITE=${OT_COMM_TEST_SUITE:="1.2"}

readonly CUR_DIR=$(dirname "$(realpath -s $0)")
readonly TEST_ROOT_DIR=${CUR_DIR}

if [ "${TEST_SUITE}" = "1.2" ]; then
    ## TODO(wgtdkp): replace with HTTPs URLs.
    readonly OTBR_REPO=git@github.com:librasungirl/ot-br-posix-1.2.git
    readonly OTBR_BRANCH=RC5
    readonly OTBR=${TEST_ROOT_DIR}/ot-br-posix-1.2
else
    readonly OTBR_REPO=https://github.com/openthread/ot-br-posix
    readonly OTBR_BRANCH=master
    readonly OTBR=${TEST_ROOT_DIR}/ot-br-posix
fi

readonly REGISTRAR_REPO=git@github.com:openthread/ot-registrar.git
readonly REGISTRAR_BRANCH=master
readonly REGISTRAR=${TEST_ROOT_DIR}/ot-registrar
readonly REGISTRAR_IP=fdaa:bb::de6
readonly REGISTRAR_LOG=${REGISTRAR}/logs/*/registrar.log
readonly TRI_LOG=${REGISTRAR}/logs/*/tri.log
REFISTRAR_IF=eth0

if [ "${TEST_SUITE}" = "1.2" ]; then
    readonly OPENTHREAD_REPO=git@github.com:openthread/openthread-1.2.git
    readonly OPENTHREAD_BRANCH=ccm/dev
    readonly OPENTHREAD=${TEST_ROOT_DIR}/openthread-ccm
else
    readonly OPENTHREAD_REPO=https://github.com/openthread/openthread
    readonly OPENTHREAD_BRANCH=master
    readonly OPENTHREAD=${TEST_ROOT_DIR}/openthread
fi

readonly RUNTIME_DIR=${TEST_ROOT_DIR}/tmp

readonly CCM_CLI=${RUNTIME_DIR}/ot-cli-ftd-ccm
readonly CCM_NCP=${RUNTIME_DIR}/ot-ncp-ftd-ccm
readonly NON_CCM_CLI=${RUNTIME_DIR}/ot-cli-ftd-non-ccm
readonly NON_CCM_NCP=${RUNTIME_DIR}/ot-ncp-ftd-non-ccm

readonly WPANTUND_CONF=/etc/wpantund.conf

readonly WPANTUND_LOG=${RUNTIME_DIR}/wpantund.log
readonly OTBR_LOG=${RUNTIME_DIR}/otbr.log

readonly COMMISSIONER_CLI=${TEST_ROOT_DIR}/../../build/src/app/cli/commissioner-cli
readonly COMMISSIONER_DAEMON=${TEST_ROOT_DIR}/../../tools/commissioner_thci/commissionerd.py
readonly COMMISSIONER_CTL=${TEST_ROOT_DIR}/../../tools/commissioner_thci/commissioner_ctl.py
readonly COMMISSIONER_DAEMON_LOG=${RUNTIME_DIR}/commissioner-daemon.log

readonly CCM_CONFIG=${TEST_ROOT_DIR}/../../src/app/etc/commissioner/ccm-config.json
readonly NON_CCM_CONFIG=${TEST_ROOT_DIR}/../../src/app/etc/commissioner/non-ccm-config.json

readonly JOINER_NODE_ID=2
readonly JOINER_EUI64=0x18b4300000000002
readonly JOINER_PASSPHRASE=abcd

## Thread network parameters
readonly NETWORK_NAME=openthread-test
readonly CHANNEL=19
readonly PANID=0xface
readonly XPANID=dead00beef00cafe
readonly MASTERKEY=00112233445566778899aabbccddeeff
readonly PSKC=3aa55f91ca47d1e4e71a08cb35e91591
readonly BACKBONE_PORT=5683

die() {
  echo " *** ERROR: " "$@"
  exit 1
}

executable_or_die() {
  [ -x "$1" ] || die "Missing executable: $1"
}

## Start the registrar in docker container.
## Return: the interface name of running registrar container.
start_registrar() {
    set -e
    local container=$(sudo docker ps -q)
    [ ! -z "${container}" ] && sudo docker kill ${container}

    cd ${REGISTRAR}

    ./script/start-service.sh

    local if_name=$(ifconfig | grep "br-*" | head -n 1 |  awk '{print $1}')

    REGISTRAR_IF=${if_name:0:-1}

    echo "registrar interface: ${REGISTRAR_IF}"

    cd -
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
    local backbone_if=$2

    sudo chmod 777 ${WPANTUND_CONF}
    sudo echo "Config:NCP:SocketPath \"system:${ncp} 1\"" > ${WPANTUND_CONF}

    sudo wpantund -c ${WPANTUND_CONF} -d 7 > ${WPANTUND_LOG} 2>&1 &
    sleep 1

    pidof wpantund

    if [ "${TEST_SUITE}" = "1.2" ]; then
        sudo otbr-agent -I wpan0 -B ${backbone_if} -d 7 -v > ${OTBR_LOG} 2>&1 &
    else
        sudo otbr-agent -I wpan0 -d 7 -v > ${OTBR_LOG} 2>&1 &
    fi
    sleep 1

    pidof otbr-agent
}

## Start commissioner daemon.
## Args: $1: configuration file path.
start_commissioner() {
    set -e
    if [ $(pgrep -f ${COMMISSIONER_DAEMON}) > 0 ]; then
        sudo kill -9 $(pgrep -f ${COMMISSIONER_DAEMON})
    fi

    local config=$1

    echo "starting commissioner daemon: [ ${COMMISSIONER_DAEMON} --cli ${COMMISSIONER_CLI} ]"
    python -u ${COMMISSIONER_DAEMON} --cli ${COMMISSIONER_CLI} --timeout 200 > ${COMMISSIONER_DAEMON_LOG} 2>&1 &
    sleep 1

    pgrep -f ${COMMISSIONER_DAEMON}

    init_commissioner "${config}"
}

## Initialize commissioner daemon with given configuration file.
## Args: $1: configuration file path.
init_commissioner() {
    set -e

    local config=$1

    echo "initializating commissioner daemon with configuration file: ${config}"
    ${COMMISSIONER_CTL} init ${config}
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

    local result=$(${COMMISSIONER_CTL} execute "${command}")

    echo "${result}"
    [ ! -z "${result}" ] || return 1

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
    local joiner_passphrase=""
    if [ ${joiner_type} = "meshcop" ]; then
        joiner_binary=${NON_CCM_CLI}
        joining_cmd="start"
        joiner_passphrase=${JOINER_PASSPHRASE}
    elif [ ${joiner_type} = "ae" ]; then
        joiner_binary=${CCM_CLI}
        joining_cmd="startae"
    elif [ ${joiner_type} = "nmkp" ]; then
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

send "joiner ${joining_cmd} ${joiner_passphrase}\r\n"
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
    if [ "${TEST_SUITE}" = "1.2" ]; then
        sudo wpanctl -I wpan0 setprop Thread:Backbone:CoapPort ${BACKBONE_PORT}
    fi
    sudo wpanctl -I wpan0 form ${NETWORK_NAME} -c ${CHANNEL}

    echo "======================================"
    sudo wpanctl status
}
