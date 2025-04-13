/* trn/rt-ov.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_RT_OV_H
#define TRN_RT_OV_H

#include <config/typedef.h>

struct ARTICLE;

/* The usual order for the overview data fields. */
enum ov_field_num
{
    OV_NUM = 0,
    OV_SUBJ,
    OV_FROM,
    OV_DATE,
    OV_MSGID,
    OV_REFS,
    OV_BYTES,
    OV_LINES,
    OV_XREF,
    OV_MAX_FIELDS
};

bool ov_init();
bool ov_data(ART_NUM first, ART_NUM last, bool cheating);
void ov_close();
const char *ov_fieldname(int num);
const char *ov_field(ARTICLE *ap, int num);

#endif
