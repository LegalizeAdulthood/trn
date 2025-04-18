// This file is Copyright 1993 by Clifford A. Adams
/* trn/scmd.h
 *
 * Scan command interpreter/router
 */
#ifndef TRN_SCMD_H
#define TRN_SCMD_H

#include <config/config2.h>

void s_go_bot();
int s_finish_cmd(const char *string);
int s_cmd_loop();
void s_look_ahead();
int s_do_cmd();
bool s_match_description(long ent);
long s_forward_search(long ent);
long s_backward_search(long ent);
void s_search();
void s_jump_num(char_int firstchar);

#endif
