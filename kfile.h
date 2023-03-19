/* kfile.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#define KF_GLOBAL false
#define KF_LOCAL true

enum
{
    KFS_LOCAL_CHANGES = 0x0001,
    KFS_THREAD_CHANGES = 0x0002,
    KFS_NORMAL_LINES = 0x0010,
    KFS_THREAD_LINES = 0x0020,
    KFS_GLOBAL_THREADFILE = 0x1000
};

enum
{
    AUTO_KILL_THD = 0x8000,
    AUTO_KILL_SBJ = 0x4000,
    AUTO_KILL_FOL = 0x2000,
    AUTO_KILL_1 = 0x1000,
    AUTO_SEL_THD = 0x0800,
    AUTO_SEL_SBJ = 0x0400,
    AUTO_SEL_FOL = 0x0200,
    AUTO_SEL_1 = 0x0100,
    AUTO_OLD = 0x0080,
    AUTO_KILLS = 0xF000,
    AUTO_SELS = 0x0F00
};

/* The following defines are only valid as flags to function calls, used
 * in combination with the AUTO_* flags above. */
enum
{
    AFFECT_UNSEL = 0,
    AFFECT_ALL = 0x0001,
    ALSO_ECHO = 0x0002,   /* only works with [un]select_article() */
    SET_TORETURN = 0x0004 /* only works with kill_*() */
};

enum
{
    KF_AGE_MASK = 0x003F
};
#define KF_DAYNUM(x)	((long)time((time_t*)nullptr) / 86400 - 10490 - (x))

enum
{
    KF_MAXDAYS = 30
};

EXT FILE* globkfp INIT(nullptr);		/* global article killer file */
EXT FILE* localkfp INIT(nullptr);		/* local (for this newsgroup) file */
EXT int kf_state;			/* the state of our kill files */
EXT int kfs_local_change_clear;		/* bits to clear local changes */
EXT int kfs_thread_change_set;		/* bits to set for thread changes */
EXT int kf_thread_cnt;			/* # entries in the thread kfile */
EXT int kf_changethd_cnt;		/* # entries changed from old to new */
EXT long kf_daynum;			/* day number for thread killfile */
EXT ART_NUM killfirst;			/* used as firstart when killing */

void kfile_init();
int do_kfile(FILE *, int);
void kill_unwanted(ART_NUM, char *, int);
void rewrite_kfile(ART_NUM);
void update_thread_kfile();
void change_auto_flags(ARTICLE *ap, int auto_flag);
void clear_auto_flags(ARTICLE *ap);
void perform_auto_flags(ARTICLE *ap, int thread_autofl, int subj_autofl, int chain_autofl);
int edit_kfile();
void open_kfile(int);
void kf_append(char *cmd, bool local);
