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

add_executable(commissioner-cli
    console.cpp
    console.hpp
    interpreter.cpp
    interpreter.hpp
    job.cpp
    job.hpp
    job_manager.cpp
    job_manager.hpp
    security_materials.cpp
    security_materials.hpp
    main.cpp
)

target_include_directories(commissioner-cli
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/json/repo/single_include>
        ${PROJECT_BINARY_DIR}/third_party/libevent/repo/include>
        ${PROJECT_SOURCE_DIR}/third_party/libevent/repo/include
)

target_link_libraries(commissioner-cli
    PRIVATE
        commissioner-app
        commissioner-common
        event_core
        event_pthreads
        mdns
        persistent-storage
        pthread
        readline
)

install(TARGETS commissioner-cli
        RUNTIME DESTINATION bin
)

if (OT_COMM_TEST)
add_library(ut-main OBJECT gmock_main.cpp)

target_include_directories(ut-main
    PRIVATE
        ${PROJECT_SOURCE_DIR}/third_party/googletest/repo/googletest/include
    )

add_executable(job-manager-test
    gmock_main.cpp
    job_manager_test.cpp
    interpreter.cpp
    ${PROJECT_SOURCE_DIR}/src/app/ps/registry.cpp
    ${PROJECT_SOURCE_DIR}/src/app/ps/persistent_storage_json.cpp
    ${PROJECT_SOURCE_DIR}/src/app/file_util.cpp
    ${PROJECT_SOURCE_DIR}/src/app/json.cpp
    ${PROJECT_SOURCE_DIR}/src/app/mdns_handler.cpp
    ${PROJECT_SOURCE_DIR}/src/app/border_agent_functions_mock.cpp
    console.cpp
    ${PROJECT_SOURCE_DIR}/src/common/utils.cpp
    ${PROJECT_SOURCE_DIR}/src/common/error.cpp
    ${PROJECT_SOURCE_DIR}/src/common/address.cpp
    job_manager.cpp
    ${PROJECT_SOURCE_DIR}/src/app/file_logger.cpp
    security_materials.cpp
    job.cpp
    ${PROJECT_SOURCE_DIR}/src/common/time.cpp
    ${PROJECT_SOURCE_DIR}/src/library/network_data.cpp
    ${PROJECT_SOURCE_DIR}/src/app/commissioner_app_mock.cpp
    ${PROJECT_SOURCE_DIR}/src/app/commissioner_app_replace.cpp
    )

target_include_directories(job-manager-test
  PRIVATE
      ${PROJECT_SOURCE_DIR}/third_party/googletest/repo/googletest/include
      ${PROJECT_SOURCE_DIR}/third_party/googletest/repo/googlemock/include
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/fmtlib/repo/include>
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/json/repo/single_include>
      ${PROJECT_SOURCE_DIR}/include
      ${PROJECT_SOURCE_DIR}/src
      ${PROJECT_BINARY_DIR}/third_party/libevent/repo/include
      ${PROJECT_SOURCE_DIR}/third_party/libevent/repo/include
)

set_target_properties(job-manager-test
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

target_link_libraries(job-manager-test
    event_core
    event_pthreads
    fmt::fmt
    gtest
    gmock
    mdns
    persistent-storage
    readline
    )

add_executable(interpreter-test
    $<TARGET_OBJECTS:ut-main>
    interpreter_test.cpp
    interpreter.cpp
    ${PROJECT_SOURCE_DIR}/src/app/ps/registry.cpp
    ${PROJECT_SOURCE_DIR}/src/app/ps/persistent_storage_json.cpp
    ${PROJECT_SOURCE_DIR}/src/app/file_util.cpp
    ${PROJECT_SOURCE_DIR}/src/app/json.cpp
    ${PROJECT_SOURCE_DIR}/src/app/mdns_handler.cpp
    ${PROJECT_SOURCE_DIR}/src/app/border_agent_functions_mock.cpp
    console.cpp
    ${PROJECT_SOURCE_DIR}/src/common/utils.cpp
    ${PROJECT_SOURCE_DIR}/src/common/error.cpp
    ${PROJECT_SOURCE_DIR}/src/common/address.cpp
    job_manager.cpp
    security_materials.cpp
    job.cpp
    ${PROJECT_SOURCE_DIR}/src/common/time.cpp
    ${PROJECT_SOURCE_DIR}/src/library/network_data.cpp
    ${PROJECT_SOURCE_DIR}/src/app/file_logger.cpp
    ${PROJECT_SOURCE_DIR}/src/app/commissioner_app_mock.cpp
    ${PROJECT_SOURCE_DIR}/src/app/commissioner_app_replace.cpp
    )

target_include_directories(interpreter-test
  PRIVATE
      ${PROJECT_SOURCE_DIR}/third_party/googletest/repo/googletest/include
      ${PROJECT_SOURCE_DIR}/third_party/googletest/repo/googlemock/include
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/fmtlib/repo/include>
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/json/repo/single_include>
      ${PROJECT_SOURCE_DIR}/include
      ${PROJECT_SOURCE_DIR}/src
      ${PROJECT_BINARY_DIR}/third_party/libevent/repo/include
      ${PROJECT_SOURCE_DIR}/third_party/libevent/repo/include
)

set_target_properties(interpreter-test
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

target_link_libraries(interpreter-test
    event_core
    event_pthreads
    fmt::fmt
    gtest
    gmock
    mdns
    persistent-storage
    readline
    )
endif()
