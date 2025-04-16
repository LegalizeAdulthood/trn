#include <gtest/gtest.h>

#include "config/common.h"
#include "trn/color.h"
#include "trn/opt.h"
#include "trn/head.h"

template <typename Enum>
struct EnumValues
{
    const char *name;
    Enum type;
    int value;
};

using HeaderLineTypeValues = EnumValues<HeaderLineType>;

#define ENUM_VALUE(enum_, value_) { #enum_, enum_, value_ }

HeaderLineTypeValues header_line_types[] =
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
    ENUM_VALUE(NEWSGROUPS_LINE, 22),
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

template <typename T>
void PrintTo(const EnumValues<T> &param, std::ostream *str)
{
    *str << param.name << " <=> " << param.value;
}

class HeaderLineTypes : public testing::TestWithParam<HeaderLineTypeValues>
{
};

TEST_P(HeaderLineTypes, orderRelationships)
{
    const HeaderLineTypeValues param = GetParam();

    ASSERT_EQ(param.value, param.type);
}

INSTANTIATE_TEST_SUITE_P(TestHeaderLineTypes, HeaderLineTypes, testing::ValuesIn(header_line_types));

using object_number_values = EnumValues<ObjectNumber>;

object_number_values object_numbers[] =
{
    // clang-format off
    ENUM_VALUE(COLOR_DEFAULT, 0),
    ENUM_VALUE(COLOR_NGNAME, 1),
    ENUM_VALUE(COLOR_PLUS, 2),
    ENUM_VALUE(COLOR_MINUS, 3),
    ENUM_VALUE(COLOR_STAR, 4),
    ENUM_VALUE(COLOR_HEADER, 5),
    ENUM_VALUE(COLOR_SUBJECT, 6),
    ENUM_VALUE(COLOR_TREE, 7),
    ENUM_VALUE(COLOR_TREE_MARK, 8),
    ENUM_VALUE(COLOR_MORE, 9),
    ENUM_VALUE(COLOR_HEADING, 10),
    ENUM_VALUE(COLOR_CMD, 11),
    ENUM_VALUE(COLOR_MOUSE, 12),
    ENUM_VALUE(COLOR_NOTICE, 13),
    ENUM_VALUE(COLOR_SCORE, 14),
    ENUM_VALUE(COLOR_ARTLINE1, 15),
    ENUM_VALUE(COLOR_MIMESEP, 16),
    ENUM_VALUE(COLOR_MIMEDESC, 17),
    ENUM_VALUE(COLOR_CITEDTEXT, 18),
    ENUM_VALUE(COLOR_BODYTEXT, 19),
    ENUM_VALUE(MAX_COLORS, 20),
    // clang-format on
};

class ObjectNumbers : public testing::TestWithParam<object_number_values>
{
};

TEST_P(ObjectNumbers, orderRelationships)
{
    const object_number_values param = GetParam();

    ASSERT_EQ(param.value, param.type);
}

INSTANTIATE_TEST_SUITE_P(TestObjectNumbers, ObjectNumbers, testing::ValuesIn(object_numbers));

using display_option_values = EnumValues<OptionIndex>;

display_option_values display_options[] =
{
    // clang-format off
    ENUM_VALUE(OI_TERSE_OUTPUT,                 2),
    ENUM_VALUE(OI_PAGER_LINE_MARKING,           3),
    ENUM_VALUE(OI_ERASE_SCREEN,                 4),
    ENUM_VALUE(OI_ERASE_EACH_LINE,              5),
    ENUM_VALUE(OI_MUCK_UP_CLEAR,                6),
    ENUM_VALUE(OI_BKGND_SPINNER,                7),
    ENUM_VALUE(OI_CHARSET,                      8),
    ENUM_VALUE(OI_FILTER_CONTROL_CHARACTERS,    9),

    ENUM_VALUE(OI_USE_UNIV_SEL,                 11),
    ENUM_VALUE(OI_UNIV_SEL_ORDER,               12),
    ENUM_VALUE(OI_UNIV_FOLLOW,                  13),
    ENUM_VALUE(OI_UNIV_SEL_CMDS,                14),
    ENUM_VALUE(OI_USE_NEWSRC_SEL,               15),
    ENUM_VALUE(OI_NEWSRC_SEL_CMDS,              16),
    ENUM_VALUE(OI_USE_ADD_SEL,                  17),
    ENUM_VALUE(OI_ADD_SEL_CMDS,                 18),
    ENUM_VALUE(OI_USE_NEWSGROUP_SEL,            19),
    ENUM_VALUE(OI_NEWSGROUP_SEL_ORDER,          20),
    ENUM_VALUE(OI_NEWSGROUP_SEL_CMDS,           21),
    ENUM_VALUE(OI_NEWSGROUP_SEL_STYLES,         22),
    ENUM_VALUE(OI_USE_NEWS_SEL,                 23),
    ENUM_VALUE(OI_NEWS_SEL_MODE,                24),
    ENUM_VALUE(OI_NEWS_SEL_ORDER,               25),
    ENUM_VALUE(OI_NEWS_SEL_CMDS,                26),
    ENUM_VALUE(OI_NEWS_SEL_STYLES,              27),
    ENUM_VALUE(OI_OPTION_SEL_CMDS,              28),
    ENUM_VALUE(OI_USE_SEL_NUM,                  29),
    ENUM_VALUE(OI_SEL_NUM_GOTO,                 30),

    ENUM_VALUE(OI_USE_THREADS,                  32),
    ENUM_VALUE(OI_SELECT_MY_POSTS,              33),
    ENUM_VALUE(OI_INITIAL_ARTICLE_LINES,        34),
    ENUM_VALUE(OI_ARTICLE_TREE_LINES,           35),
    ENUM_VALUE(OI_WORD_WRAP_MARGIN,             36),
    ENUM_VALUE(OI_AUTO_GROW_GROUPS,             37),
    ENUM_VALUE(OI_COMPRESS_SUBJECTS,            38),
    ENUM_VALUE(OI_JOIN_SUBJECT_LINES,           39),
    ENUM_VALUE(OI_GOTO_LINE_NUM,                40),
    ENUM_VALUE(OI_IGNORE_THRU_ON_SELECT,        41),
    ENUM_VALUE(OI_READ_BREADTH_FIRST,           42),
    ENUM_VALUE(OI_BKGND_THREADING,              43),
    ENUM_VALUE(OI_SCANMODE_COUNT,               44),
    ENUM_VALUE(OI_HEADER_MAGIC,                 45),
    ENUM_VALUE(OI_HEADER_HIDING,                46),

    ENUM_VALUE(OI_CITED_TEXT_STRING,            48),

    ENUM_VALUE(OI_SAVE_DIR,                     50),
    ENUM_VALUE(OI_AUTO_SAVE_NAME,               51),
    ENUM_VALUE(OI_SAVEFILE_TYPE,                52),

    ENUM_VALUE(OI_USE_MOUSE,                    54),
    ENUM_VALUE(OI_MOUSE_MODES,                  55),
    ENUM_VALUE(OI_UNIV_SEL_BTNS,                56),
    ENUM_VALUE(OI_NEWSRC_SEL_BTNS,              57),
    ENUM_VALUE(OI_ADD_SEL_BTNS,                 58),
    ENUM_VALUE(OI_NEWSGROUP_SEL_BTNS,           59),
    ENUM_VALUE(OI_NEWS_SEL_BTNS,                60),
    ENUM_VALUE(OI_OPTION_SEL_BTNS,              61),
    ENUM_VALUE(OI_ART_PAGER_BTNS,               62),

    ENUM_VALUE(OI_MULTIPART_SEPARATOR,          64),
    ENUM_VALUE(OI_AUTO_VIEW_INLINE,             65),

    ENUM_VALUE(OI_NEWGROUP_CHECK,               67),
    ENUM_VALUE(OI_RESTRICTION_INCLUDES_EMPTIES, 68),
    ENUM_VALUE(OI_APPEND_UNSUBSCRIBED_GROUPS,   69),
    ENUM_VALUE(OI_INITIAL_GROUP_LIST,           70),
    ENUM_VALUE(OI_RESTART_AT_LAST_GROUP,        71),
    ENUM_VALUE(OI_EAT_TYPEAHEAD,                72),
    ENUM_VALUE(OI_VERIFY_INPUT,                 73),
    ENUM_VALUE(OI_FUZZY_NEWSGROUP_NAMES,        74),
    ENUM_VALUE(OI_AUTO_ARROW_MACROS,            75),
    ENUM_VALUE(OI_CHECKPOINT_NEWSRC_FREQUENCY,  76),
    ENUM_VALUE(OI_DEFAULT_REFETCH_TIME,         77),
    ENUM_VALUE(OI_NOVICE_DELAYS,                78),
    ENUM_VALUE(OI_OLD_MTHREADS_DATABASE,        79),

    ENUM_VALUE(OI_TRN_LAST,                     79),

    ENUM_VALUE(OI_SCANA_FOLLOW,                 81),
    ENUM_VALUE(OI_SCANA_FOLD,                   82),
    ENUM_VALUE(OI_SCANA_UNZOOMFOLD,             83),
    ENUM_VALUE(OI_SCANA_MARKSTAY,               84),
    ENUM_VALUE(OI_SCAN_VI,                      85),
    ENUM_VALUE(OI_SCAN_ITEMNUM,                 86),
    ENUM_VALUE(OI_SCANA_DISPANUM,               87),
    ENUM_VALUE(OI_SCANA_DISPAUTHOR,             88),
    ENUM_VALUE(OI_SCANA_DISPSCORE,              89),
    ENUM_VALUE(OI_SCANA_DISPSUBCNT,             90),
    ENUM_VALUE(OI_SCANA_DISPSUBJ,               91),
    ENUM_VALUE(OI_SCANA_DISPSUMMARY,            92),
    ENUM_VALUE(OI_SCANA_DISPKEYW,               93),

    ENUM_VALUE(OI_SCAN_LAST,                    93),

    ENUM_VALUE(OI_SC_VERBOSE,                   95),

    ENUM_VALUE(OI_SCORE_LAST,                   95),
    // clang-format on
};

class DisplayOptions : public testing::TestWithParam<display_option_values>
{
};

TEST_P(DisplayOptions, orderRelationships)
{
    const display_option_values param = GetParam();

    ASSERT_EQ(param.value, param.type);
}

INSTANTIATE_TEST_SUITE_P(TestDisplayOptions, DisplayOptions, testing::ValuesIn(display_options));
