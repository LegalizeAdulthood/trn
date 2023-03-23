#include <gtest/gtest.h>

#include "common.h"
#include "head.h"

#include <iterator>

struct header_line_type_values
{
    const char *name;
    header_line_type type;
    int value;
};

#define ENUM_VALUE(enum_, value_) { #enum_, enum_, value_ }

header_line_type_values values[] =
{
    // clang-format off
    ENUM_VALUE(PAST_HEADER, 0),
    ENUM_VALUE(SHOWN_LINE, 1),
    ENUM_VALUE(HIDDEN_LINE, 2),
    ENUM_VALUE(CUSTOM_LINE, 3),
    ENUM_VALUE(SOME_LINE, 4),
    ENUM_VALUE(HEAD_FIRST, 4), // same as SOME_LINE
    ENUM_VALUE(AUTHOR_LINE, 5),
    ENUM_VALUE(BYTES_LINE, 6),
    ENUM_VALUE(CONTNAME_LINE, 7),
    ENUM_VALUE(CONTDISP_LINE, 8),
    ENUM_VALUE(CONTLEN_LINE, 9),
    ENUM_VALUE(CONTXFER_LINE, 10),
    ENUM_VALUE(CONTTYPE_LINE, 11),
    ENUM_VALUE(DIST_LINE, 12),
    ENUM_VALUE(DATE_LINE, 13),
    ENUM_VALUE(EXPIR_LINE, 14),
    ENUM_VALUE(FOLLOW_LINE, 15),
    ENUM_VALUE(FROM_LINE, 16),
    ENUM_VALUE(INREPLY_LINE, 17),
    ENUM_VALUE(KEYW_LINE, 18),
    ENUM_VALUE(LINES_LINE, 19),
    ENUM_VALUE(MIMEVER_LINE, 20),
    ENUM_VALUE(MSGID_LINE, 21),
    ENUM_VALUE(NGS_LINE, 22),
    ENUM_VALUE(PATH_LINE, 23),
    ENUM_VALUE(RVER_LINE, 24),
    ENUM_VALUE(REPLY_LINE, 25),
    ENUM_VALUE(REFS_LINE, 26),
    ENUM_VALUE(SUMRY_LINE, 27),
    ENUM_VALUE(SUBJ_LINE, 28),
    ENUM_VALUE(XREF_LINE, 29),
    ENUM_VALUE(HEAD_LAST, 30),
    // clang-format on
};

void PrintTo(const header_line_type_values &param, std::ostream *str)
{
    *str << param.name << " <=> " << param.value;
}

class HeaderLineTypes : public testing::TestWithParam<header_line_type_values>
{
};

TEST_P(HeaderLineTypes, orderRelationships)
{
    const header_line_type_values param = GetParam();

    ASSERT_EQ(param.value, param.type);
}

INSTANTIATE_TEST_SUITE_P(TestHeaderLineTypes, HeaderLineTypes, testing::ValuesIn(values));
