#include <string>

#include <gtest/gtest.h>

#include <parsedate.h>

TEST(ParseDateTest, oldStyleDate)
{
    const time_t time = parsedate("Wed Dec 3 12:23:34 1980");

    ASSERT_NE(-1, time);
    tm *utc_time = gmtime(&time);
    char buffer[80];
    asctime_s(buffer, sizeof(buffer), utc_time);
    ASSERT_EQ("Wed Dec  3 12:23:34 1980\n", std::string{buffer});
}
