add_executable(job-manager-test
    $<TARGET_OBJECTS:ut-main>
	test_job_manager.cpp
	commissioner_app_replace.cpp
	${PROJECT_SOURCE_DIR}/src/app/cli/interpreter.cpp
	${PROJECT_SOURCE_DIR}/src/app/ps/registry.cpp
	${PROJECT_SOURCE_DIR}/src/app/ps/persistent_storage_json.cpp
	${PROJECT_SOURCE_DIR}/src/common/file_util.cpp
	${PROJECT_SOURCE_DIR}/src/app/json.cpp
	${PROJECT_SOURCE_DIR}/src/app/cli/console.cpp
	${PROJECT_SOURCE_DIR}/src/common/utils.cpp
	${PROJECT_SOURCE_DIR}/src/common/error.cpp
	border_agent_functions_mock.cpp
	${PROJECT_SOURCE_DIR}/src/common/address.cpp
	${PROJECT_SOURCE_DIR}/src/app/cli/job_manager.cpp
	${PROJECT_SOURCE_DIR}/src/app/file_logger.cpp
    ${PROJECT_SOURCE_DIR}/src/app/sys_logger.cpp
	${PROJECT_SOURCE_DIR}/src/app/cli/security_materials.cpp
	${PROJECT_SOURCE_DIR}/src/app/cli/job.cpp
	${PROJECT_SOURCE_DIR}/src/common/time.cpp
	${PROJECT_SOURCE_DIR}/src/library/network_data.cpp
	commissioner_app_mock.cpp
    )

target_include_directories(job-manager-test
  PRIVATE
      ${PROJECT_SOURCE_DIR}/third_party/googletest/repo/googletest/include
      ${PROJECT_SOURCE_DIR}/third_party/googletest/repo/googlemock/include
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/fmtlib/repo/include>
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/json/repo/single_include>
      ${PROJECT_SOURCE_DIR}/include
      ${PROJECT_SOURCE_DIR}/src
)

set_target_properties(job-manager-test
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

target_link_libraries(job-manager-test
    gtest
	gmock
	fmt::fmt
	persistent-storage
	readline
	)

add_executable(interpreter-test
    $<TARGET_OBJECTS:ut-main>
	test_interpreter.cpp
	commissioner_app_replace.cpp
	${PROJECT_SOURCE_DIR}/src/app/cli/interpreter.cpp
	${PROJECT_SOURCE_DIR}/src/app/ps/registry.cpp
	${PROJECT_SOURCE_DIR}/src/app/ps/persistent_storage_json.cpp
	${PROJECT_SOURCE_DIR}/src/common/file_util.cpp
	${PROJECT_SOURCE_DIR}/src/app/json.cpp
	${PROJECT_SOURCE_DIR}/src/app/cli/console.cpp
	${PROJECT_SOURCE_DIR}/src/common/utils.cpp
	${PROJECT_SOURCE_DIR}/src/common/error.cpp
	border_agent_functions_mock.cpp
	${PROJECT_SOURCE_DIR}/src/common/address.cpp
	${PROJECT_SOURCE_DIR}/src/app/cli/job_manager.cpp
	${PROJECT_SOURCE_DIR}/src/app/cli/security_materials.cpp
	${PROJECT_SOURCE_DIR}/src/app/cli/job.cpp
	${PROJECT_SOURCE_DIR}/src/common/time.cpp
	${PROJECT_SOURCE_DIR}/src/library/network_data.cpp
	${PROJECT_SOURCE_DIR}/src/app/file_logger.cpp
    ${PROJECT_SOURCE_DIR}/src/app/sys_logger.cpp
	commissioner_app_mock.cpp
    )

target_include_directories(interpreter-test
  PRIVATE
      ${PROJECT_SOURCE_DIR}/third_party/googletest/repo/googletest/include
      ${PROJECT_SOURCE_DIR}/third_party/googletest/repo/googlemock/include
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/fmtlib/repo/include>
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/json/repo/single_include>
      ${PROJECT_SOURCE_DIR}/include
      ${PROJECT_SOURCE_DIR}/src
)

set_target_properties(interpreter-test
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

target_link_libraries(interpreter-test
    gtest
	gmock
	fmt::fmt
	persistent-storage
	readline
	)

add_subdirectory(ps)
