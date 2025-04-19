#include <config/StrongInt.h>

#include <gtest/gtest.h>

TEST(TestStrongInt, defaultConstructsToZero)
{
    const StrongInt<int> value;

    EXPECT_EQ(0, value.num);
}

TEST(TestStrongInt, constructFromValue)
{
    const StrongInt<int> value{10};

    EXPECT_EQ(10, value.num);
}

TEST(TestStrongInt, compareEqual)
{
    const StrongInt<int> lhs{1};
    const StrongInt<int> rhs{1};

    EXPECT_TRUE(lhs == rhs);
}

TEST(TestStrongInt, compareDifferent)
{
    const StrongInt<int> lhs{1};
    const StrongInt<int> rhs{2};

    EXPECT_TRUE(lhs != rhs);
}

TEST(TestStrongInt, compareLess)
{
    const StrongInt<int> lhs{1};
    const StrongInt<int> rhs{2};

    EXPECT_TRUE(lhs < rhs);
}

TEST(TestStrongInt, compareGreater)
{
    const StrongInt<int> lhs{2};
    const StrongInt<int> rhs{1};

    EXPECT_TRUE(lhs > rhs);
}

TEST(TestStrongInt, compareLessEqual)
{
    const StrongInt<int> lhs{1};
    const StrongInt<int> rhs{2};

    EXPECT_TRUE(lhs <= rhs);
    EXPECT_TRUE(lhs <= lhs);
}

TEST(TestStrongInt, compareGreaterEqual)
{
    const StrongInt<int> lhs{2};
    const StrongInt<int> rhs{1};

    EXPECT_TRUE(lhs >= rhs);
    EXPECT_TRUE(lhs >= lhs);
}

TEST(TestStrongInt, add)
{
    const StrongInt<int> lhs{2};
    const StrongInt<int> rhs{1};

    const StrongInt<int> sum = lhs + rhs;

    EXPECT_EQ(lhs.num + rhs.num, sum.num);
}

TEST(TestStrongInt, addPrimitiveType)
{
    const StrongInt<int> lhs{2};
    const int rhs{1};

    const StrongInt<int> sum = lhs + rhs;

    EXPECT_EQ(lhs.num + rhs, sum.num);
}

TEST(TestStrongInt, subtract)
{
    const StrongInt<int> lhs{2};
    const StrongInt<int> rhs{1};

    const StrongInt<int> sum = lhs - rhs;

    EXPECT_EQ(lhs.num - rhs.num, sum.num);
}

TEST(TestStrongInt, subtractPrimitiveType)
{
    const StrongInt<int> lhs{2};
    const int rhs{1};

    const StrongInt<int> sum = lhs - rhs;

    EXPECT_EQ(lhs.num - rhs, sum.num);
}
