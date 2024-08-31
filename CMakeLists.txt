###############################################################################
# GENERAL CONFIGURATIONS AND UTILITIES
###############################################################################
cmake_minimum_required(VERSION 3.20)
set(CMAKE_C_STANDARD 11)
include(cmake/utils.cmake)

###############################################################################
# PROJECT SETUP
###############################################################################
project(SimpleCProject LANGUAGES C)
include_directories(include src extern)
include(cmake/configs.cmake)
add_subdirectory(tests)

###############################################################################
# EXECUTABLE TARGETS
###############################################################################
add_executable(main_exe_file 
    src/main.c
    src/math_utils.c
)

###############################################################################
# PRE BUILD TESTS
###############################################################################

enable_pre_build_testing(main_exe_file all_tests)
