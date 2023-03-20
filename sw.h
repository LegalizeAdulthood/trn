/* sw.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

EXT char** init_environment_strings INIT(nullptr);
EXT int init_environment_cnt INIT(0);
EXT int init_environment_max INIT(0);

void sw_file(char **tcbufptr);
void sw_list(char *swlist);
void decode_switch(char *s);
void save_init_environment(char *str);
void write_init_environment(FILE *fp);
