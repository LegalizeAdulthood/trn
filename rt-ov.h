/* rt-ov.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef RT_OV_H
#define RT_OV_H

bool ov_init();
int ov_num(char *hdr, char *end);
bool ov_data(ART_NUM first, ART_NUM last, bool cheating);
void ov_close();
char *ov_fieldname(int num);
char *ov_field(ARTICLE *ap, int num);

#endif
