###############################################################################
# GENERAL CONFIGURATIONS
###############################################################################
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out_tests/${CMAKE_BUILD_TYPE}")

###############################################################################
# CUSTOM TARGETS
###############################################################################

# all_tests target must be available to group all the defined tests together
# add_clove_test assumes all_tests target is already defined so please
# do not change or remove this line
add_custom_target(all_tests)

###############################################################################
# TESTS (DO NOT CHANGE ANYTHING ABOVE, ADD YOUR TESTS/CUSTOM TARGETS BELOW)
# If you need to add any other group other than all_tests
# first `add_custom_target(my_test_group)`
# then you can pass it as the second argument to the `add_clove_test`
# otherwise don't forget to pass "" for Group and for dependencies if there
# are not any like `add_clove_test(test_something "" "")`
###############################################################################

add_clove_test(test_math_utils "" "../src/math_utils.c")
