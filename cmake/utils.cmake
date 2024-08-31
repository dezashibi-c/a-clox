function(add_clove_test TEST_NAME GROUP DEPS)
    add_executable(${TEST_NAME} tests.c ${TEST_NAME}.c ${DEPS})

    target_include_directories(${TEST_NAME} PRIVATE ../extern/clove-unit)

    add_test(NAME ${TEST_NAME} COMMAND $<TARGET_FILE:${TEST_NAME}> -x)

    add_dependencies(all_tests ${TEST_NAME})

    if(NOT GROUP STREQUAL "")
        add_dependencies(${GROUP} ${TEST_NAME})
    endif()
endfunction()

function(enable_pre_build_testing TARGET_NAME GROUP)
    add_dependencies(${TARGET_NAME} ${GROUP})
    add_custom_target(pre_build_tests_for_${TARGET_NAME}
        COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIG> --output-on-failure --verbose
        DEPENDS ${GROUP}

        COMMENT "Pre build tests for `${TARGET_NAME}`..."
    )
    add_dependencies(${TARGET_NAME} pre_build_tests_for_${TARGET_NAME})
endfunction()
