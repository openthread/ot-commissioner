#!/bin/bash
#
#  Copyright (c) 2026, The OpenThread Commissioner Authors.
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
#     names of its contributors may be endorse or promote products
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

USER_CONFIG="${NON_CCM_CONFIG}"
[ -z "${TEST_ROOT_DIR}" ] && . "$(dirname "$0")"/common.sh

my_start_commissioner()
{
    local config=$1

    # Kill existing daemon if any (without sudo)
    pkill -f "${COMMISSIONER_DAEMON}" || true
    sleep 1

    echo "starting commissioner daemon: [ ${COMMISSIONER_DAEMON} --cli ${COMMISSIONER_CLI} ]"
    python3 -u "${COMMISSIONER_DAEMON}" --cli "${COMMISSIONER_CLI}" --timeout 200 >"${COMMISSIONER_DAEMON_LOG}" 2>&1 &
    sleep 1

    pgrep -f "${COMMISSIONER_DAEMON}" || die "Failed to start commissioner daemon"

    echo "initializing commissioner daemon with configuration file: ${config}"
    ${COMMISSIONER_CTL} init "${config}"
}

test_traverser_leader_fallback()
{
    # Ensure runtime directory exists
    mkdir -p "${RUNTIME_DIR}"

    local config_file="${USER_CONFIG:-${NON_CCM_CONFIG}}"

    # Enable Route64 fallback mode by ignoring Route64 in Leader response
    export OT_COMM_IGNORE_ROUTE64_FOR_TEST=1

    if [ -n "${TBR_ADDR}" ] && [ -n "${TBR_PORT}" ]; then
        echo "Testing against external TBR: ${TBR_ADDR}:${TBR_PORT} using config ${config_file}"

        my_start_commissioner "${config_file}"

        # Connect to external TBR
        local start_res
        start_res=$(send_command_to_commissioner "start ${TBR_ADDR} ${TBR_PORT}")
        echo "${start_res}" | grep -q "\[done\]" || die "Failed to connect to external TBR"

    else
        echo "No external TBR specified. Using local simulator."
        start_daemon
        form_network "${PSKC}"

        my_start_commissioner "${config_file}"
        petition_commissioner
    fi

    local traverse_result
    traverse_result=$(send_command_to_commissioner "traversenetwork")
    echo "${traverse_result}"

    # Verify it finished successfully and printed summary
    echo "${traverse_result}" | grep -q -- "--- Traversal Summary ---" || die "Traversal failed or summary not found"

    stop_commissioner

    if [ -z "${TBR_ADDR}" ]; then
        stop_daemon
    fi
}

main()
{
    test_traverser_leader_fallback
}

main "$@"
