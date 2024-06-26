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

if (BUILD_SHARED_LIBS)
    set(EVENT__LIBRARY_TYPE "SHARED" CACHE BOOL "libevent build shared libraries" FORCE)
else()
    set(EVENT__LIBRARY_TYPE "STATIC" CACHE BOOL "libevent build static libraries" FORCE)
endif()
set(EVENT__DISABLE_OPENSSL ON CACHE BOOL "libevent disable OpenSSL" FORCE)
set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "libevent disable samples" FORCE)
set(EVENT__DISABLE_REGRESS ON CACHE BOOL "libevent disable regression tests" FORCE)
set(EVENT__DISABLE_TESTS ON CACHE BOOL "libevent disable tests" FORCE)
set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "libevent disable benchmark" FORCE)
set(EVENT__HAVE_PIPE2 OFF CACHE BOOL "disable use of pipe2 in libevent" FORCE)
add_subdirectory(repo)

target_include_directories(event_core
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/repo/include
        ${CMAKE_CURRENT_BINARY_DIR}/repo/include
)
