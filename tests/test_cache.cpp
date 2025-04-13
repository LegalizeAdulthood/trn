/* test_cache.c - unit tests for the changed portions of cache.c
 * vi: set sw=4 ts=8 ai sm noet :
 */
#include <gtest/gtest.h>

#include "config/common.h"
#include "trn/cache.h"

#include <string.h>

using namespace testing;

class DectrlTest : public Test
{
protected:
    void TearDown() override
    {
        if (m_buffer != nullptr)
        {
            free(m_buffer);
        }
    }

    void configure_unchanged(const char *before)
    {
        m_before = before;
        m_buffer = strdup(m_before);
        m_expected = before;
    }
    void configure_before_expected(const char *before, const char *expected)
    {
        m_before = before;
        m_buffer = strdup(m_before);
        m_expected = expected;
    }

    const char *m_before{};
    char *m_buffer{};
    const char *m_expected{};
};

TEST_F(DectrlTest, nullptr)
{
    dectrl(nullptr);
}

TEST_F(DectrlTest, ascii_all_printable)
{
    configure_unchanged("This is a test.");

    dectrl(m_buffer);

    ASSERT_STREQ(m_expected, m_buffer) << "dectrl changed an ASCII string with all-printable characters";
}

TEST_F(DectrlTest, ascii_some_nonprintable)
{
    configure_before_expected("This\tis\fa\177test.", "This is a test.");

    dectrl(m_buffer);

    ASSERT_STREQ(m_expected, m_buffer) << "dectrl did not change an ASCII string with some non-printable characters";
}

TEST_F(DectrlTest, iso_8859_1)
{
    configure_unchanged("\302\253\302\240 La Libert\303\251 guidant le peuple. \302\240\302\273");

    dectrl(m_buffer);

    ASSERT_STREQ(m_expected, m_buffer) << "dectrl changed an ISO8859-1 string with all-printable characters";
}

TEST_F(DectrlTest, iso_8859_1_non_printable)
{
    configure_before_expected("\302\253\302\240 La Libert\303\251 guidant\tle peuple. \302\240\302\273",
                              "\302\253\302\240 La Libert\303\251 guidant le peuple. \302\240\302\273");

    dectrl(m_buffer);

    ASSERT_STREQ(m_expected, m_buffer) << "dectrl did not change an ISO8859-1 string with non-printable characters";
}

TEST_F(DectrlTest, cjk_basic)
{
    configure_unchanged(
        "\345\257\247\345\214\226\351\243\233\347\201\260\357\274\214\344\270\215\344\275\234\346\265\256\345\241\265");

    dectrl(m_buffer);

    ASSERT_STREQ(m_expected, m_buffer) << "dectrl changed a CJK string with all-printable characters";
}

TEST_F(DectrlTest, cjk_basic_non_printable)
{
    configure_before_expected("\345\257\247\345\214\226\351\243\233\347\201\260\357\274\214\t"
                              "\344\270\215\344\275\234\346\265\256\345\241\265",
                              "\345\257\247\345\214\226\351\243\233\347\201\260\357\274\214 "
                              "\344\270\215\344\275\234\346\265\256\345\241\265");

    dectrl(m_buffer);

    ASSERT_STREQ(m_expected, m_buffer) << "dectrl did not change a CJK string with non-printable characters";
}
