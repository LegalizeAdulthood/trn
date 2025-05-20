#include <wildmat/wildmat.h>

#include <gtest/gtest.h>

TEST(TestWildMat, exactMatch)
{
    EXPECT_TRUE(wildcard_match("alt.flame", "alt.flame"));
}

TEST(TestWildMat, fontMatch)
{
    EXPECT_TRUE(
        wildcard_match("-adobe-courier-bold-o-normal--12-120-75-75-m-70-iso8859-1", "-*-*-*-*-*-*-12-*-*-*-m-*-*-*"));
}

TEST(TestWildMat, fontMisMatch)
{
    EXPECT_FALSE(
        wildcard_match("-adobe-courier-bold-o-normal--12-120-75-75-X-70-iso8859-1", "-*-*-*-*-*-*-12-*-*-*-m-*-*-*"));
}
