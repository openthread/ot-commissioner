#!/bin/bash
#
#  Copyright (c) 2023, The OpenThread Commissioner Authors.
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

test_mdns_browser()
{
    set -e

    if ! python3 -c "import zeroconf" &>/dev/null; then
        echo "SKIPPING mDNS browser test: zeroconf module not found."
        echo "Please install it using 'pip install zeroconf'"
        exit 0
    fi

    echo "Starting fake border agent service..."
    python3 "${CUR_DIR}/fake_border_agent.py" &
    FAKE_BA_PID=$!

    # Give the fake service some time to start
    sleep 2

    echo "Running mDNS browser..."
    OUTPUT=$(python3 "${CUR_DIR}/../../script/mdns-browser.py" --timeout 5 "_meshcop._udp.local.")

    echo "Stopping fake border agent service..."
    kill ${FAKE_BA_PID}
    wait ${FAKE_BA_PID} 2>/dev/null || true

    echo "Verifying mDNS browser output..."
    echo "Raw mdns-browser.py output:"
    echo "${OUTPUT}"

    # Dynamically extract the full service name from the output
    SERVICE_NAME=$(echo "${OUTPUT}" | grep -o "Service Discovered: OpenThread Border Router\.[0-9a-fA-F]\{32\}\._meshcop\._udp\.local\." | sed 's/Service Discovered: //')

    if [ -z "${SERVICE_NAME}" ]; then
        echo "FAIL: Service name not discovered in output"
        exit 1
    fi

    # Escape special characters for the final verification grep
    ESCAPED_SERVICE_NAME=$(echo "${SERVICE_NAME}" | sed 's/[\.\*+\?\|\{\}\(\)\[\]\\\^\$]/\\&/g')

    if ! echo "${OUTPUT}" | grep -q "Service Discovered: ${ESCAPED_SERVICE_NAME}"; then
        echo "FAIL: Service name verification failed"
        exit 1
    fi

    if ! echo "${OUTPUT}" | grep -q "Host: otbr.local.:49152"; then
        echo "FAIL: Host not discovered"
        exit 1
    fi

    if ! echo "${OUTPUT}" | grep -q "IP Address: 192.168.1.2"; then
        echo "FAIL: IP address not discovered"
        exit 1
    fi

    if ! echo "${OUTPUT}" | grep -q "IP Address: fe80::1"; then
        echo "FAIL: IPv6 address not discovered"
        exit 1
    fi

    if ! echo "${OUTPUT}" | grep -q "rv = 1"; then
        echo "FAIL: TXT record 'rv' not discovered"
        exit 1
    fi

    if ! echo "${OUTPUT}" | grep -q "nn = OTBR"; then
        echo "FAIL: TXT record 'nn' not discovered"
        exit 1
    fi

    echo "PASS: mDNS browser test"
}

test_mdns_browser
