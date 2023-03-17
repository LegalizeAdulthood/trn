/* This file Copyright 1992 by Clifford A. Adams */
/* sacmd.h
 *
 * main command loop
 */

#define SA_KILL 1
#define SA_MARK 2
#define SA_SELECT 3
#define SA_KILL_UNMARKED 4
#define SA_KILL_MARKED 5
#define SA_EXTRACT 6

int sa_docmd(void);
bool sa_extract_start(void);
void sa_art_cmd_prim(int, long);
int sa_art_cmd(int, int, long);
long sa_wrap_next_author(long);
