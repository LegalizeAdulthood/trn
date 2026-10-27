// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include "support/NewsgroupPageState.h"

#include <config/common.h>
#include <trn/cache.h>
#include <trn/hash.h>
#include <trn/rt-select-internal.h>
#include <trn/Subject.h>
#include <trn/univ.h>

#include <algorithm>
#include <array>
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

private:
    void             prepare_articles(int count);
    void             prepare_groups(int count);
    void             prepare_menu(int count);
    std::vector<int> flags() const;
    int              row_number(int index) const;

    std::array<Subject, 6>   m_subjects{};
    std::vector<int>         m_flags;
    std::string              m_rendered;
    ArticleUnread            m_selected{};
    int                      m_selected_subjects{};
    decltype(g_article_list) m_old_articles{std::move(g_article_list)};
    decltype(g_art_ptr_list) m_old_article_ptrs{std::move(g_art_ptr_list)};
    Article                **m_old_art_ptr{g_art_ptr};
    ArticleNum               m_old_abs_first{g_abs_first};
    ArticleNum               m_old_first{g_first_art};
    ArticleNum               m_old_last{g_last_art};
    ArticleNum               m_old_first_cached{g_first_cached};
    ArticleNum               m_old_last_cached{g_last_cached};
    bool                     m_old_cached_all{g_cached_all_in_range};
    Subject                 *m_old_first_subject{g_first_subject};
    Subject                 *m_old_last_subject{g_last_subject};
    int                      m_old_subject_count{g_subject_count};
    Article                **m_old_page_app{g_sel_page_app};
    Article                **m_old_next_app{g_sel_next_app};
    Subject                 *m_old_page_sp{g_sel_page_sp};
    Subject                 *m_old_next_sp{g_sel_next_sp};
    int                      m_old_selected_subjects{g_selected_subj_cnt};
    bool                     m_old_selected_only{g_selected_only};
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
    UniversalItemList        m_old_menu{std::move(g_univ_items)};
    UniversalItemIndex       m_old_menu_page{g_sel_page_univ_index};
    UniversalItemIndex       m_old_menu_next{g_sel_next_univ_index};
    std::string              m_old_title{g_univ_title};
    int                      m_old_level{g_univ_level};
    HashTable               *m_old_hash{g_newsrc_hash};
};

void SelectorPagingStateTest::SetUp()
{
    NewsgroupPageDisplayState::SetUp();
    g_article_list.clear();
    g_art_ptr_list.clear();
    g_art_ptr = nullptr;
    g_sel_page_app = g_sel_next_app = nullptr;
    g_sel_page_sp = g_sel_next_sp = nullptr;
    g_univ_items.clear();
    g_univ_items.reserve(6);
    g_sel_page_univ_index = g_sel_next_univ_index = {};
    g_tc_LINES = 7;
    g_can_home = false;
    g_selected_only = false;
    g_redirected = false;
    g_newsgroup_name = "comp.paging";
    g_moderated.clear();
    g_current_newsgroup = g_newsgroup_ptr = nullptr;
    g_sel_art_display_mode = "s";
    g_sel_art_display_mode_index = 0;
    g_sel_grp_display_mode = "s";
    g_sel_grp_display_mode_index = 0;
    g_sel_item_index = 0;
    g_univ_title = "Paging menu";
    g_univ_level = 1;
    g_selected_count = TR_NONE;
    g_selected_subj_cnt = 0;
    g_newsrc_hash = hash_create(17, [](std::string_view key, HashDatum data)
                                { return key.compare(reinterpret_cast<NewsgroupData *>(data.dat_ptr)->rc_name()); });
}

void SelectorPagingStateTest::TearDown()
{
    g_article_list = std::move(m_old_articles);
    g_art_ptr_list = std::move(m_old_article_ptrs);
    g_art_ptr = m_old_art_ptr;
    g_abs_first = m_old_abs_first;
    g_first_art = m_old_first;
    g_last_art = m_old_last;
    g_first_cached = m_old_first_cached;
    g_last_cached = m_old_last_cached;
    g_cached_all_in_range = m_old_cached_all;
    g_first_subject = m_old_first_subject;
    g_last_subject = m_old_last_subject;
    g_subject_count = m_old_subject_count;
    g_sel_page_app = m_old_page_app;
    g_sel_next_app = m_old_next_app;
    g_sel_page_sp = m_old_page_sp;
    g_sel_next_sp = m_old_next_sp;
    g_selected_subj_cnt = m_old_selected_subjects;
    g_selected_only = m_old_selected_only;
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
    g_univ_items = std::move(m_old_menu);
    g_sel_page_univ_index = m_old_menu_page;
    g_sel_next_univ_index = m_old_menu_next;
    g_univ_title = m_old_title;
    g_univ_level = m_old_level;
    hash_destroy(g_newsrc_hash);
    g_newsrc_hash = m_old_hash;
    NewsgroupPageDisplayState::TearDown();
}

void SelectorPagingStateTest::prepare(SelectionMode mode, bool rereading, int count)
{
    set_read_scope(rereading);
    g_sel_mode = mode;
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
            g_newsgroup_order[3]->m_flags |= NF_DEL;
        }
        else if (mode == SM_UNIVERSAL)
        {
            g_univ_items[3].m_flags |= UF_DEL;
        }
        else
        {
            article_ptr(ArticleNum{4})->m_flags |= AF_DEL;
        }
    }
    m_flags = flags();
    m_selected = g_selected_count;
    m_selected_subjects = g_selected_subj_cnt;
}

void SelectorPagingStateTest::prepare_articles(int count)
{
    g_abs_first = g_first_art = g_first_cached = ArticleNum{1};
    g_last_art = g_last_cached = ArticleNum{count};
    g_cached_all_in_range = true;
    g_subject_count = count;
    g_first_subject = count ? &m_subjects[0] : nullptr;
    g_last_subject = count ? &m_subjects[count - 1] : nullptr;
    for (int index = 0; index < count; ++index)
    {
        Subject &subject = m_subjects[index];
        Article *article = article_ptr(ArticleNum{index + 1});
        subject.m_str = "Re: paging-row-" + std::to_string(index + 1);
        subject.m_prev = index ? &m_subjects[index - 1] : nullptr;
        subject.m_next = index + 1 < count ? &m_subjects[index + 1] : nullptr;
        subject.m_articles = subject.m_thread = article;
        subject.m_thread_link = &subject;
        article->m_subj = &subject;
        article->m_from = "Paging <paging@example.test>";
        article->m_date = index + 1;
        article->m_flags = AF_EXISTS | AF_CACHED | AF_THREADED;
        if (!g_sel_rereading)
        {
            article->m_flags |= AF_UNREAD;
        }
        if (index == 0 || index == 4)
        {
            article->m_flags |= static_cast<ArticleFlags>(g_sel_mask);
        }
    }
    count_subjects(CS_UNSEL_STORE);
}

void SelectorPagingStateTest::prepare_groups(int count)
{
    for (int index = 0; index < count; ++index)
    {
        const NewsgroupFlags selected = index == 0 || index == 4 ? static_cast<NewsgroupFlags>(g_sel_mask) : NF_NONE;
        NewsgroupData       &group = add_group("comp.paging-row-" + std::to_string(index + 1),
                                               g_sel_rereading ? TR_NONE : ArticleUnread{3}, selected);
        group.m_rc = &m_newsrc;
        group.m_rc_line += "1-3";
    }
}

void SelectorPagingStateTest::prepare_menu(int count)
{
    if (count)
    {
        NewsgroupData &group = add_group("comp.paging", g_sel_rereading ? TR_NONE : ArticleUnread{count});
        group.m_rc = &m_newsrc;
        group.m_ng_max = ArticleNum{count};
        group.m_rc_line += g_sel_rereading ? "1-6" : "0";
        hash_store(g_newsrc_hash, group.rc_name(),
                   HashDatum{reinterpret_cast<char *>(&group), static_cast<unsigned>(group.rc_name().size())});
    }
    g_selected_count = count ? ArticleUnread{2} : TR_NONE;
    for (int index = 0; index < count; ++index)
    {
        UniversalItem &item = g_univ_items.emplace_back();
        item.m_num = index + 1;
        item.m_desc = "paging-row-" + std::to_string(index + 1);
        item.m_data = UniversalVirtualArticle{"comp.paging", "<paging-" + std::to_string(index + 1) + "@example.test>",
                                              "Paging", item.m_desc, ArticleNum{index + 1}};
        if (index == 0 || index == 4)
        {
            item.m_flags |= static_cast<UniversalItemFlags>(g_sel_mask);
        }
    }
}

void SelectorPagingStateTest::position_page(int page)
{
    m_rendered = render_page();
    for (int index = 0; index < page; ++index)
    {
        ASSERT_TRUE(next_page());
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
    if (g_sel_mode == SM_NEWSGROUP)
    {
        const auto item = std::find(g_newsgroup_order.begin(), g_newsgroup_order.end(), g_sel_items[index].u.np);
        return static_cast<int>(item - g_newsgroup_order.begin()) + 1;
    }
    if (g_sel_mode == SM_UNIVERSAL)
    {
        return g_sel_items[index].u.un->m_num;
    }
    const Article *article = g_sel_mode == SM_ARTICLE ? g_sel_items[index].u.ap : g_sel_items[index].u.sp->m_articles;
    return static_cast<int>(article->article_num().value_of());
}

std::vector<int> SelectorPagingStateTest::flags() const
{
    std::vector<int> result;
    if (g_sel_mode == SM_NEWSGROUP)
    {
        for (const NewsgroupData *group : g_newsgroup_order)
        {
            result.push_back(group->m_flags);
        }
    }
    else if (g_sel_mode == SM_UNIVERSAL)
    {
        for (const UniversalItem &item : univ_items())
        {
            result.push_back(item.m_flags);
        }
    }
    else
    {
        for (const auto &[number, article] : g_article_list)
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
    EXPECT_EQ(SS_NATURAL, g_sel_sort);
    EXPECT_EQ(1, g_sel_direction);
    for (const NewsgroupData *group : g_newsgroup_order)
    {
        EXPECT_EQ(g_sel_mode == SM_UNIVERSAL ? (g_sel_rereading ? " 1-6" : " 0") : " 1-3", group->rc_numbers());
        EXPECT_EQ(':', group->m_subscribe_char);
    }
}

void SelectorPagingStateTest::expect_page(int page, int cursor, bool empty, bool redraw)
{
    if (redraw)
    {
        m_rendered = render_page();
    }

    EXPECT_EQ(empty ? 0 : 6, g_sel_total_obj_cnt);
    ASSERT_EQ(empty ? 0 : 2, g_sel_page_item_cnt);
    EXPECT_EQ(empty ? 0 : 2, g_sel_page_obj_cnt);
    EXPECT_EQ(empty ? 0 : page * 2, g_sel_prior_obj_cnt);
    EXPECT_EQ(cursor, g_sel_item_index);
    EXPECT_EQ(empty || page == 2, g_sel_at_end);
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
    g_sel_item_index = 1;
    g_term_scrolled = 4;
    g_page_line = 37;
    g_output_chase_phrase = false;

    const DisplayState result = dispatch(command.key);

    EXPECT_EQ(command.result, result);
    EXPECT_EQ(command.key == Ctl('l') ? 1 : 0, g_sel_item_index);
    EXPECT_EQ(0, g_term_scrolled);
    EXPECT_EQ(1, g_page_line);
    EXPECT_TRUE(g_output_chase_phrase);
    EXPECT_EQ(mode, g_sel_mode);
    EXPECT_EQ(rereading, g_sel_rereading);
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
    EXPECT_EQ(0, g_sel_item_index);
    expect_page(0, 0, true, result == DS_DISPLAY);
}

INSTANTIATE_TEST_SUITE_P(
    CommandModeAndReadScope, EmptySelectorPagingCommandTest,
    testing::Combine(testing::Values('>', '<', '^', Ctl('r'), '$', Ctl('l')),
                     testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD, SM_NEWSGROUP, SM_UNIVERSAL), testing::Bool()));

} // namespace
