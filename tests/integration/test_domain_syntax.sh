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

mdns_announce_add_th() {
    sudo cp ${CUR_DIR}/etc/test_harness/* /etc/avahi/services/

    local ba_port
    ba_port="$(sudo "${OT_CTL}" ba port | grep -o '[0-9]\+')"
    sudo sed -i "s/49191/$ba_port/" /etc/avahi/services/border-agent.service
    sudo cat /etc/avahi/services/border-agent.service
    sleep 3
    start_border_agent_mdns_service

    send_command_to_commissioner "br scan --timeout 1000 --export /tmp/nwk.json"
    sed -i '/Addr/ s/^.*$/"Addr": "::1",/' /tmp/nwk.json

    cat /tmp/nwk.json

    send_command_to_commissioner "br add /tmp/nwk.json"
    rm -f /tmp/nwk.json
    sudo rm -f /etc/avahi/services/*.service
}

test_network_list() {
    set -e

    start_border_agent_mdns_service
    mdns_hosts_map_addresses
    start_commissioner "${NON_CCM_CONFIG}"
    commissioner_mdns_scan_import ${CUR_DIR}/etc/br_scan_initial

    send_command_to_commissioner "network list" "thread1"

    stop_commissioner
    mdns_hosts_unmap_addresses
}

test_select_identify() {
    set -e

    start_border_agent_mdns_service
    start_commissioner "${NON_CCM_CONFIG}"
    mdns_hosts_map_addresses
    commissioner_mdns_scan_import ${CUR_DIR}/etc/br_scan_initial

    send_command_to_commissioner "network identify" "none"
    send_command_to_commissioner "network select thread1"
    send_command_to_commissioner "network identify" "thread1"

    send_command_to_commissioner "network select 2222222222222222"
    send_command_to_commissioner "network identify" "thread2"

    send_command_to_commissioner "network select none"
    send_command_to_commissioner "network identify" "none"

    stop_commissioner
    mdns_hosts_unmap_addresses
}

test_start_stop_selected() {
    set -e

    start_daemon
    form_network "${PSKC}"

    start_commissioner "${NON_CCM_CONFIG}"

    mdns_announce_add_th

    send_command_to_commissioner "br list"
    send_command_to_commissioner "network identify" "none"
    send_command_to_commissioner "network select openthread-test"
    send_command_to_commissioner "start"
    send_command_to_commissioner "opdataset get active" "openthread-test"
    send_command_to_commissioner "stop"
    send_command_to_commissioner "network select none"
    send_command_to_commissioner "start --dom TestDomainName"
    send_command_to_commissioner "opdataset get active --dom TestDomainName" "openthread-test"
    send_command_to_commissioner "stop --nwk all"

    stop_commissioner
    stop_daemon
}

test_start_stop_mn_explicit() {
    set -e

    aods_fn="/tmp/aods.json"

    start_daemon
    form_network "${PSKC}"

    start_commissioner "${NON_CCM_CONFIG}"

    mdns_announce_add_th

    mdns_hosts_map_addresses
    commissioner_mdns_scan_import ${CUR_DIR}/etc/br_scan_initial

    send_command_to_commissioner "start --nwk openthread-test"
    send_command_to_commissioner "opdataset get active --nwk openthread-test --export $aods_fn"
    send_command_to_commissioner "stop --nwk openthread-test"
    send_command_to_commissioner "active --nwk openthread-test" "false"

    stop_commissioner
    stop_daemon
    mdns_hosts_unmap_addresses
}

test_start_stop_mn_all() {
    set -e

    aods_fn="/tmp/aods.json"

    start_daemon
    form_network "${PSKC}"

    start_commissioner "${NON_CCM_CONFIG}"

    mdns_announce_add_th

    mdns_hosts_map_addresses
    commissioner_mdns_scan_import ${CUR_DIR}/etc/br_scan_initial

    send_command_to_commissioner "start --nwk all"
    send_command_to_commissioner "active --nwk all" 'DEAD00BEEF00CAFE.*true'
    send_command_to_commissioner "sessionid --nwk all" 'DEAD00BEEF00CAFE": \d*'
    send_command_to_commissioner "opdataset get active --nwk all" 'DEAD00BEEF00CAFE": {'
    send_command_to_commissioner "commdataset get --nwk all" 'DEAD00BEEF00CAFE": {'
    send_command_to_commissioner "opdataset set securitypolicy 1000 ff --nwk all"
    send_command_to_commissioner "opdataset get pending --nwk all"
    send_command_to_commissioner "stop --nwk all"

    stop_commissioner
    stop_daemon
    mdns_hosts_unmap_addresses
}

test_start_stop_mn_other() {
    set -e

    aods_fn="/tmp/aods.json"

    start_daemon
    form_network "${PSKC}"

    start_commissioner "${NON_CCM_CONFIG}"

    mdns_announce_add_th

    mdns_hosts_map_addresses
    commissioner_mdns_scan_import ${CUR_DIR}/etc/br_scan_initial

    send_command_to_commissioner "network select 2222222222222222"
    send_command_to_commissioner "start --nwk other"
    send_command_to_commissioner "active --nwk other" '1111111111111111": false'
    send_command_to_commissioner "sessionid --nwk other" 'DEAD00BEEF00CAFE": \d*'
    send_command_to_commissioner "opdataset get active --nwk other" 'DEAD00BEEF00CAFE": {'
    send_command_to_commissioner "commdataset get --nwk other" 'DEAD00BEEF00CAFE": {'
    send_command_to_commissioner "opdataset set securitypolicy 1000 ff --nwk other"
    send_command_to_commissioner "opdataset get pending --nwk other"
    send_command_to_commissioner "stop --nwk other"
    send_command_to_commissioner "network select none"

    stop_commissioner
    stop_daemon
    mdns_hosts_unmap_addresses
}

test_start_stop_mn_dom() {
    set -e

    aods_fn="/tmp/aods.json"

    start_daemon
    form_network "${PSKC}"

    start_commissioner "${NON_CCM_CONFIG}"

    mdns_announce_add_th

    mdns_hosts_map_addresses
    commissioner_mdns_scan_import ${CUR_DIR}/etc/br_scan_initial

    set -x
    send_command_to_commissioner "start --dom TestDomainName"
    send_command_to_commissioner "active --dom TestDomainName" '2222222222222222": false'
    send_command_to_commissioner "sessionid --dom TestDomainName" 'DEAD00BEEF00CAFE": \d*'
    send_command_to_commissioner "opdataset get active --dom TestDomainName" 'DEAD00BEEF00CAFE": {'
    send_command_to_commissioner "commdataset get --dom TestDomainName" 'DEAD00BEEF00CAFE": {'
    send_command_to_commissioner "opdataset set securitypolicy 1000 ff --dom TestDomainName"
    send_command_to_commissioner "opdataset get pending --dom TestDomainName"
    send_command_to_commissioner "stop --dom TestDomainName"

    set +x

    stop_commissioner
    stop_daemon
    mdns_hosts_unmap_addresses
}

test_network_management() {
    set -e

    start_commissioner "${NON_CCM_CONFIG}"
    start_border_agent_mdns_service
    mdns_hosts_map_addresses
    commissioner_mdns_scan_import ${CUR_DIR}/etc/br_scan_initial

    send_command_to_commissioner "br list"
    send_command_to_commissioner "network list"

    send_command_to_commissioner "network list --nwk thread1" "1111111111111111"
    send_command_to_commissioner "network list --dom TestDomainName" "2222222222222222"
    send_command_to_commissioner "network list --nwk all" "3333333333333333"
    send_command_to_commissioner "network select thread3"
    send_command_to_commissioner "network list --nwk this" "3333333333333333"
    send_command_to_commissioner "network list --nwk other" "2222222222222222"
    send_command_to_commissioner "network list --dom this" "3333333333333333"

    send_command_to_commissioner "domain list --dom this" "TestDomainName2"

    send_command_to_commissioner "br list --nwk thread1" '"addr": "::2"'
    send_command_to_commissioner "br list --nwk thread2" '"addr": "::4"'
    send_command_to_commissioner "br list --dom TestDomainName" '"addr": "::2"'
    send_command_to_commissioner "br list --dom TestDomainName2" '"addr": "::5"'
    send_command_to_commissioner "network select none"

    send_command_to_commissioner "br delete 3"
    send_command_to_commissioner "br list --dom TestDomainName2" '\[failed\]'
    send_command_to_commissioner "br delete --nwk thread2"
    send_command_to_commissioner "br list --nwk thread2" '\[failed\]'
    send_command_to_commissioner "br list"
    send_command_to_commissioner "network list"
    ## Recursive behavior: RB 3 was last in NWK thread3 dom TestDomainName2
    send_command_to_commissioner "network list --nwk thread3" '\[failed\]'
    send_command_to_commissioner "domain list  --dom TestDomainName2" '\[failed\]'

    send_command_to_commissioner "br delete --dom TestDomainName"
    send_command_to_commissioner "network list --nwk thread1" '\[failed\]'
    send_command_to_commissioner "network list --nwk thread2" '\[failed\]'
    send_command_to_commissioner "domain list --dom TestDomainName" '\[failed\]'

    stop_commissioner
    mdns_hosts_unmap_addresses
}

test_mn_input_data() {
    set -e

    start_commissioner "${NON_CCM_CONFIG}"

    send_command_to_commissioner "network list --nwk unknown-net" '\[failed\]'
    send_command_to_commissioner "domain list --dom unknown-domain" '\[failed\]'

    send_command_to_commissioner "br list --nwk unknown-net" '\[failed\]'
    send_command_to_commissioner "br list --dom unknown-domain" '\[failed\]'

    send_command_to_commissioner "br delete 10" '\[failed\]'
    send_command_to_commissioner "br delete --nwk unknown-net" '\[failed\]'
    send_command_to_commissioner "br delete --dom unknown-domain" '\[failed\]'

    stop_commissioner
}

test_mn_input_data_export_import() {
    set -e

    start_daemon
    form_network "${PSKC}"

    start_commissioner "${NON_CCM_CONFIG}"

    mdns_announce_add_th

    send_command_to_commissioner "br list" "addr"

    send_command_to_commissioner "network select openthread-test"
    send_command_to_commissioner "start"

    send_command_to_commissioner "opdataset get active --export /tmp/aods.json"
    send_command_to_commissioner "opdataset get active --nwk openthread-test --export /tmp/aods1.json"

    send_command_to_commissioner "stop"

    stop_commissioner
    stop_daemon

    rm -f /tmp/aods.json
}

test_start_on_fake_br_data() {
    set -e

    start_daemon
    form_network "${PSKC}"

    start_commissioner "${NON_CCM_CONFIG}"

    mdns_announce_add_th

    send_command_to_commissioner "network select openthread-test"
    send_command_to_commissioner "start"
    send_command_to_commissioner "active" "true"
    send_command_to_commissioner "stop"

    stop_commissioner
    stop_daemon
}
