// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/OptionDraft.h>

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>

namespace
{

class OwnedDraftEditsTest : public testing::Test
{
protected:
    void SetUp() override;

    OptionDraft m_draft{4};
};

void OwnedDraftEditsTest::SetUp()
{
    m_draft.set(1, "one");
    m_draft.set(2, "two");
}

class InvalidDraftIndexTest : public testing::TestWithParam<int>
{
protected:
    OptionDraft m_draft{2};
};

} // namespace

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

TEST_F(OwnedDraftEditsTest, erasingOneEditPreservesTheOther)
{
    m_draft.erase(1);

    EXPECT_FALSE(m_draft.contains(1));
    const std::optional<std::string_view> value = m_draft.value(2);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(std::string_view{"two"}, *value);
    EXPECT_EQ(1U, m_draft.size());
}

TEST_F(OwnedDraftEditsTest, clearingDiscardsTheRemainingEdit)
{
    m_draft.erase(1);

    m_draft.clear();

    EXPECT_FALSE(m_draft.contains(2));
    EXPECT_FALSE(m_draft.value(2).has_value());
    EXPECT_EQ(0U, m_draft.size());
}

TEST_P(InvalidDraftIndexTest, settingAnInvalidIndexDoesNotCreateAnEdit)
{
    m_draft.set(GetParam(), "invalid");

    EXPECT_FALSE(m_draft.value(GetParam()).has_value());
    EXPECT_EQ(0U, m_draft.size());
}

TEST_P(InvalidDraftIndexTest, erasingAnInvalidIndexPreservesExistingEdits)
{
    m_draft.set(0, "kept");

    m_draft.erase(GetParam());

    EXPECT_EQ(1U, m_draft.size());
    EXPECT_EQ((std::optional<std::string_view>{"kept"}), m_draft.value(0));
}

TEST_P(InvalidDraftIndexTest, lookingUpAnInvalidIndexReturnsNoValue)
{
    const std::optional<std::string_view> value = m_draft.value(GetParam());

    EXPECT_FALSE(value.has_value());
    EXPECT_EQ(0U, m_draft.size());
}

INSTANTIATE_TEST_SUITE_P(NegativeAndPastEnd, InvalidDraftIndexTest, testing::Values(-1, 2));
