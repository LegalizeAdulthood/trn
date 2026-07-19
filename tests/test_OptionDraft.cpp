// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/OptionDraft.h>

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>

TEST(OptionDraftTest, storesOwnedValuesByOptionIndex)
{
    OptionDraft draft{4};
    std::string source{"draft value"};

    draft.set(2, source);
    source = "changed";

    const std::optional<std::string_view> value = draft.value(2);
    EXPECT_TRUE(draft.contains(2));
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(std::string_view{"draft value"}, *value);
    EXPECT_NE(source.data(), value->data());
    EXPECT_EQ(1U, draft.size());
}

TEST(OptionDraftTest, replacingValueKeepsOneSelection)
{
    OptionDraft draft{4};

    draft.set(2, "first");
    draft.set(2, "second");

    const std::optional<std::string_view> value = draft.value(2);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(std::string_view{"second"}, *value);
    EXPECT_EQ(1U, draft.size());
}

TEST(OptionDraftTest, eraseAndClearDiscardOwnedEdits)
{
    OptionDraft draft{4};

    draft.set(1, "one");
    draft.set(2, "two");
    draft.erase(1);

    EXPECT_FALSE(draft.contains(1));
    const std::optional<std::string_view> value = draft.value(2);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(std::string_view{"two"}, *value);
    EXPECT_EQ(1U, draft.size());

    draft.clear();

    EXPECT_FALSE(draft.contains(2));
    EXPECT_FALSE(draft.value(2).has_value());
    EXPECT_EQ(0U, draft.size());
}

TEST(OptionDraftTest, invalidIndexesAreIgnored)
{
    OptionDraft draft{2};

    draft.set(-1, "negative");
    draft.set(2, "past-end");
    draft.erase(2);

    EXPECT_FALSE(draft.value(-1).has_value());
    EXPECT_FALSE(draft.value(2).has_value());
    EXPECT_EQ(0U, draft.size());
}
