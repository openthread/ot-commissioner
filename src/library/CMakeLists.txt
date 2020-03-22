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

add_library(commissioner
    openthread/bloom_filter.cpp
    openthread/bloom_filter.hpp
    openthread/crc16.cpp
    openthread/crc16.hpp
    openthread/pbkdf2_cmac.cpp
    openthread/pbkdf2_cmac.hpp
    openthread/random.cpp
    openthread/random.hpp
    openthread/README.md
    openthread/sha256.cpp
    openthread/sha256.hpp
    cbor.cpp
    cbor.hpp
    coap.cpp
    coap.hpp
    coap_secure.hpp
    commissioner_impl.cpp
    commissioner_impl.hpp
    commissioner_safe.cpp
    commissioner_safe.hpp
    commissioning_session.cpp
    commissioning_session.hpp
    cose.cpp
    cose.hpp
    cwt.hpp
    dtls.cpp
    dtls.hpp
    endpoint.hpp
    error.cpp
    event.hpp
    logging.cpp
    logging.hpp
    multicast_dns.cpp
    multicast_dns.hpp
    network_data.cpp
    openthread
    socket.cpp
    socket.hpp
    time.hpp
    timer.hpp
    tlv.cpp
    tlv.hpp
    token_manager.cpp
    token_manager.hpp
    udp_proxy.cpp
    udp_proxy.hpp
    uri.hpp
    ${PROJECT_SOURCE_DIR}/include/commissioner/border_agent.hpp
    ${PROJECT_SOURCE_DIR}/include/commissioner/commissioner.hpp
    ${PROJECT_SOURCE_DIR}/include/commissioner/network_data.hpp
    ${PROJECT_SOURCE_DIR}/include/commissioner/error.hpp
    ${PROJECT_SOURCE_DIR}/include/commissioner/defines.hpp)
target_link_libraries(commissioner PRIVATE
    cose
    mdns
    mbedtls
    pthread
    fmt::fmt
    event_core
    event_pthreads
    commissioner-common)

target_include_directories(commissioner PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(commissioner PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party/mbedtls/repo/include
    ${PROJECT_SOURCE_DIR}/third_party/libevent/repo/include
    ${PROJECT_BINARY_DIR}/third_party/libevent/repo/include)

install(TARGETS commissioner
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/commissioner
        DESTINATION include)
