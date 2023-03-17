/* nntp.h
*/ 
/* This software is copyrighted as detailed in the LICENSE file. */

#ifdef SUPPORT_NNTP

#define FB_BACKGROUND	0
#define FB_OUTPUT	1
#define FB_SILENT	2
#define FB_DISCARD	3

#define MAX_NNTP_ARTICLES   10

#endif

#ifdef SUPPORT_NNTP
int nntp_list(char *, char *, int);
#endif
void nntp_finish_list(void);
int nntp_group(char *, NGDATA *);
int nntp_stat(ART_NUM);
ART_NUM nntp_stat_id(char *);
ART_NUM nntp_next_art(void);
int nntp_header(ART_NUM);
void nntp_body(ART_NUM);
long nntp_artsize(void);
int nntp_finishbody(int);
int nntp_seekart(ART_POS);
ART_POS nntp_tellart(void);
char *nntp_readart(char *, int);
time_t nntp_time(void);
int nntp_newgroups(time_t);
int nntp_artnums(void);
#if 0
int nntp_rover(void);
#endif
ART_NUM nntp_find_real_art(ART_NUM);
char *nntp_artname(ART_NUM artnum, bool allocate);
char *nntp_tmpname(int);
int nntp_handle_nested_lists(void);
int nntp_handle_timeout(void);
void nntp_server_died(DATASRC *);
#ifdef SUPPORT_XTHREAD
long nntp_readcheck(void);
long nntp_read(char *, long);
#endif
