/* OptionCatalog.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/OptionCatalog.h>

namespace
{

IniField option_field(OptionIndex index, std::string_view name, std::string_view help)
{
    return IniField::value(static_cast<int>(index), name, help);
}

} // namespace

OptionCatalog::OptionCatalog() :
    m_schema{"OPTIONS",
             {
                 IniField::group("Display Options"),
                 option_field(OI_TERSE_OUTPUT, "Terse Output", "yes/no"),
                 option_field(OI_PAGER_LINE_MARKING, "Pager Line-Marking", "standout/underline/no"),
                 option_field(OI_ERASE_SCREEN, "Erase Screen", "yes/no"),
                 option_field(OI_ERASE_EACH_LINE, "Erase Each Line", "yes/no"),
                 option_field(OI_MUCK_UP_CLEAR, "Muck Up Clear", "yes/no"),
                 option_field(OI_BACKGROUND_SPINNER, "Background Spinner", "yes/no"),
                 option_field(OI_CHARSET, "Charset", "<e.g. patm>"),
                 option_field(OI_FILTER_CONTROL_CHARACTERS, "Filter Control Characters", "yes/no"),

                 IniField::group("Selector Options"),
                 option_field(OI_USE_UNIV_SEL, "Use Universal Selector", "yes/no"),
                 option_field(OI_UNIV_SEL_ORDER, "Universal Selector Order", "natural/points"),
                 option_field(OI_UNIV_FOLLOW, "Universal Selector article follow", "yes/no"),
                 option_field(OI_UNIV_SEL_CMDS, "Universal Selector Commands", "<Last-page-cmd><Other-page-cmd>"),
                 option_field(OI_USE_NEWSRC_SEL, "Use Newsrc Selector", "yes/no"),
                 option_field(OI_NEWSRC_SEL_CMDS, "Newsrc Selector Commands", "<Last-page-cmd><Other-page-cmd>"),
                 option_field(OI_USE_ADD_SEL, "Use Addgroup Selector", "yes/no"),
                 option_field(OI_ADD_SEL_CMDS, "Addgroup Selector Commands", "<Last-page-cmd><Other-page-cmd>"),
                 option_field(OI_USE_NEWSGROUP_SEL, "Use Newsgroup Selector", "yes/no"),
                 option_field(OI_NEWSGROUP_SEL_ORDER, "Newsgroup Selector Order", "natural/group/count"),
                 option_field(OI_NEWSGROUP_SEL_CMDS, "Newsgroup Selector Commands", "<Last-page-cmd><Other-page-cmd>"),
                 option_field(OI_NEWSGROUP_SEL_STYLES, "Newsgroup Selector Display Styles", "<e.g. slm=short/long/med>"),
                 option_field(OI_USE_NEWS_SEL, "Use News Selector", "yes/no/<# articles>"),
                 option_field(OI_NEWS_SEL_MODE, "News Selector Mode", "threads/subjects/articles"),
                 option_field(OI_NEWS_SEL_ORDER, "News Selector Order", "[reverse] date/subject/author/groups/cnt/points"),
                 option_field(OI_NEWS_SEL_CMDS, "News Selector Commands", "<Last-page-cmd><Other-page-cmd>"),
                 option_field(OI_NEWS_SEL_STYLES, "News Selector Display Styles", "<e.g. lms=long/medium/short>"),
                 option_field(OI_OPTION_SEL_CMDS, "Option Selector Commands", "<Last-page-cmd><Other-page-cmd>"),
                 option_field(OI_USE_SEL_NUM, "Use Selector Numbers", "yes/no"),
                 option_field(OI_SEL_NUM_GOTO, "Selector Number Auto-Goto", "yes/no"),

                 IniField::group("Newsreading Options"),
                 option_field(OI_USE_THREADS, "Use Threads", "yes/no"),
                 option_field(OI_SELECT_MY_POSTS, "Select My Postings", "subthread/parent/thread/no"),
                 option_field(OI_INITIAL_ARTICLE_LINES, "Initial Article Lines", "no/<# lines>"),
                 option_field(OI_ARTICLE_TREE_LINES, "Article Tree Lines", "no/<# lines>"),
                 option_field(OI_WORD_WRAP_MARGIN, "Word-Wrap Margin", "no/<# chars in margin>"),
                 option_field(OI_AUTO_GROW_GROUPS, "Auto-Grow Groups", "yes/no"),
                 option_field(OI_COMPRESS_SUBJECTS, "Compress Subjects", "yes/no"),
                 option_field(OI_JOIN_SUBJECT_LINES, "Join Subject Lines", "no/<# chars>"),
                 option_field(OI_GOTO_LINE_NUM, "Line Num for Goto", "<# line (1-n)>"),
                 option_field(OI_IGNORE_THRU_ON_SELECT, "Ignore THRU on Select", "yes/no"),
                 option_field(OI_READ_BREADTH_FIRST, "Read Breadth First", "yes/no"),
                 option_field(OI_BACKGROUND_THREADING, "Background Threading", "yes/no"),
                 option_field(OI_SCAN_MODE_COUNT, "Scan Mode Count", "no/<# articles>"),
                 option_field(OI_HEADER_MAGIC, "Header Magic", "<[!]header,...>"),
                 option_field(OI_HEADER_HIDING, "Header Hiding", "<[!]header,...>"),

                 IniField::group("Posting Options"),
                 option_field(OI_CITED_TEXT_STRING, "Cited Text String", "<e.g. '>'>"),

                 IniField::group("Save Options"),
                 option_field(OI_SAVE_DIR, "Save Dir", "<directory path>"),
                 option_field(OI_AUTO_SAVE_NAME, "Auto Savename", "yes/no"),
                 option_field(OI_SAVE_FILE_TYPE, "Default Savefile Type", "norm/mail/ask"),

                 IniField::group("Mouse Options"),
                 option_field(OI_USE_MOUSE, "Use XTerm Mouse", "yes/no"),
                 option_field(OI_MOUSE_MODES, "Mouse Modes", "<e.g. acjlptwK>"),
                 option_field(OI_UNIV_SEL_BTNS, "Universal Selector Mousebar", "<e.g. [PgUp]< [PgDn]> Z [Quit]q>"),
                 option_field(OI_NEWSRC_SEL_BTNS, "Newsrc Selector Mousebar", "<e.g. [PgUp]< [PgDn]> Z [Quit]q>"),
                 option_field(OI_ADD_SEL_BTNS, "Addgroup Selector Mousebar", "<e.g. [Top]^ [Bot]$ [ OK ]Z>"),
                 option_field(OI_NEWSGROUP_SEL_BTNS, "Newsgroup Selector Mousebar", "<e.g. [ OK ]Z [Quit]q [Help]?>"),
                 option_field(OI_NEWS_SEL_BTNS, "News Selector Mousebar", "<e.g. [KillPg]D [Read]^j [Quit]Q>"),
                 option_field(OI_OPTION_SEL_BTNS, "Option Selector Mousebar", "<e.g. [Save]S [Use]^I [Abandon]q>"),
                 option_field(OI_ART_PAGER_BTNS, "Article Pager Mousebar", "<e.g. [Next]n J [Sel]+ [Quit]q>"),

                 IniField::group("MIME Options"),
                 option_field(OI_MULTIPART_SEPARATOR, "Multipart Separator", "<string>"),
                 option_field(OI_AUTO_VIEW_INLINE, "Auto-View Inline", "yes/no"),

                 IniField::group("Misc Options"),
                 option_field(OI_NEW_GROUP_CHECK, "Check for New Groups", "yes/no"),
                 option_field(OI_RESTRICTION_INCLUDES_EMPTIES, "Restriction Includes Empty Groups", "yes/no"),
                 option_field(OI_APPEND_UNSUBSCRIBED_GROUPS, "Append Unsubscribed Groups", "yes/no"),
                 option_field(OI_INITIAL_GROUP_LIST, "Initial Group List", "no/<# groups>"),
                 option_field(OI_RESTART_AT_LAST_GROUP, "Restart At Last Group", "yes/no"),
                 option_field(OI_EAT_TYPEAHEAD, "Eat Type-Ahead", "yes/no"),
                 option_field(OI_VERIFY_INPUT, "Verify Input", "yes/no"),
                 option_field(OI_FUZZY_NEWSGROUP_NAMES, "Fuzzy Newsgroup Names", "yes/no"),
                 option_field(OI_AUTO_ARROW_MACROS, "Auto Arrow Macros", "regular/alternate/no"),
                 option_field(OI_CHECKPOINT_NEWSRC_FREQUENCY, "Checkpoint Newsrc Frequency", "<# articles>"),
                 option_field(OI_DEFAULT_REFETCH_TIME, "Default Refetch Time", "never/<1 day 5 hours 8 mins>"),
                 option_field(OI_NOVICE_DELAYS, "Novice Delays", "yes/no"),
                 option_field(OI_OLD_MTHREADS_DATABASE, "Old Mthreads Database", "yes/no"),

                 IniField::group("Article Scan Mode Options"),
                 option_field(OI_SCAN_ART_FOLLOW, "Follow Threads", "yes/no"),
                 option_field(OI_SCAN_ART_FOLD, "Fold Subjects", "yes/no"),
                 option_field(OI_SCAN_ART_UNZOOM_FOLD, "Re-fold Subjects", "yes/no"),
                 option_field(OI_SCAN_ART_MARK_STAY, "Mark Without Moving", "yes/no"),
                 option_field(OI_SCAN_VI, "VI Key Movement Allowed", "yes/no"),
                 option_field(OI_SCAN_ITEM_NUM, "Display Item Numbers", "yes/no"),
                 option_field(OI_SCAN_ART_DISP_ART_NUM, "Display Article Number", "yes/no"),
                 option_field(OI_SCAN_ART_DISP_AUTHOR, "Display Author", "yes/no"),
                 option_field(OI_SCAN_ART_DISP_SCORE, "Display Score", "yes/no"),
                 option_field(OI_SCAN_ART_DISP_SUB_COUNT, "Display Subject Count", "yes/no"),
                 option_field(OI_SCAN_ART_DISP_SUBJ, "Display Subject", "yes/no"),
                 option_field(OI_SCAN_ART_DISP_SUMMARY, "Display Summary", "yes/no"),
                 option_field(OI_SCAN_ART_DISP_KEYW, "Display Keywords", "yes/no"),

                 IniField::group("Scoring Options"),
                 option_field(OI_SC_VERBOSE, "Verbose scoring", "yes/no"),
             }},
    m_rows_by_option(static_cast<int>(OI_SCORE_LAST) + 1)
{
    for (int row = first_row(); row <= row_count(); ++row)
    {
        if (display_row(row).is_value())
        {
            m_rows_by_option[display_row(row).id()] = row;
        }
    }
}

std::string_view OptionCatalog::help(OptionIndex option) const
{
    const int row = row_for(option);
    if (row == 0)
    {
        return {};
    }
    return display_row(row).help();
}

int OptionCatalog::row_for(OptionIndex option) const
{
    const int index = static_cast<int>(option);
    if (index < 0 || index >= size_cast<int>(m_rows_by_option))
    {
        return 0;
    }
    return m_rows_by_option[index];
}

int OptionCatalog::previous_group_row(int row) const
{
    while (--row >= first_row())
    {
        if (is_group(row))
        {
            return row;
        }
    }
    return 0;
}
