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

add_library(commissioner
    cbor.cpp
    cbor.hpp
    coap.cpp
    coap.hpp
    coap_secure.hpp
    commissioner_impl.cpp
    commissioner_impl.hpp
    commissioner_impl_internal.hpp
    commissioner_safe.cpp
    commissioner_safe.hpp
    cose.cpp
    cose.hpp
    cwt.hpp
    dtls.cpp
    dtls.hpp
    endpoint.hpp
    event.hpp
    joiner_session.cpp
    joiner_session.hpp
    mbedtls_error.cpp
    mbedtls_error.hpp
    message.hpp
    network_data.cpp
    openthread/bloom_filter.cpp
    openthread/bloom_filter.hpp
    openthread/crc16.cpp
    openthread/crc16.hpp
    openthread/pbkdf2_cmac.cpp
    openthread/pbkdf2_cmac.hpp
    openthread/random.cpp
    openthread/random.hpp
    openthread/sha256.cpp
    openthread/sha256.hpp
    socket.cpp
    socket.hpp
    timer.hpp
    tlv.cpp
    tlv.hpp
    token_manager.cpp
    token_manager.hpp
    udp_proxy.cpp
    udp_proxy.hpp
    uri.hpp
)

target_link_libraries(commissioner
    PRIVATE
        $<$<BOOL:${OT_COMM_CCM}>:cn-cbor::cn-cbor>
        $<$<BOOL:${OT_COMM_CCM}>:cose>
        mbedtls
        mbedx509
        mbedcrypto
        fmt::fmt
        event_core
        event_pthreads
        $<$<NOT:$<BOOL:${OT_COMM_ANDROID}>>:pthread>
        commissioner-common
        commissioner-config
)

target_compile_definitions(commissioner
    PRIVATE
        $<IF:$<BOOL:${OT_COMM_CCM}>, OT_COMM_CONFIG_CCM_ENABLE=1, OT_COMM_CONFIG_CCM_ENABLE=0>
        $<IF:$<BOOL:${OT_COMM_REFERENCE_DEVICE}>, OT_COMM_CONFIG_REFERENCE_DEVICE_ENABLE=1, OT_COMM_CONFIG_REFERENCE_DEVICE_ENABLE=0>
)

target_include_directories(commissioner
    PUBLIC
        ${PROJECT_SOURCE_DIR}/third_party/googletest/repo/googletest/include
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

install(TARGETS commissioner
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)

## Install third-party shared libraries. We need to install
## those shared libraries to enable commissioner-cli.
##
## We here install the .a/.so files directly
## because those third-party libraries are included
## as EXCLUDE_FROM_ALL, which means `install()` is
## disabled for them (intended).

if (BUILD_SHARED_LIBS)
    set(EVENT_CORE event_core_shared)
    set(EVENT_PTHREADS event_pthreads_shared)
else()
    set(EVENT_CORE event_core_static)
    set(EVENT_PTHREADS event_pthreads_static)
endif()

install(FILES
        $<$<BOOL:${OT_COMM_CCM}>:$<TARGET_FILE:cn-cbor::cn-cbor>>
        $<$<BOOL:${OT_COMM_CCM}>:$<TARGET_FILE:cose>>
        $<$<BOOL:${OT_COMM_APP}>:$<TARGET_FILE:mdns>>
        $<TARGET_FILE:mbedtls>
        $<TARGET_FILE:mbedx509>
        $<TARGET_FILE:mbedcrypto>
        $<TARGET_FILE:fmt::fmt>
        $<TARGET_FILE:${EVENT_CORE}>
        $<TARGET_FILE:${EVENT_PTHREADS}>
        DESTINATION lib
)

## Update ldconfig links and caches.
install(CODE "execute_process(COMMAND sudo ldconfig)")

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/commissioner
        DESTINATION include
)

if (OT_COMM_TEST)
    add_executable(commissioner-test
        coap_secure.hpp
        coap_secure_test.cpp
        coap.hpp
        coap_test.cpp
        commissioner_impl.hpp
        commissioner_impl_internal.hpp
        commissioner_impl_test.cpp
        commissioner_safe.hpp
        commissioner_safe_test.cpp
        cose.hpp
        cose_test.cpp
        dtls.hpp
        dtls_test.cpp
        socket.hpp
        socket_test.cpp
        token_manager.hpp
        token_manager_test.cpp
        $<TARGET_OBJECTS:commissioner-common-test>
    )

    target_link_libraries(commissioner-test
        PRIVATE
            commissioner
            commissioner-common
            $<$<BOOL:${OT_COMM_CCM}>:cn-cbor::cn-cbor>
            $<$<BOOL:${OT_COMM_CCM}>:cose>
            mbedtls
            mbedx509
            mbedcrypto
            event_core
            event_pthreads
            gtest
            gmock
            gtest_main
    )

    target_compile_definitions(commissioner-test
        PRIVATE
            $<IF:$<BOOL:${OT_COMM_CCM}>, OT_COMM_CONFIG_CCM_ENABLE=1, OT_COMM_CONFIG_CCM_ENABLE=0>
    )

    target_include_directories(commissioner-test
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
            ${PROJECT_SOURCE_DIR}/third_party/googletest/repo/googletest/include
            ${PROJECT_SOURCE_DIR}/third_party/googletest/repo/googlemock/include
    )

    set_target_properties(commissioner-test
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
endif()
