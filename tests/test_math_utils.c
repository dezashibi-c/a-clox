#define CLOVE_SUITE_NAME MathUtilTests

#include "clove-unit/clove-unit.h"
#include "math_utils.h"

CLOVE_TEST(Addition)
{
    int result = add(2, 3);

    CLOVE_INT_EQ(result, 5);
}

CLOVE_TEST(Subtraction)
{
    int result = subtract(5, 3);

    CLOVE_INT_EQ(result, 2);
}
