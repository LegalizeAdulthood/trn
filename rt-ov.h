/* rt-ov.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */

bool ov_init _((void));
int ov_num _((char*,char*));
bool ov_data(ART_NUM first, ART_NUM last, bool cheating);
void ov_close _((void));
char* ov_fieldname _((int));
char* ov_field _((ARTICLE*,int));
