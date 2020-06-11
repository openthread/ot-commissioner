#
#  Copyright (c) 2020, The OpenThread Authors.
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

set(JAVA_PACKAGE_NAME io.openthread.commissioner)
set(JAVA_OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/io/openthread/commissioner)

find_package(SWIG 4.0 REQUIRED)
find_package(Java REQUIRED)

if (NOT OT_COMM_ANDROID)
    find_package(JNI REQUIRED)
    include_directories(${JNI_INCLUDE_DIRS})
endif()

include(UseSWIG)
include(UseJava)

set(CMAKE_SWIG_OUTDIR ${CMAKE_CURRENT_SOURCE_DIR}/io/openthread/commissioner)

list(APPEND CMAKE_SWIG_FLAGS "-package;${JAVA_PACKAGE_NAME}")

include_directories(${PROJECT_SOURCE_DIR}/include)

set_property(SOURCE commissioner.i PROPERTY CPLUSPLUS ON)

set(UseSWIG_TARGET_NAME_PREFERENCE LEGACY)

swig_add_library(commissioner-java
    TYPE SHARED
    LANGUAGE java
    SOURCES commissioner.i)

swig_link_libraries(commissioner-java
    commissioner
    commissioner-common
)

## GCC is too strict on this in optimization mode, see http://www.swig.org/Doc3.0/Java.html#Java_compiling_dynamic
## for explanations.
target_compile_options(commissioner-java
    PRIVATE
        -fno-strict-aliasing
)

install(TARGETS commissioner-java
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
