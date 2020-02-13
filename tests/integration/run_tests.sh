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

. $(dirname $0)/test_announce_begin.sh
. $(dirname $0)/test_discover.sh
. $(dirname $0)/test_energy_scan.sh
. $(dirname $0)/test_joining.sh
. $(dirname $0)/test_operational_dataset.sh
. $(dirname $0)/test_pan_id_query.sh
. $(dirname $0)/test_petition.sh

## Run a test case and collect the result.
## Return: 0, test case succeed, 1, test case failed;
## Note: A test case must be started with 'test_' to make it runnable.
run_test_case() {
    local test_case=$1

    echo "====== test case: [ ${test_case} ] ======"

    rm -rf ${COMMISSIONER_LOG}

    ## we cannot declare output with `local`,
    ## because `local` is a command and its return value iwll override
    ## return value of the test case.
    output=$(${test_case})
    local result=$?

    if [ ${result} = 0 ]; then
        echo "state: pass"
    else
        result=1
        echo "state: fail"

        echo "------ test output begin ------"
        echo "${output}"
        echo "------ test output end ------"

        echo "------ wpantund log begin ------"
        cat ${WPANTUND_LOG}
        echo "------ wpantund log end ------"

        echo "------ otbr log begin ------"
        cat ${OTBR_LOG}
        echo "------ otbr log end ------"

        echo "------ commissioner daemon log begin ------"
        cat ${COMMISSIONER_DAEMON_LOG}
        echo "------ commissioner daemon log end ------"

        echo "------ commissioner library log begin ------"
        cat ${COMMISSIONER_LOG}
        echo "------ commissioner library log end ------"
    fi

    return ${result}
}

main () {
    local test_case_to_run=""

    if [ ! -z "$#" ]; then
        test_case_to_run="$1"
    fi

    local total_case_count=0
    local failed_case_count=0

    echo "###### integration tests begin ######"
    for test_case in $(declare -F | awk '{print $NF}' | grep "^test_"); do
        if [ ! -z ${test_case_to_run} ] && [ ${test_case} != ${test_case_to_run} ]; then
            continue
        fi

        run_test_case ${test_case}
        failed_case_count=$((failed_case_count + $?))
        total_case_count=$((total_case_count + 1))
    done

    echo "###### integration tests end ######"

    if [ ${failed_case_count} != 0 ]; then
        echo "${failed_case_count}/${total_case_count} cases failed"

        echo "###################################"
        return 1
    else
        echo "all(${total_case_count}) test cases passed"

        echo "###################################"
        return 0
    fi
}

main "$@"
