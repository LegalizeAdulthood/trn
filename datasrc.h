/* datasrc.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

struct SRCFILE
{
    FILE*	fp;		/* the file pointer to read the data */
    HASHTABLE*	hp;		/* the hash table for the data */
    LIST*	lp;		/* the list used to store the data */
    long	recent_cnt;	/* # lines/bytes this file might be */
    time_t	lastfetch;	/* when the data was last fetched */
    time_t	refetch_secs;	/* how long before we refetch this file */
};

struct DATASRC
{
    char*	name;		/* our user-friendly name */
    char*	newsid;		/* the active file name or host name */
    SRCFILE	act_sf;		/* the active file's hashed contents */
    char*	grpdesc;	/* the newsgroup description file or tmp */
    SRCFILE	desc_sf;	/* the group description's hashed contents */
    char*	extra_name;	/* local active.times or server's actfile */
    NNTPLINK	nntplink;
    char*	spool_dir;
    char*	over_dir;
    char*	over_fmt;
    char*	thread_dir;
    char*	auth_user;
    char*	auth_pass;
    long	lastnewgrp;	/* time of last newgroup check */
    FILE*	ov_in;		/* the overview's file handle */
    time_t	ov_opened;	/* time overview file was opened */
    Uchar	fieldnum[OV_MAX_FIELDS];
    Uchar	fieldflags[OV_MAX_FIELDS];
    int		flags;
};

enum
{
    DF_TRY_OVERVIEW = 0x0001,
    DF_TRY_THREAD = 0x0002,
    DF_ADD_OK = 0x0004,
    DF_DEFAULT = 0x0008,
    DF_OPEN = 0x0010,
    DF_ACTIVE = 0x0020,
    DF_UNAVAILABLE = 0x0040,
    DF_REMOTE = 0x0080,
    DF_TMPACTFILE = 0x0100,
    DF_TMPGRPDESC = 0x0200,
    DF_USELISTACT = 0x0400,
    DF_XHDR_BROKEN = 0x0800,
    DF_NOXGTITLE = 0x1000,
    DF_NOLISTGROUP = 0x2000,
    DF_NOXREFS = 0x4000
};

enum
{
    FF_HAS_FIELD = 0x01,
    FF_CHECK4FIELD = 0x02,
    FF_HAS_HDR = 0x04,
    FF_CHECK4HDR = 0x08,
    FF_FILTERSEND = 0x10
};

#define DATASRC_NNTP_FLAGS(dp) (((dp) == datasrc? nntplink.flags : (dp)->nntplink.flags))

EXT LIST* datasrc_list;		/* a list of all DATASRCs */
EXT DATASRC* datasrc;		/* the current datasrc */
EXT int datasrc_cnt INIT(0);

#define datasrc_ptr(n)  ((DATASRC*)listnum2listitem(datasrc_list,(long)(n)))
#define datasrc_first() ((DATASRC*)listnum2listitem(datasrc_list,0L))
#define datasrc_next(p) ((DATASRC*)next_listitem(datasrc_list,(char*)(p)))

#define LENGTH_HACK 5	/* Don't bother comparing strings with lengths
			 * that differ by more than this. */
#define MAX_NG 9	/* Maximum number of groups to offer. */

#define DATASRC_ALARM_SECS   (5 * 60)

EXT char* trnaccess_mem INIT(nullptr);

EXT char* nntp_auth_file INIT(nullptr);

void datasrc_init();
char *read_datasrcs(char *filename);
DATASRC *get_datasrc(const char *name);
DATASRC *new_datasrc(const char *name, char **vals);
bool open_datasrc(DATASRC *dp);
void set_datasrc(DATASRC *dp);
void check_datasrcs();
void close_datasrc(DATASRC *dp);
bool actfile_hash(DATASRC *dp);
bool find_actgrp(DATASRC *dp, char *outbuf, const char *nam, int len, ART_NUM high);
char *find_grpdesc(DATASRC *dp, char *groupname);
int srcfile_open(SRCFILE *sfp, const char *filename, const char *fetchcmd, const char *server);
char *srcfile_append(SRCFILE *sfp, char *bp, int keylen);
void srcfile_end_append(SRCFILE *sfp, const char *filename);
void srcfile_close(SRCFILE *sfp);
int find_close_match();
