// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/edit_dist.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <tuple>

namespace
{

using EditDistanceCase = std::tuple<std::string_view, std::string_view, int>;

class EditDistanceInputTest : public testing::TestWithParam<EditDistanceCase>
{
protected:
    void SetUp() override;

    std::string m_from;
    std::string m_to;
};

void EditDistanceInputTest::SetUp()
{
    m_from = std::get<0>(GetParam());
    m_to = std::get<1>(GetParam());
}

} // namespace

TEST_P(EditDistanceInputTest, measuresTheDistanceBetweenTheInputPair)
{
    const int distance = edit_distn(m_from, m_to);

    EXPECT_EQ(std::get<2>(GetParam()), distance);
}

INSTANTIATE_TEST_SUITE_P(EmptyOperands, EditDistanceInputTest,
                         testing::Values(EditDistanceCase{"", "", 0}, EditDistanceCase{"", "abc", 3},
                                         EditDistanceCase{"abc", "", 3}));

INSTANTIATE_TEST_SUITE_P(CommonEdits, EditDistanceInputTest,
                         testing::Values(EditDistanceCase{"comp.lang.c", "comp.lang.c", 0},
                                         EditDistanceCase{"comp.lang.c", "comp.lang.cpp", 2},
                                         EditDistanceCase{"comp.lang.c", "comp.lang.x", 1},
                                         EditDistanceCase{"ab", "ba", 1}, EditDistanceCase{"kitten", "sitting", 3}));

TEST(EditDistanceTest, handlesLongInputs)
{
    std::string from(600, 'a');
    std::string to{from};

    to[300] = 'b';

    EXPECT_EQ(1, edit_distn(from, to));
}
