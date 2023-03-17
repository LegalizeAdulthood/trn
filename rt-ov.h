/* rt-ov.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */

bool ov_init();
int ov_num(char *, char *);
bool ov_data(ART_NUM first, ART_NUM last, bool cheating);
void ov_close();
char *ov_fieldname(int);
char *ov_field(ARTICLE *ap, int num);
