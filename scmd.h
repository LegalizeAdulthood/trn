/* This file is Copyright 1993 by Clifford A. Adams */
/* scmd.h
 *
 * Scan command interpreter/router
 */
#ifndef TRN_SCMD_H
#define TRN_SCMD_H

void s_go_bot();
int s_finish_cmd(const char *string);
int s_cmdloop();
void s_lookahead();
int s_docmd();
bool s_match_description(long ent);
long s_forward_search(long ent);
long s_backward_search(long ent);
void s_search();
void s_jumpnum(char_int firstchar);

#endif
