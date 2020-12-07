add_library(ut-main OBJECT main.cpp)
target_include_directories(ut-main
    PRIVATE
	    ${PROJECT_SOURCE_DIR}/third_party/googletest/repo/googletest/include
	)
  
add_subdirectory(app)
