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

#include "wildmat/wildmat.h"

    // What character marks an inverted character class?
#define NEGATE_CLASS            '^'

#define OPTIMIZE_JUST_STAR // Is "*" a common pattern?
#undef MATCH_TAR_PATTERN   // Do tar(1) matching rules, which ignore a trailing slash?

static bool do_match(const char *text, const char *p);

//
//  Match text and p, return true, false.
//
static bool do_match(const char *text, const char *p)
{
    for (; *p; text++, p++)
    {
        if (*text == '\0' && *p != '*')
        {
            return false;
        }
        switch (*p)
        {
        case '\\':
            // Literal match with following character.
            p++;
            // FALLTHROUGH

        default:
            if (*text != *p)
            {
                return false;
            }
            continue;

        case '?':
            // Match anything.
            continue;

        case '*':
            while (*++p == '*')
            {
                // Consecutive stars act just like one.
            }
            if (*p == '\0')
            {
                // Trailing star matches everything.
                return true;
            }
            while (*text)
            {
                if (do_match(text++, p))
                {
                    return true;
                }
            }
            return false;

        case '[':
        {
            const bool reverse = p[1] == NEGATE_CLASS;
            if (reverse)
            {
                // Inverted character class.
                p++;
            }
            bool matched{};
            for (int last = 0400; *++p && *p != ']'; last = *p)
            {
                // This next line requires a good C compiler.
                if (*p == '-' ? *text <= *++p && *text >= last : *text == *p)
                {
                    matched = true;
                }
            }
            if (matched == reverse)
            {
                return false;
            }
            continue;
        }
        }
    }

#ifdef  MATCH_TAR_PATTERN
    if (*text == '/')
    {
        return true;
    }
#endif  // MATCH_TAR_ATTERN
    return *text == '\0';
}


//
//  User-level routine.  Returns true or false.
//
bool wildcard_match(const char *text, const char *p)
{
#ifdef  OPTIMIZE_JUST_STAR
    if (p[0] == '*' && p[1] == '\0')
    {
        return true;
    }
#endif  // OPTIMIZE_JUST_STAR
    return do_match(text, p);
}

#ifdef  TEST
#include <stdio.h>

int main()
{
    // Yes, we use gets not fgets.  Sue me.
    extern char* gets();
    char         p[80];
    char         text[80];

    printf("Wildmat tester.  Enter pattern, then strings to test.\n");
    printf("A blank line gets prompts for a new pattern; a blank pattern\n");
    printf("exits the program.\n");

    while (true)
    {
        printf("\nEnter pattern:  ");
        (void)fflush(stdout);
        if (gets(p) == nullptr || p[0] == '\0')
        {
            break;
        }
        while (true)
        {
            printf("Enter text:  ");
            (void)fflush(stdout);
            if (gets(text) == nullptr)
            {
                exit(0);
            }
            if (text[0] == '\0')
            {
                // Blank line; go back and get a new pattern.
                break;
            }
            printf("      %s\n", wildmat(text, p) ? "YES" : "NO");
        }
    }

    return 0;
}
#endif  // TEST
