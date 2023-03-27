#include <gtest/gtest.h>

#include "common.h"
#include "datasrc.h"

#include <addng.h>
#include <cache.h>

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

TEST(FieldFlags, bitwiseOr)
{
    const field_flags val = FF_NONE | FF_HAS_FIELD;

    EXPECT_EQ(FF_HAS_FIELD, val);
}

TEST(FieldFlags, bitwiseAnd)
{
    const field_flags val = FF_NONE & FF_HAS_FIELD;

    EXPECT_EQ(FF_NONE, val);
}

TEST(FieldFlags, bitwiseXorSet)
{
    const field_flags val = FF_NONE ^ FF_HAS_FIELD;

    EXPECT_EQ(FF_HAS_FIELD, val);
}

TEST(FieldFlags, bitwiseXorClear)
{
    const field_flags val = FF_HAS_FIELD ^ FF_HAS_FIELD;

    EXPECT_EQ(FF_NONE, val);
}

TEST(FieldFlags, bitwiseNot)
{
    const field_flags val = ~FF_HAS_FIELD;

    EXPECT_NE(FF_NONE, val);
    EXPECT_EQ(FF_NONE, val & FF_HAS_FIELD);
}

TEST(FieldFlags, bitwiseOrEqual)
{
    field_flags val = FF_NONE;

    val |= FF_HAS_FIELD;

    EXPECT_EQ(FF_HAS_FIELD, val);
}

TEST(FieldFlags, bitwiseAndEqual)
{
    field_flags val = FF_HAS_FIELD | FF_CHECK4FIELD;

    val &= FF_HAS_FIELD;

    EXPECT_EQ(FF_HAS_FIELD, val);
}

TEST(FieldFlags, bitwiseXorEqual)
{
    field_flags val = FF_HAS_FIELD | FF_CHECK4FIELD;

    val ^= FF_HAS_FIELD;

    EXPECT_EQ(FF_CHECK4FIELD, val);
}

namespace
{

template <typename T>
struct addgroup_flag_equivalence
{
    addgroup_flags agf;
    T              other;
};

using addgroup_subject_flag = addgroup_flag_equivalence<subject_flags>;
using addgroup_article_flag = addgroup_flag_equivalence<article_flags>;

struct SubjectAddGroupFlagEquivalences : testing::TestWithParam<addgroup_subject_flag>
{
};

struct ArticleAddGroupFlagEquivalences : testing::TestWithParam<addgroup_article_flag>
{
};

} // namespace

static addgroup_subject_flag subject_equivs[] =
{
    { AGF_NONE, SF_NONE },
    { AGF_DEL, SF_DEL },
    { AGF_SEL, SF_SEL },
    { AGF_DELSEL, SF_DELSEL }
};

TEST_P(SubjectAddGroupFlagEquivalences, sameValue)
{
    addgroup_subject_flag param = GetParam();

    ASSERT_EQ(static_cast<int>(param.agf), static_cast<int>(param.other));
}

INSTANTIATE_TEST_SUITE_P(TestSubjectAddGroupFlags, SubjectAddGroupFlagEquivalences, testing::ValuesIn(subject_equivs));

static addgroup_article_flag article_equivs[] =
{
    { AGF_NONE, AF_NONE },
    { AGF_DEL, AF_DEL },
    { AGF_SEL, AF_SEL },
    { AGF_DELSEL, AF_DELSEL }
};

TEST_P(ArticleAddGroupFlagEquivalences, sameValue)
{
    addgroup_article_flag param = GetParam();

    ASSERT_EQ(static_cast<int>(param.agf), static_cast<int>(param.other));
}

INSTANTIATE_TEST_SUITE_P(TestSubjectAddGroupFlags, ArticleAddGroupFlagEquivalences, testing::ValuesIn(article_equivs));
