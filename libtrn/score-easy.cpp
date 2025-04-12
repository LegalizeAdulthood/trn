/* This file Copyright 1993 by Clifford A. Adams */
/* score-easy.c
 *
 * Simple interactive menus for scorefile tasks.
 */

#include "config/common.h"
#include "trn/score-easy.h"

#include "trn/terminal.h"
#include "trn/util.h"

/* new line to return to the caller. */
static char s_sc_e_newline[LBUFLEN];

/* returns new string or nullptr to abort. */
char *sc_easy_append()
{
    char ch;

    char  filechar = '\0'; /* GCC warning avoidance */
    char *s = s_sc_e_newline;
    printf("\nScorefile easy append mode.\n");
    bool q_done = false;
    while (!q_done) {
        printf("0) Exit.\n");
        printf("1) List the current scorefile abbreviations.\n");
        printf("2) Add an entry to the global scorefile.\n");
        printf("3) Add an entry to this newsgroup's scorefile.\n");
        printf("4) Add an entry to another scorefile.\n");
        printf("5) Use a temporary scoring rule.\n");
        ch = menu_get_char();
        q_done = true;
        switch (ch) {
          case '0':
            return nullptr;
          case '1':
            strcpy(s_sc_e_newline,"?");
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
            printf("No help available (yet).\n");
            q_done = false;
            break;
          default:
            q_done = false;
            break;
        }
    }
    while (filechar == '\0') {  /* choose one */
        printf("Type the (single character) abbreviation of the scorefile:");
        fflush(stdout);
        eat_typeahead();
        getcmd(g_buf);
        printf("%c\n",*g_buf);
        filechar = *g_buf;
        /* If error checking is done later, then an error should set
         * filechar to '\0' and continue the while loop.
         */
    }
    *s++ = filechar;
    *s++ = ' ';
    q_done = false;
    while (!q_done) {
        printf("What type of line do you want to add?\n");
        printf("0) Exit.\n");
        printf("1) A scoring rule line.\n");
        printf("   (for the current article's author/subject)\n");
        printf("2) A command, comment, or other kind of line.\n");
        printf("   (use this for any other kind of line)\n");
        printf("\n[Other line formats will be supported later.]\n");
        ch = menu_get_char();
        q_done = true;
        switch (ch) {
          case '0':
            return nullptr;
          case '1':
            break;
          case '2':
            printf("Enter the line below:\n");
            fflush(stdout);
            g_buf[0] = '>';
            g_buf[1] = FINISHCMD;
            if (finish_command(true)) {
                sprintf(s,"%s",g_buf+1);
                return s_sc_e_newline;
            }
            printf("\n");
            q_done = false;
            break;
          case 'h':
            printf("No help available (yet).\n");
            q_done = false;
            break;
          default:
            q_done = false;
            break;
        }
    }
    q_done = false;
    while (!q_done) {
        printf("Enter a score amount (like 10 or -6):");
        fflush(stdout);
        g_buf[0] = ' ';
        g_buf[1] = FINISHCMD;
        if (finish_command(true)) {
            long score = atoi(g_buf + 1);
            if (score == 0)
            {
                if (g_buf[1] != '0')
                {
                    continue;   /* the while loop */
                }
            }
            sprintf(s,"%ld",score);
            s = s_sc_e_newline+strlen(s_sc_e_newline); /* point at terminator  */
            *s++ = ' ';
            q_done = true;
        } else
        {
            printf("\n");
        }
    }
    q_done = false;
    while (!q_done) {
        printf("Do you want to:\n");
        printf("0) Exit.\n");
        printf("1) Give the score to the current subject.\n");
        printf("2) Give the score to the current author.\n");
/* add some more options here later */
/* perhaps fold regular-expression question here? */
        ch = menu_get_char();
        q_done = true;
        switch (ch) {
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
            printf("No help available (yet).\n");
            q_done = false;
            break;
          default:
            q_done = false;
            break;
        }
    }
    /* later ask for headers, pattern-matching, etc... */
    return nullptr;
}

/* returns new string or nullptr to abort. */
const char *sc_easy_command()
{
    printf("\nScoring easy command mode.\n");
    bool q_done = false;
    while (!q_done) {
        printf("0) Exit.\n");
        printf("1) Add something to a scorefile.\n");
        printf("2) Rescore the articles in the current newsgroup.\n");
        printf("3) Explain the current article's score.\n");
        printf("   (show the rules that matched this article)\n");
        printf("4) Edit this newsgroup's scoring rule file.\n");
        /* later add an option to edit an arbitrary file */
        printf("5) Continue scoring unscored articles.\n");
        char ch = menu_get_char();
        q_done = true;
        switch (ch) {
          case '0':
            return nullptr;
          case '1':
            return "\"";        /* do an append command */
          case '2':
            return "r";
          case '3':
            return "s";
          case '4':
            /* add more later */
            return "e";
          case '5':
            return "f";
          case 'h':
            printf("No help available (yet).\n");
            q_done = false;
            break;
          default:
            q_done = false;
            break;
        }
    }
    return nullptr;
}
