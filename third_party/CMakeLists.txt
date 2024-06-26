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

if (OT_COMM_TEST)
    add_subdirectory(googletest/repo)
endif()

if (OT_COMM_CCM)
    set(CN_CBOR_FATAL_WARNINGS OFF CACHE BOOL "Disable -Werror and /W4")
    set(CN_CBOR_COVERALLS OFF CACHE BOOL "Disable generating coveralls data")
    set(CN_CBOR_BUILD_TESTS OFF CACHE BOOL "Disable compiling cbor tests")
    add_subdirectory(cn-cbor/repo)

    add_subdirectory(COSE-C)
endif()

add_subdirectory(fmtlib/repo)

if (OT_COMM_APP)
    set(JSON_BuildTests OFF CACHE BOOL "Disable compiling json tests" FORCE)
    add_subdirectory(json/repo)
endif()

add_subdirectory(libevent)

add_subdirectory(mbedtls)

if (OT_COMM_APP)
    add_subdirectory(mdns/repo)
endif()
