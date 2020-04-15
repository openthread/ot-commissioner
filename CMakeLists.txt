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

cmake_minimum_required(VERSION 3.13.1)
project(ot-commissioner VERSION 1.0.0)

option(OT_COMM_COVERAGE             "Enable coverage reporting" OFF)
option(OT_COMM_TEST                 "Build tests" ON)
option(OT_COMM_APP                  "Build the CLI App" ON)
option(OT_COMM_USE_VENDORED_LIBS    "Build using submodules instead of using find_package" ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if(NOT "${CMAKE_CXX_STANDARD}")
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wfatal-errors -Wno-missing-braces")

if(OT_COMM_USE_VENDORED_LIBS)
    # Defines for mbedtls
    set(MBEDTLS_USER_CONFIG ${PROJECT_SOURCE_DIR}/third_party/mbedtls/mbedtls_user_config.h)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMBEDTLS_USER_CONFIG_FILE='<${MBEDTLS_USER_CONFIG}>'")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMBEDTLS_USER_CONFIG_FILE='<${MBEDTLS_USER_CONFIG}>'")
    add_subdirectory(third_party EXCLUDE_FROM_ALL)
else()
    find_package(fmt REQUIRED)
    find_package(nlohmann_json REQUIRED)
    find_package(mdns REQUIRED)
    find_package(MbedTLS REQUIRED)
    find_package(Libevent REQUIRED)
    find_package(cose-c REQUIRED)
    find_package(cn-cbor REQUIRED)
    if (OT_COMM_TEST)
        find_package(Catch2 REQUIRED)
    endif()
endif()

add_subdirectory(src)

if (OT_COMM_TEST)
    add_subdirectory(tests)
endif()
add_subdirectory(tools)

