#!/bin/bash
#
#  Copyright (c) 2021, The OpenThread Commissioner Authors.
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

[ -z "${TEST_ROOT_DIR}" ] && . "$(dirname "$0")"/common.sh

LEADER_OUTPUT=${RUNTIME_DIR}/leader-output

start_leader() {
    expect <<EOF | tee "${LEADER_OUTPUT}" &
spawn ${NON_CCM_CLI} 9
send "factoryreset\r\n"
sleep 1
send "panid 0xface\r\n"
expect "Done"
send "ifconfig up\r\n"
expect "Done"
send "pskc ${PSKC}\r\n"
expect "Done"
send "thread start\r\n"
expect "Done"
sleep 3
send "state\r\n"
expect "leader"
expect "Done"
sleep 1
send "ipaddr linklocal\r\n"
expect "Done"
send "ba port\r\n"
expect "Done"
set timeout -1
wait eof
EOF
}

stop_leader() {
    killall expect
    killall "${NON_CCM_CLI}"
}

test_native_commissioner() {
    set -x
    start_leader
    sleep 6

    ba_lla=$(grep -A+1 'ipaddr linklocal' "${LEADER_OUTPUT}" | tail -n1 | tr -d '\r\n')
    ba_port=$(grep -A+1 'ba port' "${LEADER_OUTPUT}" | tail -n1 | tr -d '\r\n')

    start_daemon

    ot_ctl ifconfig up
    ot_ctl panid 0xface

    start_commissioner "${NON_CCM_CONFIG}"

    send_command_to_commissioner "start ${ba_lla}%wpan0 ${ba_port}"

    send_command_to_commissioner "active"
    stop_commissioner

    stop_daemon
    stop_leader
}
