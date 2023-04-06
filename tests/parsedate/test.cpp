#include <gtest/gtest.h>

#include <parsedate.h>

TEST(ParseDateTest, failed)
{
    char date[]{"Wed Dec 3 12:00:00 1980"};

    time_t time = parsedate(date);

    ASSERT_NE(-1, time);
}
