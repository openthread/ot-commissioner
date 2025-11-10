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

readonly CUR_DIR=$(dirname "$(realpath $0)")
readonly TEST_ROOT_DIR=${CUR_DIR}

readonly RUNTIME_DIR=/tmp/test-ot-commissioner

readonly OPENTHREAD_REPO=https://github.com/openthread/openthread
readonly OPENTHREAD_BRANCH=main
readonly OPENTHREAD=${OT_COMM_OPENTHREAD:-"${RUNTIME_DIR}/openthread"}

readonly NON_CCM_CLI=${RUNTIME_DIR}/ot-cli-ftd-non-ccm
readonly NON_CCM_RCP=${RUNTIME_DIR}/ot-rcp-non-ccm
readonly OT_CTL=${RUNTIME_DIR}/ot-ctl
readonly OT_DAEMON=${RUNTIME_DIR}/ot-daemon

readonly OT_DAEMON_SETTINGS_PATH=${RUNTIME_DIR}/tmp
readonly OT_DAEMON_LOG=${RUNTIME_DIR}/ot-daemon.log

## '/usr/local' is the by default installing directory.
readonly COMMISSIONER_CLI=/usr/local/bin/commissioner-cli
readonly COMMISSIONER_DAEMON=/usr/local/bin/commissionerd.py
readonly COMMISSIONER_CTL=/usr/local/bin/commissioner_ctl.py
readonly COMMISSIONER_DAEMON_LOG=${RUNTIME_DIR}/commissioner-daemon.log
readonly COMMISSIONER_LOG=./commissioner.log

readonly CCM_TOKEN=/usr/local/etc/commissioner/credentials/token.hex
readonly CCM_CA_CERT=/usr/local/etc/commissioner/credentials/trust-anchor.pem
readonly NON_CCM_CONFIG=/usr/local/etc/commissioner/non-ccm-config.json

readonly JOINER_NODE_ID=2
readonly JOINER_EUI64=0x18b4300000000002
readonly JOINER_CREDENTIAL=ABCDEF

## Thread network parameters
readonly NETWORK_NAME=openthread-test
readonly CHANNEL=19
readonly PANID=0xface
readonly XPANID=dead00beef00cafe
readonly NETWORK_KEY=00112233445566778899aabbccddeeff
readonly PSKC=3aa55f91ca47d1e4e71a08cb35e91591
readonly CHANNEL_MASK=0x07fff800
readonly SECURITY_POLICY=(672 onrc)
readonly MESH_LOCAL_PREFIX="fd00:db8::"

die() {
  echo " *** ERROR: " "$@"
  exit 1
}

executable_or_die() {
  [ -x "$1" ] || die "Missing executable: $1"
}

start_daemon() {
    cd "${RUNTIME_DIR}"

    if pidof ot-daemon; then
        stop_daemon
    fi

    sudo rm -rf ${OT_DAEMON_SETTINGS_PATH}
    sudo "${OT_DAEMON}" -Iwpan0 -d7 -v "spinel+hdlc+uart://${NON_CCM_RCP}?forkpty-arg=1" > "${OT_DAEMON_LOG}" 2>&1 &

    sleep 15

    pidof ot-daemon || die "Failed to start ot-daemon"
}

stop_daemon() {
    sudo killall ot-daemon || true
    sleep 1
    (pidof ot-daemon && die "killing ot-daemon failed") || true
}

## Start commissioner daemon.
## Args: $1: configuration file path.
start_commissioner() {
    if [ -n "$(pgrep -f "${COMMISSIONER_DAEMON}")" ]; then
        sudo kill -9 "$(pgrep -f "${COMMISSIONER_DAEMON}")"
    fi

    local config=$1

    echo "starting commissioner daemon: [ ${COMMISSIONER_DAEMON} --cli ${COMMISSIONER_CLI} ]"
    python3 -u "${COMMISSIONER_DAEMON}" --cli "${COMMISSIONER_CLI}" --timeout 200 > "${COMMISSIONER_DAEMON_LOG}" 2>&1 &
    sleep 1

    pgrep -f "${COMMISSIONER_DAEMON}"

    init_commissioner "${config}"
}

## Initialize commissioner daemon with given configuration file.
## Args: $1: configuration file path.
init_commissioner() {
    local config=$1

    echo "initializating commissioner daemon with configuration file: ${config}"
    ${COMMISSIONER_CTL} init "${config}"
}

## Stop commissioner
stop_commissioner() {
    send_command_to_commissioner "stop"
    ${COMMISSIONER_CTL} exit || true
}

## Send command to commissioner.
## Args:
##   $1: the command.
##   $2: the expected error (optional). The command
##       is expected to success if this argument is
##       not specified.
send_command_to_commissioner() {
    local command=$1
    local expect_error=""
    if [ "$#" == 2 ]; then
        expect_error="$2"
    fi
    local result
    result=$(${COMMISSIONER_CTL} execute "${command}")

    echo "${result}"
    [ -n "${result}" ] || return 1

    if [ -z "${expect_error}" ]; then
        echo "${result}" | grep -q "\[done\]" || return 1
    else
        echo "${result}" | grep -q "${expect_error}" || return 1
    fi
}

## Start a joiner
## Args: $1: the type of the joiner, valid values are:
##           "meshcop", "ae", "nmkp";
## Return: "succeed" or "failed";
start_joiner() {
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
    sudo expect -f-  <<EOF || return 1
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

ot_ctl() {
    sudo timeout -k 5 10 "${OT_CTL}" "$@"
}

## Form a Thread network with border agent as the leader.
## Args: $1: PSKc.
form_network() {
    local pskc=$1

    sudo "${OT_CTL}" dataset clear
    sudo "${OT_CTL}" dataset activetimestamp 1
    sudo "${OT_CTL}" dataset channel "${CHANNEL}"
    sudo "${OT_CTL}" dataset channelmask "${CHANNEL_MASK}"
    sudo "${OT_CTL}" dataset extpanid "${XPANID}"
    sudo "${OT_CTL}" dataset meshlocalprefix "${MESH_LOCAL_PREFIX}"
    sudo "${OT_CTL}" dataset networkkey "${NETWORK_KEY}"
    sudo "${OT_CTL}" dataset networkname "${NETWORK_NAME}"
    sudo "${OT_CTL}" dataset panid "${PANID}"
    sudo "${OT_CTL}" dataset pskc "${pskc}"
    sudo "${OT_CTL}" dataset securitypolicy "${SECURITY_POLICY[@]}"
    sudo "${OT_CTL}" dataset commit active
    sudo "${OT_CTL}" ifconfig up
    sudo "${OT_CTL}" thread start

    sleep 10

    ot_ctl state
}

petition_commissioner() {
    local ba_port
    ba_port="$(sudo "${OT_CTL}" ba port | grep -o '[0-9]\+')"
    send_command_to_commissioner "start :: ${ba_port}"
}

install_borderagent_mdns_data() {
    local ba_mdns_data_dir=$1

    [[ -d "${ba_mdns_data_dir}" ]] || {
        echo "'${ba_mdns_data_dir}' is not a directory"
        exit 1
    }

    sudo rm -f /etc/avahi/services/*.service
    sudo cp "${ba_mdns_data_dir}"/* /etc/avahi/services/
    sleep 3
}

uninstall_borderagent_mdns_data() {
    sudo rm -f /etc/avahi/services/*.service
}

start_border_agent_mdns_service() {
    ## See etc/avahi/services/border-agent.service
    ## for the service registration.
    sudo service avahi-daemon restart

    ## Wait for the service to start.
    sleep 3
}

stop_border_agent_mdns_service() {
    sudo service avahi-daemon stop
}

declare -a jobids

mdns_hosts_map_addresses() {
    local last_addr
    local last_br

    for i in $(seq 0 3)
    do
        last_br=$(($i + 1))
        last_addr=$(($i + 2))
        avahi-publish-service -a ba$last_br.local ::$last_addr &
        jobids[$i]=$!
    done

    sleep 3
}

mdns_hosts_unmap_addresses() {
    for i in ${!jobids[*]}
    do
        if [ ${jobids[$i]} -ne 0 ]
        then
            kill ${jobids[$i]}
            jobids[$i]=0
        fi
    done
}

commissioner_mdns_scan_import() {
    set -e
    local src_dir=$1

    sudo rm -f /etc/avahi/services/*.service
    rm -f /tmp/nwk.json
    for i in $(ls -1 $src_dir)
    do
        sudo cp $src_dir/$i /etc/avahi/services/
        sleep 3
        send_command_to_commissioner "br scan --timeout 1000 --export /tmp/nwk.json"
        send_command_to_commissioner "br add /tmp/nwk.json"
        rm -f /tmp/nwk.json
        sudo rm -f /etc/avahi/services/$i
    done
}
