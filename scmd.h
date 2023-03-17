/* This file is Copyright 1993 by Clifford A. Adams */
/* scmd.h
 *
 * Scan command interpreter/router
 */

void s_go_bot();
int s_finish_cmd(char *);
int s_cmdloop();
void s_lookahead();
int s_docmd();
bool s_match_description(long);
long s_forward_search(long);
long s_backward_search(long);
void s_search();
void s_jumpnum(char_int);
