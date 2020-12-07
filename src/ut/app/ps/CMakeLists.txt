add_executable(ps-test
    $<TARGET_OBJECTS:ut-main>
    )

set_target_properties(ps-test
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
	  
target_link_libraries(ps-test
    gtest
	)
