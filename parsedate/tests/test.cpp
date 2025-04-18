#include <string>

#include <gtest/gtest.h>

#include <parsedate/parsedate.h>

TEST(ParseDateTest, oldStyleDate)
{
    const std::time_t time = parsedate("Wed Dec 3 12:23:34 1980");

    ASSERT_NE(-1, time);
    tm *utc_time = gmtime(&time);
    char buffer[80];
#ifdef WIN32
    asctime_s(buffer, sizeof(buffer), utc_time);
#else
    asctime_r(utc_time, buffer);
#endif
    ASSERT_EQ("Wed Dec  3 12:23:34 1980\n", std::string{buffer});
}
