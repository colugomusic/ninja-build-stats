cmake_minimum_required(VERSION 4.3.1)
project(ninja-build-stats-test)

add_executable(ninja-build-stats-test)

target_sources(ninja-build-stats-test PRIVATE
	src/main.cpp
)
