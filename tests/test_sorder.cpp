// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/scan.h>
#include <trn/sorder.h>

#include <gtest/gtest.h>

#include <tuple>
#include <vector>

namespace
{

class ScanOrderTest : public testing::Test
{
protected:
    void              SetUp() override;
    void              TearDown() override;
    std::vector<long> current_entries();

    int m_first_context{-1};
    int m_second_context{-1};
};

void ScanOrderTest::SetUp()
{
    m_first_context = s_new_context(S_GROUP);
    s_change_context(m_first_context);
    s_order_add(3);
    s_order_add(1);
    s_order_add(2);
    s_order_add(2);
}

void ScanOrderTest::TearDown()
{
    if (m_second_context >= 0)
    {
        s_delete_context(m_second_context);
    }
    if (m_first_context >= 0)
    {
        s_change_context(m_first_context);
        s_delete_context(m_first_context);
    }
    g_s_cur_type = S_NONE;
    g_s_order_changed = false;
}

std::vector<long> ScanOrderTest::current_entries()
{
    std::vector<long> result;
    for (long entry = s_first(); entry != 0; entry = s_next(entry))
    {
        result.push_back(entry);
    }
    return result;
}

class SortedScanOrderTest : public ScanOrderTest
{
protected:
    void SetUp() override;
};

void SortedScanOrderTest::SetUp()
{
    ScanOrderTest::SetUp();
    s_sort();
}

class ScanOrderEndpointTest : public SortedScanOrderTest, public testing::WithParamInterface<bool>
{
};

using ScanOrderStepCase = std::tuple<bool, long, long>;

class ScanOrderStepTest : public SortedScanOrderTest, public testing::WithParamInterface<ScanOrderStepCase>
{
};

class IndependentScanContextsTest : public SortedScanOrderTest
{
protected:
    void SetUp() override;
};

void IndependentScanContextsTest::SetUp()
{
    SortedScanOrderTest::SetUp();
    s_save_context();
    m_second_context = s_new_context(S_GROUP);
    s_change_context(m_second_context);
    s_order_add(20);
    s_order_add(10);
    s_sort();
    s_save_context();
}

class RestoredScanContextTest : public IndependentScanContextsTest, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void RestoredScanContextTest::SetUp()
{
    IndependentScanContextsTest::SetUp();
    if (GetParam())
    {
        s_change_context(m_first_context);
    }
}

} // namespace

TEST_F(ScanOrderTest, sortsEntriesAndDiscardsDuplicates)
{
    s_sort();

    EXPECT_EQ((std::vector<long>{1, 2, 3}), g_s_contexts[m_first_context].ent_sort);
    EXPECT_EQ(2, g_s_ent_sorted_max);
}

TEST_P(ScanOrderEndpointTest, returnsTheRequestedEndOfTheOrder)
{
    const long entry = GetParam() ? s_last() : s_first();

    EXPECT_EQ(GetParam() ? 3 : 1, entry);
}

INSTANTIATE_TEST_SUITE_P(FirstAndLast, ScanOrderEndpointTest, testing::Bool());

TEST_P(ScanOrderStepTest, stepsWithinTheOrderOrStopsAtItsBoundary)
{
    const long entry = std::get<0>(GetParam()) ? s_prev(std::get<1>(GetParam())) : s_next(std::get<1>(GetParam()));

    EXPECT_EQ(std::get<2>(GetParam()), entry);
}

INSTANTIATE_TEST_SUITE_P(DirectionAndPosition, ScanOrderStepTest,
                         testing::Values(ScanOrderStepCase{false, 1, 2}, ScanOrderStepCase{false, 2, 3},
                                         ScanOrderStepCase{false, 3, 0}, ScanOrderStepCase{true, 1, 0},
                                         ScanOrderStepCase{true, 3, 2}));

TEST_F(IndependentScanContextsTest, newContextUsesAnIndependentOrder)
{
    const std::vector<long> entries = current_entries();

    EXPECT_EQ((std::vector<long>{10, 20}), entries);
    EXPECT_EQ((std::vector<long>{1, 2, 3}), g_s_contexts[m_first_context].ent_sort);
}

TEST_P(RestoredScanContextTest, restoresTheSavedOrderWhenReturningToAContext)
{
    s_change_context(GetParam() ? m_second_context : m_first_context);
    const std::vector<long> entries = current_entries();

    EXPECT_EQ((GetParam() ? std::vector<long>{10, 20} : std::vector<long>{1, 2, 3}), entries);
    EXPECT_EQ(GetParam() ? 20 : 3, s_last());
}

INSTANTIATE_TEST_SUITE_P(FirstAndSecond, RestoredScanContextTest, testing::Bool());
