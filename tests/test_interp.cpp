#include <gtest/gtest.h>

#include <memory>

#include "common.h"
#include "intrp.h"

TEST(Interpolator, noEscapes)
{
    std::unique_ptr<char> buffer(new char[4096]);
    char pattern[]{"this string contains no escapes"};

    const char *new_pattern = dointerp(buffer.get(), 4096, pattern, "", nullptr);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{pattern}, std::string{buffer.get()});
}
