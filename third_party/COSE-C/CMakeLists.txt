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

add_library(cose)

target_sources(cose
    PRIVATE
        repo/src/bcrypt.c
        repo/src/cbor.c
        repo/src/cose_int.h
        repo/src/Cose.c
        repo/src/crypto.h
        repo/src/Encrypt.c
        repo/src/Encrypt0.c
        repo/src/key.c
        repo/src/MacMessage.c
        repo/src/MacMessage0.c
        repo/src/mbedtls.c
        repo/src/openssl.c
        repo/src/Recipient.c
        repo/src/Sign.c
        repo/src/Sign0.c
        repo/src/SignerInfo.c
)

target_compile_definitions(cose
    PUBLIC USE_MBED_TLS
)

target_compile_options(cose
    PRIVATE
        -std=gnu99
        -Wall
        -Wextra
        -Wfatal-errors
        -pedantic
)

target_link_libraries(cose
    PUBLIC
        cn-cbor
        mbedtls
)

target_include_directories(cose
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/repo/include
)
