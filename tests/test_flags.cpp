#include <gtest/gtest.h>

#include <config/common.h>

#include <trn/ngdata.h>
#include <trn/addng.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/rcstuff.h>
#include <trn/univ.h>

TEST(DataSrcFlags, bitwiseOr)
{
    const DataSourceFlags val = DF_NONE | DF_ADD_OK;

    EXPECT_EQ(DF_ADD_OK, val);
}

TEST(DataSrcFlags, bitwiseAnd)
{
    const DataSourceFlags val = DF_NONE & DF_ADD_OK;

    EXPECT_EQ(DF_NONE, val);
}

TEST(DataSrcFlags, bitwiseXorSet)
{
    const DataSourceFlags val = DF_NONE ^ DF_ADD_OK;

    EXPECT_EQ(DF_ADD_OK, val);
}

TEST(DataSrcFlags, bitwiseXorClear)
{
    const DataSourceFlags val = DF_ADD_OK ^ DF_ADD_OK;

    EXPECT_EQ(DF_NONE, val);
}

TEST(DataSrcFlags, bitwiseNot)
{
    const DataSourceFlags val = ~DF_ADD_OK;

    EXPECT_NE(DF_NONE, val);
    EXPECT_EQ(DF_NONE, val & DF_ADD_OK);
}

TEST(DataSrcFlags, bitwiseOrEqual)
{
    DataSourceFlags val = DF_NONE;

    val |= DF_ADD_OK;

    EXPECT_EQ(DF_ADD_OK, val);
}

TEST(DataSrcFlags, bitwiseAndEqual)
{
    DataSourceFlags val = DF_ADD_OK | DF_DEFAULT;

    val &= DF_ADD_OK;

    EXPECT_EQ(DF_ADD_OK, val);
}

TEST(DataSrcFlags, bitwiseXorEqual)
{
    DataSourceFlags val = DF_ADD_OK | DF_DEFAULT;

    val ^= DF_ADD_OK;

    EXPECT_EQ(DF_DEFAULT, val);
}

TEST(FieldFlags, bitwiseOr)
{
    const FieldFlags val = FF_NONE | FF_HAS_FIELD;

    EXPECT_EQ(FF_HAS_FIELD, val);
}

TEST(FieldFlags, bitwiseAnd)
{
    const FieldFlags val = FF_NONE & FF_HAS_FIELD;

    EXPECT_EQ(FF_NONE, val);
}

TEST(FieldFlags, bitwiseXorSet)
{
    const FieldFlags val = FF_NONE ^ FF_HAS_FIELD;

    EXPECT_EQ(FF_HAS_FIELD, val);
}

TEST(FieldFlags, bitwiseXorClear)
{
    const FieldFlags val = FF_HAS_FIELD ^ FF_HAS_FIELD;

    EXPECT_EQ(FF_NONE, val);
}

TEST(FieldFlags, bitwiseNot)
{
    const FieldFlags val = ~FF_HAS_FIELD;

    EXPECT_NE(FF_NONE, val);
    EXPECT_EQ(FF_NONE, val & FF_HAS_FIELD);
}

TEST(FieldFlags, bitwiseOrEqual)
{
    FieldFlags val = FF_NONE;

    val |= FF_HAS_FIELD;

    EXPECT_EQ(FF_HAS_FIELD, val);
}

TEST(FieldFlags, bitwiseAndEqual)
{
    FieldFlags val = FF_HAS_FIELD | FF_CHECK_FOR_FIELD;

    val &= FF_HAS_FIELD;

    EXPECT_EQ(FF_HAS_FIELD, val);
}

TEST(FieldFlags, bitwiseXorEqual)
{
    FieldFlags val = FF_HAS_FIELD | FF_CHECK_FOR_FIELD;

    val ^= FF_HAS_FIELD;

    EXPECT_EQ(FF_CHECK_FOR_FIELD, val);
}

namespace
{

template <typename T>
struct AddGroupFlagEquivalence
{
    AddGroupFlags agf;
    T              other;
};

using AddGroupSubjectFlag = AddGroupFlagEquivalence<SubjectFlags>;
using AddGroupArticleFlag = AddGroupFlagEquivalence<ArticleFlags>;
using AddGroupNewsgroupFlag = AddGroupFlagEquivalence<NewsgroupFlags>;
using AddGroupMultircFlag = AddGroupFlagEquivalence<MultircFlags>;
using AddGroupUniversalItemFlag = AddGroupFlagEquivalence<UniversalItemFlags>;

struct SubjectAddGroupFlagEquivalences : testing::TestWithParam<AddGroupSubjectFlag>
{
};

struct ArticleAddGroupFlagEquivalences : testing::TestWithParam<AddGroupArticleFlag>
{
};

struct NewsgroupAddGroupFlagEquivalence : testing::TestWithParam<AddGroupNewsgroupFlag>
{
};

struct MultiRCAddGroupFlagEquivalence : testing::TestWithParam<AddGroupMultircFlag>
{
};

struct UnivItemAddGroupFlagEquivalence : testing::TestWithParam<AddGroupUniversalItemFlag>
{
};

} // namespace

static AddGroupSubjectFlag subject_equivs[] =
{
    // clang-format off
    { AGF_NONE, SF_NONE },
    { AGF_DEL, SF_DEL },
    { AGF_SEL, SF_SEL },
    { AGF_DEL_SEL, SF_DEL_SEL }
    // clang-format on
};

TEST_P(SubjectAddGroupFlagEquivalences, sameValue)
{
    AddGroupSubjectFlag param = GetParam();

    ASSERT_EQ(static_cast<int>(param.agf), static_cast<int>(param.other));
}

INSTANTIATE_TEST_SUITE_P(TestSubjectAddGroupFlags, SubjectAddGroupFlagEquivalences, testing::ValuesIn(subject_equivs));

static AddGroupArticleFlag article_equivs[] =
{
    // clang-format off
    { AGF_NONE, AF_NONE },
    { AGF_DEL, AF_DEL },
    { AGF_SEL, AF_SEL },
    { AGF_DEL_SEL, AF_DEL_SEL }
    // clang-format on
};

TEST_P(ArticleAddGroupFlagEquivalences, sameValue)
{
    AddGroupArticleFlag param = GetParam();

    ASSERT_EQ(static_cast<int>(param.agf), static_cast<int>(param.other));
}

INSTANTIATE_TEST_SUITE_P(TestArticleAddGroupFlags, ArticleAddGroupFlagEquivalences, testing::ValuesIn(article_equivs));

static AddGroupNewsgroupFlag newsgroup_equivs[] =
{
    // clang-format off
    { AGF_NONE, NF_NONE },
    { AGF_DEL, NF_DEL },
    { AGF_SEL, NF_SEL },
    { AGF_DEL_SEL, NF_DELSEL }
    // clang-format on
};

TEST_P(NewsgroupAddGroupFlagEquivalence, sameValue)
{
    AddGroupNewsgroupFlag param = GetParam();

    ASSERT_EQ(static_cast<int>(param.agf), static_cast<int>(param.other));
}

INSTANTIATE_TEST_SUITE_P(TestNewsgroupAddGroupFlags, NewsgroupAddGroupFlagEquivalence, testing::ValuesIn(newsgroup_equivs));

static AddGroupMultircFlag multirc_equivs[] =
{
    // clang-format off
    { AGF_NONE, MF_NONE },
    // { AGF_DEL, MF_DEL },
    { AGF_SEL, MF_SEL },
    // { AGF_DELSEL, MNF_DELSEL }
    // clang-format on
};

TEST_P(MultiRCAddGroupFlagEquivalence, sameValue)
{
    AddGroupMultircFlag param = GetParam();

    ASSERT_EQ(static_cast<int>(param.agf), static_cast<int>(param.other));
}

INSTANTIATE_TEST_SUITE_P(TestMultiRCAddGroupFlags, MultiRCAddGroupFlagEquivalence, testing::ValuesIn(multirc_equivs));

static AddGroupUniversalItemFlag univitem_equivs[] =
{
    // clang-format off
    { AGF_NONE, UF_NONE },
    { AGF_DEL, UF_DEL },
    { AGF_SEL, UF_SEL },
    { AGF_DEL_SEL, UF_DELSEL }
    // clang-format on
};

TEST_P(UnivItemAddGroupFlagEquivalence, sameValue)
{
    AddGroupUniversalItemFlag param = GetParam();

    ASSERT_EQ(static_cast<int>(param.agf), static_cast<int>(param.other));
}

INSTANTIATE_TEST_SUITE_P(TestUnivItemAddGroupFlags, UnivItemAddGroupFlagEquivalence, testing::ValuesIn(univitem_equivs));
