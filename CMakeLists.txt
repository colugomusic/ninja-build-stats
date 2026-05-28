cmake_minimum_required(VERSION 4.3.1)
project(ninja-build-stats)

enable_testing()

add_executable(ninja-build-stats)

target_sources(ninja-build-stats PRIVATE
	src/main.cpp
)

if (BUILD_TESTING)
	add_subdirectory(test)
endif()
