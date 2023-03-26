#include <gtest/gtest.h>

#include <array>

#include "common.h"
#include "env.h"
#include "init.h"

namespace
{

struct InitTest : testing::Test
{
protected:
    void SetUp() override
    {
        putenv("HOME=");
        putenv("LOGDIR=");
    }
};

} // namespace

TEST_F(InitTest, homeDirFromHOME)
{
    std::array<char, TCBUF_SIZE> tcbuf{};
    const bool                   lax{true};
    std::string                  home{"C:\\users\\bonzo"};
    putenv(("HOME=" + home).c_str());

    env_init(tcbuf.data(), lax);

    ASSERT_EQ(home, std::string{g_home_dir});
}

TEST_F(InitTest, homeDirFromLOGDIR)
{
    std::array<char, TCBUF_SIZE> tcbuf{};
    const bool                   lax{true};
    std::string                  logDir{"C:\\users\\bonzo"};
    putenv(("LOGDIR=" + logDir).c_str());

    env_init(tcbuf.data(), lax);

    ASSERT_EQ(logDir, std::string{g_home_dir});
}
