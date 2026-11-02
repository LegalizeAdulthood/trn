// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include "support/NewsgroupPageState.h"

#include <config/common.h>
#include <trn/cache.h>
#include <trn/hash.h>
#include <trn/rt-page.h>
#include <trn/rt-select-internal.h>
#include <trn/Subject.h>
#include <trn/univ.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <string>
#include <tuple>
#include <vector>

namespace
{

struct PagingCommand
{
    const char  *name;
    char         key;
    int          origin;
    int          destination;
    DisplayState result;
};

class SelectorPagingStateTest : public test_support::NewsgroupPageDisplayState
{
protected:
    void         SetUp() override;
    void         TearDown() override;
    void         prepare(SelectionMode mode, bool rereading, int count = 6);
    void         position_page(int page);
    DisplayState dispatch(char command);
    void         expect_page(int page, int cursor, bool empty = false, bool redraw = true);
    void         expect_preserved() const;
    std::vector<int> flags() const;
    int              row_number(int index) const;

private:
    void             prepare_articles(int count);
    void             prepare_groups(int count);
    void             prepare_menu(int count);

    std::array<Subject, 6>   m_subjects{};
    std::vector<int>         m_flags;
    std::string              m_rendered;
    ArticleUnread            m_selected{};
    int                      m_selected_subjects{};
    decltype(g_cache.article_list) m_old_articles{std::move(g_cache.article_list)};
    decltype(g_art_ptr_list) m_old_article_ptrs{std::move(g_art_ptr_list)};
    Article                **m_old_art_ptr{g_art_ptr};
    ArticleNum               m_old_abs_first{g_abs_first};
    ArticleNum               m_old_first{g_first_art};
    ArticleNum               m_old_last{g_last_art};
    ArticleNum                     m_old_first_cached{g_cache.first_cached};
    ArticleNum                     m_old_last_cached{g_cache.last_cached};
    bool                           m_old_cached_all{g_cache.cached_all_in_range};
    Subject                 *m_old_first_subject{g_first_subject};
    Subject                 *m_old_last_subject{g_last_subject};
    int                      m_old_subject_count{g_subject_count};
    Article                **m_old_page_app{g_sel_page.page_art};
    Article                **m_old_next_app{g_sel_page.next_art};
    Subject                 *m_old_page_sp{g_sel_page.page_subj};
    Subject                 *m_old_next_sp{g_sel_page.next_subj};
    int                      m_old_selected_subjects{g_selected_subj_cnt};
    bool                     m_old_selected_only{g_sel.selected_only};
    bool                     m_old_threaded{g_threaded_group};
    bool                     m_old_redirected{g_redirected};
    bool                     m_old_can_home{g_can_home};
    int                      m_old_page_line{g_page_line};
    bool                     m_old_chase{g_output_chase_phrase};
    GeneralMode              m_old_general{g_general_mode};
    MinorMode                m_old_minor{g_mode};
    std::string              m_old_name{g_newsgroup_name};
    std::string              m_old_directory{g_newsgroup_dir};
    std::string              m_old_moderated{g_moderated};
    NewsgroupData           *m_old_current{g_current_newsgroup};
    std::string              m_old_display{g_sel_art_display_mode};
    std::size_t              m_old_display_index{g_sel_art_display_mode_index};
    UniversalItemList        m_old_menu{std::move(g_univ.items)};
    UniversalItemIndex       m_old_menu_page{g_sel_page.page_univ_index};
    UniversalItemIndex       m_old_menu_next{g_sel_page.next_univ_index};
    std::string              m_old_title{g_univ.title};
    int                      m_old_level{g_univ.level};
    HashTable               *m_old_hash{g_newsrc_hash};
};

void SelectorPagingStateTest::SetUp()
{
    NewsgroupPageDisplayState::SetUp();
    g_cache.article_list.clear();
    g_art_ptr_list.clear();
    g_art_ptr = nullptr;
    g_sel_page.page_art = g_sel_page.next_art = nullptr;
    g_sel_page.page_subj = g_sel_page.next_subj = nullptr;
    g_univ.items.clear();
    g_univ.items.reserve(6);
    g_sel_page.page_univ_index = g_sel_page.next_univ_index = {};
    g_tc_LINES = 7;
    g_can_home = false;
    g_sel.selected_only = false;
    g_redirected = false;
    g_newsgroup_name = "comp.paging";
    g_moderated.clear();
    g_current_newsgroup = g_newsgroup_ptr = nullptr;
    g_sel_art_display_mode = "s";
    g_sel_art_display_mode_index = 0;
    g_sel_grp_display_mode = "s";
    g_sel_grp_display_mode_index = 0;
    g_sel_page.item_index = 0;
    g_univ.title = "Paging menu";
    g_univ.level = 1;
    g_selected_count = TR_NONE;
    g_selected_subj_cnt = 0;
    g_newsrc_hash = hash_create(17, [](std::string_view key, HashDatum data)
                                { return key.compare(reinterpret_cast<NewsgroupData *>(data.dat_ptr)->rc_name()); });
}

void SelectorPagingStateTest::TearDown()
{
    g_cache.article_list = std::move(m_old_articles);
    g_art_ptr_list = std::move(m_old_article_ptrs);
    g_art_ptr = m_old_art_ptr;
    g_abs_first = m_old_abs_first;
    g_first_art = m_old_first;
    g_last_art = m_old_last;
    g_cache.first_cached = m_old_first_cached;
    g_cache.last_cached = m_old_last_cached;
    g_cache.cached_all_in_range = m_old_cached_all;
    g_first_subject = m_old_first_subject;
    g_last_subject = m_old_last_subject;
    g_subject_count = m_old_subject_count;
    g_sel_page.page_art = m_old_page_app;
    g_sel_page.next_art = m_old_next_app;
    g_sel_page.page_subj = m_old_page_sp;
    g_sel_page.next_subj = m_old_next_sp;
    g_selected_subj_cnt = m_old_selected_subjects;
    g_sel.selected_only = m_old_selected_only;
    g_threaded_group = m_old_threaded;
    g_redirected = m_old_redirected;
    g_can_home = m_old_can_home;
    g_page_line = m_old_page_line;
    g_output_chase_phrase = m_old_chase;
    g_general_mode = m_old_general;
    g_mode = m_old_minor;
    g_newsgroup_name = m_old_name;
    g_newsgroup_dir = m_old_directory;
    g_moderated = m_old_moderated;
    g_current_newsgroup = m_old_current;
    g_sel_art_display_mode = m_old_display;
    g_sel_art_display_mode_index = m_old_display_index;
    g_univ.items = std::move(m_old_menu);
    g_sel_page.page_univ_index = m_old_menu_page;
    g_sel_page.next_univ_index = m_old_menu_next;
    g_univ.title = m_old_title;
    g_univ.level = m_old_level;
    hash_destroy(g_newsrc_hash);
    g_newsrc_hash = m_old_hash;
    NewsgroupPageDisplayState::TearDown();
}

void SelectorPagingStateTest::prepare(SelectionMode mode, bool rereading, int count)
{
    set_read_scope(rereading);
    g_sel.mode = mode;
    g_general_mode = GM_SELECTOR;
    g_mode = mode == SM_UNIVERSAL ? MM_UNIVERSAL : mode == SM_NEWSGROUP ? MM_NEWSGROUP_SELECTOR : MM_THREAD_SELECTOR;
    g_in_ng = mode == SM_ARTICLE || mode == SM_SUBJECT || mode == SM_THREAD;
    g_threaded_group = mode == SM_THREAD;
    if (mode == SM_NEWSGROUP)
    {
        prepare_groups(count);
    }
    else if (mode == SM_UNIVERSAL)
    {
        prepare_menu(count);
    }
    else
    {
        prepare_articles(count);
    }
    init_pages(PRESERVE_PAGE);
    if (count)
    {
        if (mode == SM_NEWSGROUP)
        {
            g_ng_order.order[3]->m_flags |= NF_DEL;
        }
        else if (mode == SM_UNIVERSAL)
        {
            g_univ.items[3].m_flags |= UF_DEL;
        }
        else
        {
            g_cache.ptr(ArticleNum{4})->m_flags |= AF_DEL;
        }
    }
    m_flags = flags();
    m_selected = g_selected_count;
    m_selected_subjects = g_selected_subj_cnt;
}

void SelectorPagingStateTest::prepare_articles(int count)
{
    g_abs_first = g_first_art = g_cache.first_cached = ArticleNum{1};
    g_last_art = g_cache.last_cached = ArticleNum{count};
    g_cache.cached_all_in_range = true;
    g_subject_count = count;
    g_first_subject = count ? &m_subjects[0] : nullptr;
    g_last_subject = count ? &m_subjects[count - 1] : nullptr;
    for (int index = 0; index < count; ++index)
    {
        Subject &subject = m_subjects[index];
        Article *article = g_cache.ptr(ArticleNum{index + 1});
        subject.m_str = "Re: paging-row-" + std::to_string(index + 1);
        subject.m_prev = index ? &m_subjects[index - 1] : nullptr;
        subject.m_next = index + 1 < count ? &m_subjects[index + 1] : nullptr;
        subject.m_articles = subject.m_thread = article;
        subject.m_thread_link = &subject;
        article->m_subj = &subject;
        article->m_from = "Paging <paging@example.test>";
        article->m_date = index + 1;
        article->m_flags = AF_EXISTS | AF_CACHED | AF_THREADED;
        if (!g_sel.rereading)
        {
            article->m_flags |= AF_UNREAD;
        }
        if (index == 0 || index == 4)
        {
            article->m_flags |= static_cast<ArticleFlags>(g_sel.mask);
        }
    }
    count_subjects(CS_UNSEL_STORE);
}

void SelectorPagingStateTest::prepare_groups(int count)
{
    for (int index = 0; index < count; ++index)
    {
        const NewsgroupFlags selected = index == 0 || index == 4 ? static_cast<NewsgroupFlags>(g_sel.mask) : NF_NONE;
        NewsgroupData       &group = add_group("comp.paging-row-" + std::to_string(index + 1),
                                         g_sel.rereading ? TR_NONE : ArticleUnread{3}, selected);
        group.m_rc = &m_newsrc;
        group.m_rc_line += "1-3";
    }
}

void SelectorPagingStateTest::prepare_menu(int count)
{
    if (count)
    {
        NewsgroupData &group = add_group("comp.paging", g_sel.rereading ? TR_NONE : ArticleUnread{count});
        group.m_rc = &m_newsrc;
        group.m_ng_max = ArticleNum{count};
        group.m_rc_line += g_sel.rereading ? "1-6" : "0";
        hash_store(g_newsrc_hash, group.rc_name(),
                   HashDatum{reinterpret_cast<char *>(&group), static_cast<unsigned>(group.rc_name().size())});
    }
    g_selected_count = count ? ArticleUnread{2} : TR_NONE;
    for (int index = 0; index < count; ++index)
    {
        UniversalItem &item = g_univ.items.emplace_back();
        item.m_num = index + 1;
        item.m_desc = "paging-row-" + std::to_string(index + 1);
        item.m_data = UniversalVirtualArticle{"comp.paging", "<paging-" + std::to_string(index + 1) + "@example.test>",
                                              "Paging", item.m_desc, ArticleNum{index + 1}};
        if (index == 0 || index == 4)
        {
            item.m_flags |= static_cast<UniversalItemFlags>(g_sel.mask);
        }
    }
}

void SelectorPagingStateTest::position_page(int page)
{
    m_rendered = render_page();
    for (int index = 0; index < page; ++index)
    {
        ASSERT_TRUE(g_sel_page.next_page());
        m_rendered = render_page();
    }
}

DisplayState SelectorPagingStateTest::dispatch(char command)
{
    testing::internal::CaptureStdout();
    const DisplayState result = sel_command(command);
    (void) testing::internal::GetCapturedStdout();
    return result;
}

int SelectorPagingStateTest::row_number(int index) const
{
    if (g_sel.mode == SM_NEWSGROUP)
    {
        const auto item = std::find(g_ng_order.order.begin(), g_ng_order.order.end(), g_sel_page.items[index].u.np);
        return static_cast<int>(item - g_ng_order.order.begin()) + 1;
    }
    if (g_sel.mode == SM_UNIVERSAL)
    {
        return g_sel_page.items[index].u.un->m_num;
    }
    const Article *article =
        g_sel.mode == SM_ARTICLE ? g_sel_page.items[index].u.ap : g_sel_page.items[index].u.sp->m_articles;
    return static_cast<int>(article->article_num().value_of());
}

std::vector<int> SelectorPagingStateTest::flags() const
{
    std::vector<int> result;
    if (g_sel.mode == SM_NEWSGROUP)
    {
        for (const NewsgroupData *group : g_ng_order.order)
        {
            result.push_back(group->m_flags);
        }
    }
    else if (g_sel.mode == SM_UNIVERSAL)
    {
        for (const UniversalItem &item : univ_items())
        {
            result.push_back(item.m_flags);
        }
    }
    else
    {
        for (const auto &[number, article] : g_cache.article_list)
        {
            result.push_back(article.m_flags);
            result.push_back(article.m_subj->m_flags);
        }
    }
    return result;
}

void SelectorPagingStateTest::expect_preserved() const
{
    EXPECT_EQ(m_flags, flags());
    EXPECT_EQ(m_selected, g_selected_count);
    EXPECT_EQ(m_selected_subjects, g_selected_subj_cnt);
    EXPECT_EQ(SS_NATURAL, g_sel.sort);
    EXPECT_EQ(1, g_sel.direction);
    for (const NewsgroupData *group : g_ng_order.order)
    {
        EXPECT_EQ(g_sel.mode == SM_UNIVERSAL ? (g_sel.rereading ? " 1-6" : " 0") : " 1-3", group->rc_numbers());
        EXPECT_EQ(':', group->m_subscribe_char);
    }
}

void SelectorPagingStateTest::expect_page(int page, int cursor, bool empty, bool redraw)
{
    if (redraw)
    {
        m_rendered = render_page();
    }

    EXPECT_EQ(empty ? 0 : 6, g_sel_page.total_obj_cnt);
    ASSERT_EQ(empty ? 0 : 2, g_sel_page.page_item_cnt);
    EXPECT_EQ(empty ? 0 : 2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(empty ? 0 : page * 2, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(cursor, g_sel_page.item_index);
    EXPECT_EQ(empty || page == 2, g_sel_page.at_end);
    for (int number = 1; number <= 6; ++number)
    {
        const bool visible = !empty && number > page * 2 && number <= page * 2 + 2;
        EXPECT_EQ(visible, m_rendered.find("paging-row-" + std::to_string(number)) != std::string::npos) << m_rendered;
    }
    if (!empty)
    {
        EXPECT_EQ(page * 2 + 1, row_number(0));
        EXPECT_EQ(page * 2 + 2, row_number(1));
    }
    expect_preserved();
}

class SelectorPagingCommandTest : public SelectorPagingStateTest,
                                  public testing::WithParamInterface<std::tuple<PagingCommand, SelectionMode, bool>>
{
};

TEST_P(SelectorPagingCommandTest, pageCommandMovesOrRetainsThePageWithoutChangingSelections)
{
    const auto [command, mode, rereading] = GetParam();
    prepare(mode, rereading);
    ASSERT_NO_FATAL_FAILURE(position_page(command.origin));
    g_sel_page.item_index = 1;
    g_term_scrolled = 4;
    g_page_line = 37;
    g_output_chase_phrase = false;

    const DisplayState result = dispatch(command.key);

    EXPECT_EQ(command.result, result);
    EXPECT_EQ(command.key == Ctl('l') ? 1 : 0, g_sel_page.item_index);
    EXPECT_EQ(0, g_term_scrolled);
    EXPECT_EQ(1, g_page_line);
    EXPECT_TRUE(g_output_chase_phrase);
    EXPECT_EQ(mode, g_sel.mode);
    EXPECT_EQ(rereading, g_sel.rereading);
    expect_page(command.destination, command.key == Ctl('l') ? 1 : 0, false, result == DS_DISPLAY);
}

INSTANTIATE_TEST_SUITE_P(
    CommandModeAndReadScope, SelectorPagingCommandTest,
    testing::Combine(
        testing::Values(
            PagingCommand{"Next", '>', 0, 1, DS_DISPLAY}, PagingCommand{"NextToLast", '>', 1, 2, DS_DISPLAY},
            PagingCommand{"NextAtLast", '>', 2, 2, DS_ASK}, PagingCommand{"Previous", '<', 1, 0, DS_DISPLAY},
            PagingCommand{"PreviousFromLast", '<', 2, 1, DS_DISPLAY},
            PagingCommand{"PreviousAtFirst", '<', 0, 0, DS_ASK}, PagingCommand{"First", '^', 2, 0, DS_DISPLAY},
            PagingCommand{"FirstAtFirst", '^', 0, 0, DS_ASK}, PagingCommand{"ControlR", Ctl('r'), 1, 0, DS_DISPLAY},
            PagingCommand{"Last", '$', 0, 2, DS_DISPLAY}, PagingCommand{"LastAtLast", '$', 2, 2, DS_ASK},
            PagingCommand{"Refresh", Ctl('l'), 1, 1, DS_DISPLAY}),
        testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD, SM_NEWSGROUP, SM_UNIVERSAL), testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<PagingCommand, SelectionMode, bool>> &info)
    {
        return std::string{std::get<0>(info.param).name} + "Mode" + std::to_string(std::get<1>(info.param)) +
               (std::get<2>(info.param) ? "Read" : "Unread");
    });

class EmptySelectorPagingCommandTest : public SelectorPagingStateTest,
                                       public testing::WithParamInterface<std::tuple<char, SelectionMode, bool>>
{
};

TEST_P(EmptySelectorPagingCommandTest, anEmptySelectorRetainsItsEmptyPageAndSelectionState)
{
    const auto [command, mode, rereading] = GetParam();
    prepare(mode, rereading, 0);
    ASSERT_NO_FATAL_FAILURE(position_page(0));

    const DisplayState result = dispatch(command);

    EXPECT_EQ(command == Ctl('l') ? DS_DISPLAY : DS_ASK, result);
    EXPECT_EQ(0, g_sel_page.item_index);
    expect_page(0, 0, true, result == DS_DISPLAY);
}

INSTANTIATE_TEST_SUITE_P(
    CommandModeAndReadScope, EmptySelectorPagingCommandTest,
    testing::Combine(testing::Values('>', '<', '^', Ctl('r'), '$', Ctl('l')),
                     testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD, SM_NEWSGROUP, SM_UNIVERSAL), testing::Bool()));

class SelectorEscapedPageState : public SelectorPagingStateTest
{
protected:
    void         SetUp() override;
    void         TearDown() override;
    DisplayState escape(char command);
    void         expect_next_command();

    std::string m_output;

private:
    void             discard_input();
    int              m_old_errno{errno};
    std::string_view m_old_erase{g_tc_CE};
};

void SelectorEscapedPageState::SetUp()
{
    SelectorPagingStateTest::SetUp();
    discard_input();
    errno = 0;
    g_tc_CE = "";
}

void SelectorEscapedPageState::TearDown()
{
    discard_input();
    errno = m_old_errno;
    g_tc_CE = m_old_erase;
    SelectorPagingStateTest::TearDown();
}

void SelectorEscapedPageState::discard_input()
{
    while (macro_pending())
    {
        (void) read_tty_char();
    }
}

DisplayState SelectorEscapedPageState::escape(char command)
{
    g_pushback.push('Q' | 0200);
    g_pushback.push(static_cast<unsigned char>(command) | 0200);
    testing::internal::CaptureStdout();
    const DisplayState result = sel_command('\\');
    m_output = testing::internal::GetCapturedStdout();
    return result;
}

void SelectorEscapedPageState::expect_next_command()
{
    ASSERT_TRUE(macro_pending());
    const std::string next = get_cmd();
    ASSERT_FALSE(next.empty());
    EXPECT_EQ('Q', next.front());
    EXPECT_FALSE(macro_pending());
}

class SelectorBlankEscapeTest : public SelectorEscapedPageState,
                                public testing::WithParamInterface<std::tuple<SelectionMode, bool, char>>
{
};

TEST_P(SelectorBlankEscapeTest, blankEscapeRetainsThePageCursorSelectionsAndQueuedCommand)
{
    const auto [mode, rereading, blank] = GetParam();
    prepare(mode, rereading);
    ASSERT_NO_FATAL_FAILURE(position_page(1));
    g_sel_page.item_index = 1;

    const DisplayState result = escape(blank);

    EXPECT_EQ(DS_ASK, result);
    EXPECT_EQ(mode, g_sel.mode);
    EXPECT_EQ(rereading, g_sel.rereading);
    EXPECT_NE(std::string::npos, m_output.find("Cmd: "));
    expect_page(1, 1, false, false);
    expect_next_command();
}

INSTANTIATE_TEST_SUITE_P(ModeReadScopeAndBlank, SelectorBlankEscapeTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD, SM_NEWSGROUP,
                                                          SM_UNIVERSAL),
                                          testing::Bool(), testing::Values(' ', '\r', '\n')));

class SelectorRefreshEscapeTest : public SelectorEscapedPageState,
                                  public testing::WithParamInterface<std::tuple<SelectionMode, bool>>
{
};

TEST_P(SelectorRefreshEscapeTest, escapedControlCommandRequestsRefreshWithoutChangingThePage)
{
    const auto [mode, rereading] = GetParam();
    prepare(mode, rereading);
    ASSERT_NO_FATAL_FAILURE(position_page(1));
    g_sel_page.item_index = 1;

    const DisplayState result = escape(Ctl('l'));

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(mode, g_sel.mode);
    EXPECT_EQ(rereading, g_sel.rereading);
    expect_page(1, 1, false, false);
    expect_next_command();
}

INSTANTIATE_TEST_SUITE_P(
    ModeAndReadScope, SelectorRefreshEscapeTest,
    testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD, SM_NEWSGROUP, SM_UNIVERSAL), testing::Bool()));

class SelectorGroupEscapeTest : public SelectorEscapedPageState, public testing::WithParamInterface<bool>
{
};

TEST_P(SelectorGroupEscapeTest, commandPromptUsesTheCursorGroupInsteadOfThePreviousGroup)
{
    prepare(SM_NEWSGROUP, GetParam());
    ASSERT_NO_FATAL_FAILURE(position_page(1));
    g_sel_page.item_index = 1;
    NewsgroupData *const target = g_sel_page.items[1].u.np;
    std::string          directory{target->rc_line()};
    std::replace(directory.begin(), directory.end(), '.', '/');
    g_newsgroup_ptr = g_ng_order.order.front();
    g_newsgroup_name = "stale.group";

    const DisplayState result = escape(' ');

    EXPECT_EQ(DS_ASK, result);
    EXPECT_EQ(target, g_newsgroup_ptr);
    EXPECT_EQ(target->rc_line(), g_newsgroup_name);
    EXPECT_EQ(directory, g_newsgroup_dir);
    EXPECT_NE(std::string::npos, m_output.find("[" + std::string(target->rc_line()) + "] Cmd: "));
    EXPECT_EQ(std::string::npos, m_output.find("stale.group"));
    expect_page(1, 1, false, false);
    expect_next_command();
}

TEST_P(SelectorGroupEscapeTest, anEmptyGroupPageUsesTheEndPromptAndClearsThePreviousGroup)
{
    prepare(SM_NEWSGROUP, GetParam(), 0);
    ASSERT_NO_FATAL_FAILURE(position_page(0));
    NewsgroupData previous{};
    g_newsgroup_ptr = &previous;

    const DisplayState result = escape(' ');

    EXPECT_EQ(DS_ASK, result);
    EXPECT_EQ(nullptr, g_newsgroup_ptr);
    EXPECT_NE(std::string::npos, m_output.find("[*End*] Cmd: "));
    expect_page(0, 0, true, false);
    expect_next_command();
}

INSTANTIATE_TEST_SUITE_P(ReadScope, SelectorGroupEscapeTest, testing::Bool());

class SelectorExitStateTest : public SelectorPagingStateTest
{
protected:
    void             prepare_exit(SelectionMode mode, bool rereading, bool empty = false);
    void             set_selection(std::vector<int> selected, bool killed = false);
    void             expect_exit(std::vector<int> selected, bool implicit = false) const;
    std::vector<int> current_objects() const;

private:
    void             group_current_row();
    void             set_object_flags(int number, int value);
    std::vector<int> marked_objects(int mask) const;
    std::vector<int> rows() const;

    std::vector<int>   m_before_flags;
    std::vector<int>   m_before_killed;
    std::vector<int>   m_before_rows;
    Article          **m_page_app{};
    Subject           *m_page_sp{};
    NewsgroupData     *m_page_np{};
    UniversalItemIndex m_page_menu{};
    int                m_prior{};
    int                m_total{};
    int                m_objects{};
    int                m_items{};
    bool               m_end{};
    bool               m_empty{};
    SelectionMode      m_mode{};
    bool               m_rereading{};
};

void SelectorExitStateTest::prepare_exit(SelectionMode mode, bool rereading, bool empty)
{
    prepare(mode, rereading, empty ? 0 : 6);
    m_empty = empty;
    m_mode = mode;
    m_rereading = rereading;
    g_tc_LINES = 9;
    if (!empty)
    {
        group_current_row();
    }
}

void SelectorExitStateTest::group_current_row()
{
    if (g_sel.mode != SM_SUBJECT && g_sel.mode != SM_THREAD)
    {
        return;
    }
    Article *first = g_cache.ptr(ArticleNum{2});
    Article *reply = g_cache.ptr(ArticleNum{3});
    Subject *first_subject = first->m_subj;
    Subject *reply_subject = reply->m_subj;
    if (g_sel.mode == SM_SUBJECT)
    {
        first->m_subj_next = reply;
        reply->m_subj = first_subject;
        reply_subject->m_articles = nullptr;
        first_subject->m_next = reply_subject->m_next;
        reply_subject->m_next->m_prev = first_subject;
        --g_subject_count;
    }
    else
    {
        first_subject->m_thread_link = reply_subject;
        reply_subject->m_thread_link = first_subject;
        reply_subject->m_thread = first;
        first->m_child1 = reply;
        reply->m_parent = first;
    }
}

std::vector<int> SelectorExitStateTest::current_objects() const
{
    return g_sel.mode == SM_SUBJECT || g_sel.mode == SM_THREAD ? std::vector<int>{2, 3} : std::vector<int>{2};
}

void SelectorExitStateTest::set_object_flags(int number, int value)
{
    if (g_sel.mode == SM_NEWSGROUP)
    {
        g_ng_order.order[number - 1]->m_flags = static_cast<NewsgroupFlags>(value);
    }
    else if (g_sel.mode == SM_UNIVERSAL)
    {
        g_univ.items[number - 1].m_flags = static_cast<UniversalItemFlags>(value);
    }
    else
    {
        g_cache.ptr(ArticleNum{number})->m_flags = static_cast<ArticleFlags>(value);
    }
}

std::vector<int> SelectorExitStateTest::marked_objects(int mask) const
{
    const std::vector<int> values = flags();
    const std::size_t      stride = g_in_ng ? 2 : 1;
    std::vector<int>       result;
    for (std::size_t index = 0; index < values.size(); index += stride)
    {
        if (values[index] & mask)
        {
            result.push_back(static_cast<int>(index / stride) + 1);
        }
    }
    return result;
}

std::vector<int> SelectorExitStateTest::rows() const
{
    std::vector<int> result;
    for (int index = 0; index < g_sel_page.page_item_cnt; ++index)
    {
        result.push_back(row_number(index));
    }
    return result;
}

void SelectorExitStateTest::set_selection(std::vector<int> selected, bool killed)
{
    std::vector<int> values = flags();
    const int        stride = g_in_ng ? 2 : 1;
    const int        selection_bits = AGF_SEL | AGF_DEL_SEL | AGF_DEL;
    for (int index = 0; index < (m_empty ? 0 : 6); ++index)
    {
        int value = values[index * stride] & ~selection_bits;
        if (std::find(selected.begin(), selected.end(), index + 1) != selected.end())
        {
            value |= g_sel.mask;
        }
        if (index == 4)
        {
            value |= g_sel.rereading ? AGF_SEL : AGF_DEL_SEL;
        }
        set_object_flags(index + 1, value);
    }
    if (g_in_ng)
    {
        for (Subject *subject = g_first_subject; subject; subject = subject->m_next)
        {
            subject->m_flags &= ~(SF_SEL | SF_DEL_SEL | SF_DEL);
        }
        count_subjects(CS_UNSEL_STORE);
    }
    else
    {
        g_selected_count = ArticleUnread{static_cast<long>(selected.size())};
    }
    init_pages(PRESERVE_PAGE);
    if (!m_empty)
    {
        values = flags();
        for (const int number : current_objects())
        {
            const int value = values[(number - 1) * stride];
            set_object_flags(number, killed ? value | AGF_DEL : value & ~static_cast<int>(AGF_DEL));
            if (g_in_ng)
            {
                Subject *subject = g_cache.ptr(ArticleNum{number})->m_subj;
                subject->m_flags = killed ? subject->m_flags | SF_DEL : subject->m_flags & ~SF_DEL;
            }
        }
    }
    ASSERT_NO_FATAL_FAILURE(position_page(0));
    g_sel_page.item_index = m_empty ? 0 : 1;
    m_before_flags = flags();
    m_before_killed = marked_objects(AGF_DEL);
    m_before_rows = rows();
    m_page_app = g_sel_page.page_art;
    m_page_sp = g_sel_page.page_subj;
    m_page_np = g_sel_page.page_ngd;
    m_page_menu = g_sel_page.page_univ_index;
    m_prior = g_sel_page.prior_obj_cnt;
    m_total = g_sel_page.total_obj_cnt;
    m_objects = g_sel_page.page_obj_cnt;
    m_items = g_sel_page.page_item_cnt;
    m_end = g_sel_page.at_end;
    EXPECT_EQ(selected, marked_objects(g_sel.mask));
    EXPECT_EQ(ArticleUnread{static_cast<long>(selected.size())}, g_selected_count);
}

void SelectorExitStateTest::expect_exit(std::vector<int> selected, bool implicit) const
{
    EXPECT_EQ(selected, marked_objects(g_sel.mask));
    EXPECT_EQ(ArticleUnread{static_cast<long>(selected.size())}, g_selected_count);
    EXPECT_EQ(m_empty ? std::vector<int>{} : std::vector<int>{5},
              marked_objects(g_sel.rereading ? AGF_SEL : AGF_DEL_SEL));
    std::vector<int> killed = m_before_killed;
    if (implicit)
    {
        for (int number : current_objects())
        {
            killed.erase(std::remove(killed.begin(), killed.end(), number), killed.end());
        }
    }
    else
    {
        EXPECT_EQ(m_before_flags, flags());
    }
    const std::vector<int> targets = m_empty ? std::vector<int>{} : current_objects();
    const std::vector<int> after = flags();
    const int              stride = g_in_ng ? 2 : 1;
    for (int index = 0; index < (m_empty ? 0 : 6); ++index)
    {
        if (std::find(targets.begin(), targets.end(), index + 1) == targets.end())
        {
            for (int field = 0; field < stride; ++field)
            {
                EXPECT_EQ(m_before_flags[index * stride + field], after[index * stride + field]);
            }
        }
    }
    std::vector<Subject *> selected_subjects;
    if (g_in_ng)
    {
        for (int number : selected)
        {
            Subject *subject = g_cache.ptr(ArticleNum{number})->m_subj;
            if (std::find(selected_subjects.begin(), selected_subjects.end(), subject) == selected_subjects.end())
            {
                selected_subjects.push_back(subject);
            }
        }
    }
    EXPECT_EQ(selected_subjects.size(), g_selected_subj_cnt);
    EXPECT_EQ(killed, marked_objects(AGF_DEL));
    EXPECT_EQ(m_mode, g_sel.mode);
    EXPECT_EQ(m_rereading, g_sel.rereading);
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(m_mode == SM_UNIVERSAL   ? MM_UNIVERSAL
              : m_mode == SM_NEWSGROUP ? MM_NEWSGROUP_SELECTOR
                                       : MM_THREAD_SELECTOR,
              g_mode);
    EXPECT_EQ(m_rereading ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
    EXPECT_EQ(m_empty ? 0 : 1, g_sel_page.item_index);
    EXPECT_EQ(m_page_app, g_sel_page.page_art);
    EXPECT_EQ(m_page_sp, g_sel_page.page_subj);
    EXPECT_EQ(m_page_np, g_sel_page.page_ngd);
    EXPECT_EQ(m_page_menu, g_sel_page.page_univ_index);
    EXPECT_EQ(m_prior, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(m_total, g_sel_page.total_obj_cnt);
    EXPECT_EQ(m_objects, g_sel_page.page_obj_cnt);
    EXPECT_EQ(m_items, g_sel_page.page_item_cnt);
    EXPECT_EQ(m_end, g_sel_page.at_end);
    EXPECT_EQ(m_before_rows, rows());
    if (g_sel.mode == SM_NEWSGROUP)
    {
        EXPECT_EQ(m_empty ? nullptr : g_sel_page.items[1].u.np, g_newsgroup_ptr);
    }
    EXPECT_EQ(RF_ACTIVE, m_newsrc.flags);
    for (const NewsgroupData *group : g_ng_order.order)
    {
        EXPECT_EQ(g_sel.mode == SM_UNIVERSAL ? (g_sel.rereading ? " 1-6" : " 0") : " 1-3", group->rc_numbers());
        EXPECT_EQ(':', group->m_subscribe_char);
    }
    for (const auto &[number, article] : g_cache.article_list)
    {
        EXPECT_NE(AF_NONE, article.m_flags & AF_EXISTS);
        EXPECT_EQ(!g_sel.rereading, (article.m_flags & AF_UNREAD) != 0);
    }
}

enum class ExitSelection
{
    NONE,
    CURRENT,
    OFF_PAGE,
    KILLED
};

class SelectorEnterPolicyTest : public SelectorExitStateTest,
                                public testing::WithParamInterface<std::tuple<char, SelectionMode, bool, ExitSelection>>
{
};

TEST_P(SelectorEnterPolicyTest, enterSelectsTheCurrentRowOnlyWhenTheAcceptancePolicyAllowsIt)
{
    const auto [command, mode, rereading, selection] = GetParam();
    prepare_exit(mode, rereading);
    const std::vector<int> initial = selection == ExitSelection::CURRENT    ? current_objects()
                                     : selection == ExitSelection::OFF_PAGE ? std::vector<int>{6}
                                                                            : std::vector<int>{};
    ASSERT_NO_FATAL_FAILURE(set_selection(initial, selection == ExitSelection::KILLED));
    const bool implicit = selection == ExitSelection::NONE || (selection == ExitSelection::KILLED && rereading);

    const DisplayState result = dispatch(command);

    EXPECT_EQ(DS_QUIT, result);
    expect_exit(implicit ? current_objects() : initial, implicit);
}

INSTANTIATE_TEST_SUITE_P(
    CommandModeReadScopeAndSelections, SelectorEnterPolicyTest,
    testing::Combine(testing::Values('\r', '\n'),
                     testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD, SM_NEWSGROUP, SM_UNIVERSAL), testing::Bool(),
                     testing::Values(ExitSelection::NONE, ExitSelection::CURRENT, ExitSelection::OFF_PAGE,
                                     ExitSelection::KILLED)));

class SelectorExitPreservationTest : public SelectorExitStateTest,
                                     public testing::WithParamInterface<std::tuple<char, SelectionMode, bool, bool>>
{
};

TEST_P(SelectorExitPreservationTest, acceptQuitAndCancelRetainThePendingSelectionWithoutAddingTheCurrentRow)
{
    const auto [command, mode, rereading, selected] = GetParam();
    prepare_exit(mode, rereading);
    const std::vector<int> initial = selected ? std::vector<int>{6} : std::vector<int>{};
    ASSERT_NO_FATAL_FAILURE(set_selection(initial, true));

    const DisplayState result = dispatch(command);

    EXPECT_EQ(DS_QUIT, result);
    expect_exit(initial);
}

INSTANTIATE_TEST_SUITE_P(CommandModeReadScopeAndSelections, SelectorExitPreservationTest,
                         testing::Combine(testing::Values('Z', 'q', '\033'),
                                          testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD, SM_NEWSGROUP,
                                                          SM_UNIVERSAL),
                                          testing::Bool(), testing::Bool()));

class EmptySelectorExitPolicyTest : public SelectorExitStateTest,
                                    public testing::WithParamInterface<std::tuple<char, SelectionMode, bool>>
{
};

TEST_P(EmptySelectorExitPolicyTest, anEmptySelectorExitsWithoutSelectingAnAbsentCurrentRow)
{
    const auto [command, mode, rereading] = GetParam();
    prepare_exit(mode, rereading, true);
    ASSERT_NO_FATAL_FAILURE(set_selection({}));

    const DisplayState result = dispatch(command);

    EXPECT_EQ(DS_QUIT, result);
    expect_exit({});
}

INSTANTIATE_TEST_SUITE_P(
    CommandModeAndReadScope, EmptySelectorExitPolicyTest,
    testing::Combine(testing::Values('\r', '\n', 'Z', 'q', '\033'),
                     testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD, SM_NEWSGROUP, SM_UNIVERSAL), testing::Bool()));

class SelectorPartialAcceptTest : public SelectorExitStateTest,
                                  public testing::WithParamInterface<std::tuple<char, SelectionMode, bool>>
{
};

TEST_P(SelectorPartialAcceptTest, enterAcceptsAPartialCurrentGroupWithoutSelectingItsOtherArticles)
{
    const auto [command, mode, rereading] = GetParam();
    prepare_exit(mode, rereading);
    ASSERT_NO_FATAL_FAILURE(set_selection({2}));

    const DisplayState result = dispatch(command);

    EXPECT_EQ(DS_QUIT, result);
    expect_exit({2});
}

INSTANTIATE_TEST_SUITE_P(CommandModeAndReadScope, SelectorPartialAcceptTest,
                         testing::Combine(testing::Values('\r', '\n'), testing::Values(SM_SUBJECT, SM_THREAD),
                                          testing::Bool()));

} // namespace
