###############################################################################
# GENERAL CONFIGURATIONS AND UTILITIES
###############################################################################
cmake_minimum_required(VERSION 3.20)
set(CMAKE_C_STANDARD 11)
include(cmake/utils.cmake)

###############################################################################
# PROJECT SETUP
###############################################################################
project(CLoxCompiler LANGUAGES C)
include_directories(include src extern)
include(cmake/configs.cmake)
add_subdirectory(tests)

###############################################################################
# EXECUTABLE TARGETS
###############################################################################
add_executable(clox
    src/main.c
    src/chunk.c
    src/memory.c
    src/debug.c
    src/value.c
    src/vm.c
    src/compiler.c
    src/scanner.c
    src/object.c
    src/table.c
)

option(ENABLE_NAN_BOXING "Enable NAN_BOXING" ON)
option(ENABLE_DEBUG_PRINT_CODE "Enable DEBUG_PRINT_CODE" OFF)
option(ENABLE_DEBUG_TRACE_EXECUTION "Enable DEBUG_TRACE_EXECUTION" OFF)
option(ENABLE_DEBUG_STRESS_GC "Enable DEBUG_STRESS_GC" ON)
option(ENABLE_DEBUG_LOG_GC "Enable DEBUG_LOG_GC" OFF)

if(ENABLE_NAN_BOXING)
    target_compile_definitions(clox PRIVATE NAN_BOXING)
endif()

if(ENABLE_DEBUG_PRINT_CODE)
    target_compile_definitions(clox PRIVATE DEBUG_PRINT_CODE)
endif()

if(ENABLE_DEBUG_TRACE_EXECUTION)
    target_compile_definitions(clox PRIVATE DEBUG_TRACE_EXECUTION)
endif()

if(ENABLE_DEBUG_STRESS_GC)
    target_compile_definitions(clox PRIVATE DEBUG_STRESS_GC)
endif()

if(ENABLE_DEBUG_LOG_GC)
    target_compile_definitions(clox PRIVATE DEBUG_LOG_GC)
endif()


###############################################################################
# PRE BUILD TESTS
###############################################################################

# enable_pre_build_testing(main_exe_file all_tests)
