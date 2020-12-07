add_executable(ps-test
    $<TARGET_OBJECTS:ut-main>
	test-registry.cpp
    )

target_include_directories(ps-test
  PRIVATE
      ${PROJECT_SOURCE_DIR}/third_party/googletest/repo/googletest/include
      ${PROJECT_SOURCE_DIR}/third_party/googletest/repo/googlemock/include
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/json/repo/single_include>
      ${PROJECT_SOURCE_DIR}/include
      ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(ps-test
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
	  
target_link_libraries(ps-test
    gtest
	)
