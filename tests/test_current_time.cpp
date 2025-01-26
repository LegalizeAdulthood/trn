#include <util.h>

#include <chrono>
#include <thread>

#include <gtest/gtest.h>

TEST(TestCurrentTime, msAccuracy)
{
    using namespace std::chrono_literals;
    const double start{current_time()};
    std::this_thread::sleep_for(100ms);

    const double now{current_time()};

    EXPECT_GT(now, start);
    EXPECT_LE(0.1, now - start);
}
