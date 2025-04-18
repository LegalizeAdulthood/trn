/* trn/rt-ov.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_RT_OV_H
#define TRN_RT_OV_H

#include <config/typedef.h>

struct Article;

// The usual order for the overview data fields.
enum OverviewFieldNum
{
    OV_NUM = 0,
    OV_SUBJ,
    OV_FROM,
    OV_DATE,
    OV_MSG_ID,
    OV_REFS,
    OV_BYTES,
    OV_LINES,
    OV_XREF,
    OV_MAX_FIELDS
};

bool        ov_init();
bool        ov_data(ArticleNum first, ArticleNum last, bool cheating);
void        ov_close();
const char *ov_field_name(int num);
const char *ov_field(Article *ap, int num);

#endif
