/* sw.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_SW_H
#define TRN_SW_H

void sw_file(char **tcbufptr);
void sw_list(char *swlist);
void decode_switch(char *s);
void save_init_environment(char *str);
void write_init_environment(FILE *fp);

#endif
