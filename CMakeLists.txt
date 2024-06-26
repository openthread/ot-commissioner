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

cmake_minimum_required(VERSION 3.20.1)
project(ot-commissioner VERSION 0.2.0)

option(OT_COMM_ANDROID           "Build with Android NDK" OFF)
option(OT_COMM_APP               "Build the CLI App" ON)
option(OT_COMM_CCM               "Build with Commercial Commissioning Mode" ON)
option(OT_COMM_REFERENCE_DEVICE  "Enable features for being a Thread Group reference device" OFF)
option(OT_COMM_COVERAGE          "Enable coverage reporting" OFF)
option(OT_COMM_JAVA_BINDING      "Build Java binding" OFF)
set(OT_COMM_JAVA_BINDING_OUTDIR  "" CACHE STRING "Specify output directory of generated Java source files")
option(OT_COMM_TEST              "Build tests" ON)
option(OT_COMM_WARNING_AS_ERROR  "Treat warning as error" OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if (CMAKE_BUILD_TYPE STREQUAL Debug)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Output compile commands for clang-tidy" FORCE)
endif()

if(NOT "${CMAKE_CXX_STANDARD}")
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

# `pthread` is already included in Android glibc implementation (`bionic`),
# so it is not necessary to find the thread libs separately.
if (ANDROID)
    set (CMAKE_THREAD_LIBS_INIT 1)
endif()

execute_process(
    COMMAND git describe HEAD --always
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE OT_COMM_GIT_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(OT_COMM_GIT_REVISION)
    set(OT_COMM_VERSION "${PROJECT_VERSION}+${OT_COMM_GIT_REVISION}")
else()
    set(OT_COMM_VERSION "${PROJECT_VERSION}")
endif()

message(STATUS "Version: ${OT_COMM_VERSION}")

add_library(commissioner-config INTERFACE)
target_compile_definitions(commissioner-config
    INTERFACE OT_COMM_VERSION="${OT_COMM_VERSION}"
)

# Override GoogleTest defaults to omit GoogltTest installation
option(INSTALL_GTEST "Enable installation of googletest. (Projects embedding googletest may want to turn this OFF.)" OFF)

if (OT_COMM_TEST)
  add_definitions(-DMOCKABLE=virtual)
else (OT_COMM_TEST)
  add_definitions(-DMOCKABLE=)
endif (OT_COMM_TEST)

add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(third_party EXCLUDE_FROM_ALL)
