// This file Copyright 1993 by Clifford A. Adams
/* score-easy.c
 *
 * Simple interactive menus for scorefile tasks.
 */

#include "config/common.h"
#include "trn/score-easy.h"

#include "trn/terminal.h"
#include "trn/util.h"

#include <cstdio>
#include <cstring>

// new line to return to the caller.
static char s_sc_e_newline[LINE_BUF_LEN];

// returns new string or nullptr to abort.
char *sc_easy_append()
{
    char ch;

    char  filechar = '\0'; // GCC warning avoidance
    char *s = s_sc_e_newline;
    std::printf("\nScorefile easy append mode.\n");
    bool q_done = false;
    while (!q_done)
    {
        std::printf("0) Exit.\n");
        std::printf("1) List the current scorefile abbreviations.\n");
        std::printf("2) Add an entry to the global scorefile.\n");
        std::printf("3) Add an entry to this newsgroup's scorefile.\n");
        std::printf("4) Add an entry to another scorefile.\n");
        std::printf("5) Use a temporary scoring rule.\n");
        ch = menu_get_char();
        q_done = true;
        switch (ch)
        {
        case '0':
            return nullptr;

        case '1':
            std::strcpy(s_sc_e_newline,"?");
            return s_sc_e_newline;

        case '2':
            filechar = '*';
            break;

        case '3':
            filechar = '"';
            break;

        case '4':
            filechar = '\0';
            break;

        case '5':
            filechar = '!';
            break;

        case 'h':
            std::printf("No help available (yet).\n");
            q_done = false;
            break;

        default:
            q_done = false;
            break;
        }
    }
    while (filechar == '\0')    // choose one
    {
        std::printf("Type the (single character) abbreviation of the scorefile:");
        std::fflush(stdout);
        eat_typeahead();
        get_cmd(g_buf);
        std::printf("%c\n",*g_buf);
        filechar = *g_buf;
        // If error checking is done later, then an error should set
        // filechar to '\0' and continue the while loop.
    }
    *s++ = filechar;
    *s++ = ' ';
    q_done = false;
    while (!q_done)
    {
        std::printf("What type of line do you want to add?\n");
        std::printf("0) Exit.\n");
        std::printf("1) A scoring rule line.\n");
        std::printf("   (for the current article's author/subject)\n");
        std::printf("2) A command, comment, or other kind of line.\n");
        std::printf("   (use this for any other kind of line)\n");
        std::printf("\n[Other line formats will be supported later.]\n");
        ch = menu_get_char();
        q_done = true;
        switch (ch)
        {
        case '0':
            return nullptr;

        case '1':
            break;

        case '2':
            std::printf("Enter the line below:\n");
            std::fflush(stdout);
            g_buf[0] = '>';
            g_buf[1] = FINISH_CMD;
            if (finish_command(true))
            {
                std::sprintf(s,"%s",g_buf+1);
                return s_sc_e_newline;
            }
            std::printf("\n");
            q_done = false;
            break;

        case 'h':
            std::printf("No help available (yet).\n");
            q_done = false;
            break;

        default:
            q_done = false;
            break;
        }
    }
    q_done = false;
    while (!q_done)
    {
        std::printf("Enter a score amount (like 10 or -6):");
        std::fflush(stdout);
        g_buf[0] = ' ';
        g_buf[1] = FINISH_CMD;
        if (finish_command(true))
        {
            long score = atoi(g_buf + 1);
            if (score == 0)
            {
                if (g_buf[1] != '0')
                {
                    continue;   // the while loop
                }
            }
            std::sprintf(s,"%ld",score);
            s = s_sc_e_newline+std::strlen(s_sc_e_newline); // point at terminator
            *s++ = ' ';
            q_done = true;
        }
        else
        {
            std::printf("\n");
        }
    }
    q_done = false;
    while (!q_done)
    {
        std::printf("Do you want to:\n");
        std::printf("0) Exit.\n");
        std::printf("1) Give the score to the current subject.\n");
        std::printf("2) Give the score to the current author.\n");
// add some more options here later
// perhaps fold regular-expression question here?
        ch = menu_get_char();
        q_done = true;
        switch (ch)
        {
        case '0':
            return nullptr;

        case '1':
            *s++ = 'S';
            *s++ = '\0';
            return s_sc_e_newline;

        case '2':
            *s++ = 'F';
            *s++ = '\0';
            return s_sc_e_newline;

        case 'h':
            std::printf("No help available (yet).\n");
            q_done = false;
            break;

        default:
            q_done = false;
            break;
        }
    }
    // later ask for headers, pattern-matching, etc...
    return nullptr;
}

// returns new string or nullptr to abort.
const char *sc_easy_command()
{
    std::printf("\nScoring easy command mode.\n");
    bool q_done = false;
    while (!q_done)
    {
        std::printf("0) Exit.\n");
        std::printf("1) Add something to a scorefile.\n");
        std::printf("2) Rescore the articles in the current newsgroup.\n");
        std::printf("3) Explain the current article's score.\n");
        std::printf("   (show the rules that matched this article)\n");
        std::printf("4) Edit this newsgroup's scoring rule file.\n");
        // later add an option to edit an arbitrary file
        std::printf("5) Continue scoring unscored articles.\n");
        char ch = menu_get_char();
        q_done = true;
        switch (ch)
        {
        case '0':
            return nullptr;

        case '1':
            return "\"";        // do an append command

        case '2':
            return "r";

        case '3':
            return "s";

        case '4':
            // add more later
            return "e";

        case '5':
            return "f";

        case 'h':
            std::printf("No help available (yet).\n");
            q_done = false;
            break;

        default:
            q_done = false;
            break;
        }
    }
    return nullptr;
}
