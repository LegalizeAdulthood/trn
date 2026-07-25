/*  $Revision$
**
**  Do shell-style pattern matching for ?, \, [], and * characters.
**  Might not be robust in face of malformed patterns; e.g., "foo[a-"
**  could cause a segmentation violation.  It is 8bit clean.
**
**  Written by Rich $alz, mirror!rs, Wed Nov 26 19:03:17 EST 1986.
**  Rich $alz is now <rsalz@bbn.com>.
**  April, 1991:  Replaced mutually-recursive calls with in-line code
**  for the star character.
**
**  Special thanks to Lars Mathiesen <thorinn@diku.dk> for the ABORT code.
**  This can greatly speed up failing wildcard patterns.  For example:
**      pattern: -*-*-*-*-*-*-12-*-*-*-m-*-*-*
**      text 1:  -adobe-courier-bold-o-normal--12-120-75-75-m-70-iso8859-1
**      text 2:  -adobe-courier-bold-o-normal--12-120-75-75-X-70-iso8859-1
**  Text 1 matches with 51 calls, while text 2 fails with 54 calls.  Without
**  the ABORT, then it takes 22310 calls to fail.  Ugh.  The following
**  explanation is from Lars:
**  The precondition that must be fulfilled is that DoMatch will consume
**  at least one character in text.  This is true if *p is neither '*' nor
**  '\0'.)  The last return has ABORT instead of false to avoid quadratic
**  behaviour in cases like pattern "*a*b*c*d" with text "abcxxxxx".  With
**  false, each star-loop has to run to the end of the text; with ABORT
**  only the last one does.
**
**  Once the control of one instance of DoMatch enters the star-loop, that
**  instance will return either true or ABORT, and any calling instance
**  will therefore return immediately after (without calling recursively
**  again).  In effect, only one star-loop is ever active.  It would be
**  possible to modify the code to maintain this context explicitly,
**  eliminating all recursive calls at the cost of some complication and
**  loss of clarity (and the ABORT stuff seems to be unclear enough by
**  itself).  I think it would be unwise to try to get this into a
**  released version unless you have a good test data base to try it out
**  on.
*/

#include <wildmat/wildmat.h>

    // What character marks an inverted character class?
#define NEGATE_CLASS            '^'

#define OPTIMIZE_JUST_STAR // Is "*" a common pattern?
#undef MATCH_TAR_PATTERN   // Do tar(1) matching rules, which ignore a trailing slash?

static bool do_match(std::string_view text, std::string_view pattern);

//
//  Match text and p, return true, false.
//
static bool do_match(std::string_view text, std::string_view pattern)
{
    std::size_t text_pos{};
    std::size_t pattern_pos{};

    while (pattern_pos < pattern.size())
    {
        if (text_pos == text.size() && pattern[pattern_pos] != '*')
        {
            return false;
        }
        switch (pattern[pattern_pos])
        {
        case '\\':
            // Literal match with following character.
            ++pattern_pos;
            if (pattern_pos == pattern.size())
            {
                return false;
            }
            // FALLTHROUGH

        default:
            if (text[text_pos] != pattern[pattern_pos])
            {
                return false;
            }
            ++text_pos;
            ++pattern_pos;
            break;

        case '?':
            // Match anything.
            ++text_pos;
            ++pattern_pos;
            break;

        case '*':
            while (++pattern_pos < pattern.size() && pattern[pattern_pos] == '*')
            {
                // Consecutive stars act just like one.
            }
            if (pattern_pos == pattern.size())
            {
                // Trailing star matches everything.
                return true;
            }
            while (text_pos < text.size())
            {
                if (do_match(text.substr(text_pos), pattern.substr(pattern_pos)))
                {
                    return true;
                }
                ++text_pos;
            }
            return false;

        case '[':
        {
            const bool  reverse = pattern_pos + 1 < pattern.size() && pattern[pattern_pos + 1] == NEGATE_CLASS;
            std::size_t class_pos = pattern_pos + (reverse ? 2 : 1);
            bool        matched{};
            int         last = 0400;
            while (class_pos < pattern.size() && pattern[class_pos] != ']')
            {
                // This next line requires a good C compiler.
                const char ch = pattern[class_pos++];
                if (ch == '-' && class_pos < pattern.size())
                {
                    const char range_end = pattern[class_pos++];
                    if (text[text_pos] <= range_end && text[text_pos] >= last)
                    {
                        matched = true;
                    }
                    last = range_end;
                }
                else
                {
                    if (text[text_pos] == ch)
                    {
                        matched = true;
                    }
                    last = ch;
                }
            }
            if (matched == reverse)
            {
                return false;
            }
            ++text_pos;
            pattern_pos = class_pos;
            if (pattern_pos < pattern.size() && pattern[pattern_pos] == ']')
            {
                ++pattern_pos;
            }
            break;
        }
        }
    }

#ifdef  MATCH_TAR_PATTERN
    if (text_pos < text.size() && text[text_pos] == '/')
    {
        return true;
    }
#endif  // MATCH_TAR_ATTERN
    return text_pos == text.size();
}


//
//  User-level routine.  Returns true or false.
//
bool wildcard_match(std::string_view text, std::string_view pattern)
{
#ifdef  OPTIMIZE_JUST_STAR
    if (pattern == "*")
    {
        return true;
    }
#endif  // OPTIMIZE_JUST_STAR
    return do_match(text, pattern);
}

#ifdef  TEST
#include <iostream>
#include <string>

int main()
{
    std::string pattern;
    std::string text;

    std::cout << "Wildmat tester.  Enter pattern, then strings to test.\n";
    std::cout << "A blank line gets prompts for a new pattern; a blank pattern\n";
    std::cout << "exits the program.\n";

    while (true)
    {
        std::cout << "\nEnter pattern:  " << std::flush;
        if (!std::getline(std::cin, pattern) || pattern.empty())
        {
            break;
        }
        while (true)
        {
            std::cout << "Enter text:  " << std::flush;
            if (!std::getline(std::cin, text))
            {
                return 0;
            }
            if (text.empty())
            {
                // Blank line; go back and get a new pattern.
                break;
            }
            std::cout << "      " << (wildcard_match(text, pattern) ? "YES" : "NO") << '\n';
        }
    }

    return 0;
}
#endif  // TEST
