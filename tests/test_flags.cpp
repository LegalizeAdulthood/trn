#include <gtest/gtest.h>

#include "common.h"
#include "datasrc.h"

TEST(DataSrcFlags, bitwiseOr)
{
    const datasrc_flags val = DF_NONE | DF_ADD_OK;

    EXPECT_EQ(DF_ADD_OK, val);
}

TEST(DataSrcFlags, bitwiseAnd)
{
    const datasrc_flags val = DF_NONE & DF_ADD_OK;

    EXPECT_EQ(DF_NONE, val);
}

TEST(DataSrcFlags, bitwiseXorSet)
{
    const datasrc_flags val = DF_NONE ^ DF_ADD_OK;

    EXPECT_EQ(DF_ADD_OK, val);
}

TEST(DataSrcFlags, bitwiseXorClear)
{
    const datasrc_flags val = DF_ADD_OK ^ DF_ADD_OK;

    EXPECT_EQ(DF_NONE, val);
}

TEST(DataSrcFlags, bitwiseNot)
{
    const datasrc_flags val = ~DF_ADD_OK;

    EXPECT_NE(DF_NONE, val);
    EXPECT_EQ(DF_NONE, val & DF_ADD_OK);
}

TEST(DataSrcFlags, bitwiseOrEqual)
{
    datasrc_flags val = DF_NONE;

    val |= DF_ADD_OK;

    EXPECT_EQ(DF_ADD_OK, val);
}

TEST(DataSrcFlags, bitwiseAndEqual)
{
    datasrc_flags val = DF_ADD_OK | DF_DEFAULT;

    val &= DF_ADD_OK;

    EXPECT_EQ(DF_ADD_OK, val);
}

TEST(DataSrcFlags, bitwiseXorEqual)
{
    datasrc_flags val = DF_ADD_OK | DF_DEFAULT;

    val ^= DF_ADD_OK;

    EXPECT_EQ(DF_DEFAULT, val);
}
