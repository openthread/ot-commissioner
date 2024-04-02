#
#  Copyright (c) 2020, The OpenThread Commissioner Authors.
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

## Mbedtls builds static libraries by default.
if (BUILD_SHARED_LIBS)
    set(USE_STATIC_MBEDTLS_LIBRARY OFF CACHE BOOL "Disable static mbedtls library")
    set(USE_SHARED_MBEDTLS_LIBRARY ON  CACHE BOOL "Enable shared mbedtls library")
endif()

set(ENABLE_TESTING OFF CACHE BOOL "Disable mbedtls test")
set(ENABLE_PROGRAMS OFF CACHE BOOL "Disable mbetls program")

add_subdirectory(repo)

## Include the user config file by absolute path to avoid exposing current directory.
target_compile_definitions(mbedtls
    PUBLIC MBEDTLS_CONFIG_FILE="${CMAKE_CURRENT_SOURCE_DIR}/mbedtls-config.h"
)
target_include_directories(mbedtls
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/repo/include
)

target_compile_definitions(mbedx509
    PUBLIC MBEDTLS_CONFIG_FILE="${CMAKE_CURRENT_SOURCE_DIR}/mbedtls-config.h"
)
target_include_directories(mbedx509
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/repo/include
)

target_compile_definitions(mbedcrypto
    PUBLIC MBEDTLS_CONFIG_FILE="${CMAKE_CURRENT_SOURCE_DIR}/mbedtls-config.h"
)
target_include_directories(mbedcrypto
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/repo/include
)
