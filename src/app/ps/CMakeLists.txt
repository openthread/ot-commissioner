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

add_library(persistent-storage
    persistent_storage.hpp
    persistent_storage_json.hpp
    persistent_storage_json.cpp
    registry.hpp
    registry.cpp
    registry_entries.hpp
    registry_entries.cpp
    nlohmann_json.hpp
    semaphore.hpp
    semaphore.cpp
    semaphore_posix.hpp
)

target_include_directories(persistent-storage
    PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/mbedtls/repo/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/json/repo/single_include>
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(persistent-storage
    PUBLIC
    commissioner-common
    commissioner
)

if (BUILD_SHARED_LIBS)
    install(TARGETS persistent-storage
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
endif()

if (OT_COMM_TEST)
    add_library(commissioner-ps-test OBJECT
        persistent_storage_json_test.cpp
        registry_test.cpp
    )

    target_include_directories(commissioner-ps-test
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/third_party/Catch2/repo/single_include
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/json/repo/single_include>
    )
endif (OT_COMM_TEST)
