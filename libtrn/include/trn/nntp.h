/* trn/nntp.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_NNTP_H
#define TRN_NNTP_H

#include <config/typedef.h>

#include <ctime>

struct DataSource;
struct NewsgroupData;

enum FinishBodyMode
{
    FB_BACKGROUND = 0,
    FB_OUTPUT = 1,
    FB_SILENT = 2,
    FB_DISCARD = 3
};

enum
{
    MAX_NNTP_ARTICLES = 10
};

int     nntp_list(const char *type, const char *arg, int len);
void    nntp_finish_list();
int     nntp_group(const char *group, NewsgroupData *gp);
int     nntp_stat(ART_NUM artnum);
ART_NUM nntp_stat_id(char *msgid);
ART_NUM nntp_next_art();
int     nntp_header(ART_NUM artnum);
void    nntp_body(ART_NUM artnum);
long    nntp_artsize();
int     nntp_finishbody(FinishBodyMode bmode);
int     nntp_seekart(ART_POS pos);
ART_POS nntp_tellart();
char       *nntp_readart(char *s, int limit);
std::time_t nntp_time();
int     nntp_newgroups(std::time_t t);
int     nntp_artnums();
#if 0
int nntp_rover();
#endif
ART_NUM nntp_find_real_art(ART_NUM after);
char *nntp_artname(ART_NUM artnum, bool allocate);
char *nntp_tmpname(int ndx);
int nntp_handle_nested_lists();
int nntp_handle_timeout();
void nntp_server_died(DataSource *dp);
#ifdef SUPPORT_XTHREAD
long nntp_readcheck();
long nntp_read(char *buf, long n);
#endif

#endif
