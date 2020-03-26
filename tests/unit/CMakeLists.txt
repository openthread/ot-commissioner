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

add_executable(commissioner-tests
    main.cpp
    test_address.cpp
    test_coap_secure.cpp
    test_coap.cpp
    test_commissioner_impl.cpp
    test_cose.cpp
    test_dtls.cpp
    test_json.cpp
    test_mesh_local_addr.cpp
    test_pskc.cpp
    test_socket.cpp
    test_token_manager.cpp)

target_link_libraries(commissioner-tests PRIVATE
    commissioner-app
    commissioner-common
    cose
    mdns
    mbedtls
    pthread
    fmt::fmt
    event_core_static
    event_pthreads_static)

target_include_directories(commissioner-tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/app
    ${PROJECT_SOURCE_DIR}/src/library)

if(OT_COMM_USE_VENDORED_LIBS)
    target_include_directories(commissioner-tests PRIVATE
        ${PROJECT_SOURCE_DIR}/third_party/mbedtls/repo/include
        ${PROJECT_SOURCE_DIR}/third_party/libevent/repo/include
        ${PROJECT_BINARY_DIR}/third_party/libevent/repo/include
        ${PROJECT_SOURCE_DIR}/third_party/Catch2/repo/single_include)
endif()

add_custom_target(test-commissioner
    COMMAND ./commissioner-tests
    DEPENDS commissioner-tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
