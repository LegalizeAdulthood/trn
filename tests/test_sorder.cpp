// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/scan.h>
#include <trn/sorder.h>

#include <gtest/gtest.h>

TEST(ScanOrderTest, sortsAndRestoresIndependentContexts)
{
    const int first_context = s_new_context(S_GROUP);
    s_change_context(first_context);
    s_order_add(3);
    s_order_add(1);
    s_order_add(2);
    s_order_add(2);

    EXPECT_EQ(1, s_first());
    EXPECT_EQ(2, s_next(1));
    EXPECT_EQ(3, s_next(2));
    EXPECT_EQ(0, s_next(3));
    EXPECT_EQ(0, s_prev(1));
    EXPECT_EQ(2, s_prev(3));
    EXPECT_EQ(3, s_last());
    s_save_context();

    const int second_context = s_new_context(S_GROUP);
    s_change_context(second_context);
    s_order_add(20);
    s_order_add(10);

    EXPECT_EQ(10, s_first());
    EXPECT_EQ(20, s_next(10));
    EXPECT_EQ(0, s_next(20));
    s_save_context();

    s_change_context(first_context);
    EXPECT_EQ(1, s_first());
    EXPECT_EQ(2, s_next(1));
    EXPECT_EQ(3, s_last());

    s_change_context(second_context);
    EXPECT_EQ(10, s_first());
    EXPECT_EQ(20, s_last());

    s_delete_context(second_context);
    s_change_context(first_context);
    s_delete_context(first_context);
}
