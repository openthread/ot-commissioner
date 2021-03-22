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

test_br_scan() {
    set -e

	install_borderagent_mdns_data etc/br_scan_initial
    start_border_agent_mdns_service
    start_commissioner "${NON_CCM_CONFIG}"

    ## TODO(wgtdkp): verify the output
    send_command_to_commissioner "br scan" "[done]"

    stop_commissioner
    stop_border_agent_mdns_service
	uninstall_borderagent_mdns_data etc/br_scan_initial
}

test_br_scan_export() {
    set -e

	install_borderagent_mdns_data etc/br_scan_initial
    start_border_agent_mdns_service
    start_commissioner "${NON_CCM_CONFIG}"

    send_command_to_commissioner "br scan --export /tmp/br_scan_export.json" "[done]"

    stop_commissioner
    stop_border_agent_mdns_service
	uninstall_borderagent_mdns_data etc/br_scan_initial
	
	grep -q "ExtendedPanId" /tmp/br_scan_export.json
	jsonlint-php /tmp/br_scan_export.json
	rm -f /tmp/br_scan_export.json
}

test_registry() {
    set -e

	install_borderagent_mdns_data etc/br_scan_initial
    start_border_agent_mdns_service
    start_commissioner "${NON_CCM_CONFIG}"

    send_command_to_commissioner "br scan --export /tmp/br_scan_export.json" "[done]"
	send_command_to_commissioner "br add /tmp/br_scan_export.json" "[done]"
	send_command_to_commissioner "network list" "[done]"
	
    stop_commissioner
    stop_border_agent_mdns_service
	uninstall_borderagent_mdns_data etc/br_scan_initial

	rm -f /tmp/br_scan_export.json
}

test_scan_filter() {
    set -e

	rm -f /tmp/br_scan_export.json /tmp/registry.json

	install_borderagent_mdns_data etc/br_scan_initial
    start_border_agent_mdns_service
    start_commissioner "${NON_CCM_CONFIG}"

    send_command_to_commissioner "br scan --export /tmp/br_scan_export.json" "[done]"
	send_command_to_commissioner "br add /tmp/br_scan_export.json" "[done]"
	send_command_to_commissioner "network list" "thread1"

	send_command_to_commissioner "br scan --nwk thread1 --export /tmp/br_scan_export-thread1.json" "[done]"

	cnt=$(grep "Address" /tmp/br_scan_export-thread1.json | wc -l)
	[[ "$cnt" == "2" ]] || {
		echo Expected 2 BR records, detected $cnt
		exit 1
	}
	
    stop_commissioner
    stop_border_agent_mdns_service
	uninstall_borderagent_mdns_data etc/br_scan_initial

	rm -f /tmp/br_scan_export.json /tmp/br_scan_export-thread1.json
}

test_br_update_on_add() {
    set -e

	rm -f /tmp/br_scan_export.json /tmp/registry.json

	install_borderagent_mdns_data etc/br_scan_initial
    start_border_agent_mdns_service
    start_commissioner "${NON_CCM_CONFIG}"

    send_command_to_commissioner "br scan --export /tmp/br_scan_export.json"
	send_command_to_commissioner "br add /tmp/br_scan_export.json"
	send_command_to_commissioner "br list --nwk thread2" "::3"

    stop_border_agent_mdns_service
	uninstall_borderagent_mdns_data etc/br_scan_initial
	install_borderagent_mdns_data etc/br_scan_modified

    send_command_to_commissioner "br scan --export /tmp/br_scan_export.json"
	send_command_to_commissioner "br add /tmp/br_scan_export.json"
	send_command_to_commissioner "br list --nwk thread2" "::5"

    stop_commissioner
    stop_border_agent_mdns_service
	uninstall_borderagent_mdns_data etc/br_scan_modified

	rm -f /tmp/br_scan_export.json /tmp/nwk-thread2.json
}
