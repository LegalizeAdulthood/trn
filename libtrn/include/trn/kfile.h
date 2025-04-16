/* trn/kfile.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_KFILE_H
#define TRN_KFILE_H

#include <config/typedef.h>

#include "trn/enum-flags.h"

#include <cstdint>
#include <cstdio>

struct Article;

enum : bool
{
    KF_GLOBAL = false,
    KF_LOCAL = true
};

enum KillFileStateFlags : std::uint16_t
{
    KFS_NONE = 0x0000,
    KFS_LOCAL_CHANGES = 0x0001,
    KFS_THREAD_CHANGES = 0x0002,
    KFS_NORMAL_LINES = 0x0010,
    KFS_THREAD_LINES = 0x0020,
    KFS_GLOBAL_THREADFILE = 0x1000
};
DECLARE_FLAGS_ENUM(KillFileStateFlags, std::uint16_t);

enum autokill_flags : std::uint16_t
{
    AUTO_KILL_NONE = 0x0,
    AUTO_KILL_THD = 0x8000,
    AUTO_KILL_SBJ = 0x4000,
    AUTO_KILL_FOL = 0x2000,
    AUTO_KILL_1 = 0x1000,
    AUTO_KILL_MASK = 0xF000,
    AUTO_SEL_THD = 0x0800,
    AUTO_SEL_SBJ = 0x0400,
    AUTO_SEL_FOL = 0x0200,
    AUTO_SEL_1 = 0x0100,
    AUTO_SEL_MASK = 0x0F00,
    AUTO_OLD = 0x0080,
    /* The following defines are only valid as flags to function calls, used
     * in combination with the AUTO_* flags above. */
    AFFECT_UNSEL = 0,
    AFFECT_ALL = 0x0001,
    ALSO_ECHO = 0x0002,   /* only works with [un]select_article() */
    SET_TORETURN = 0x0004 /* only works with kill_*() */
};
DECLARE_FLAGS_ENUM(autokill_flags, std::uint16_t);

enum
{
    KF_AGE_MASK = 0x003F
};

enum
{
    KF_MAXDAYS = 30
};

extern std::FILE          *g_localkfp;               /* local (for this newsgroup) file */
extern KillFileStateFlags g_kf_state;               /* the state of our kill files */
extern KillFileStateFlags g_kfs_thread_change_set;  /* bits to set for thread changes */
extern int                 g_kf_changethd_cnt;       /* # entries changed from old to new */
extern ART_NUM             g_killfirst;              /* used as g_firstart when killing */

void kfile_init();
int do_kfile(std::FILE *kfp, int entering);
void kill_unwanted(ART_NUM starting, const char *message, int entering);
void rewrite_kfile(ART_NUM thru);
void update_thread_kfile();
void change_auto_flags(Article *ap, autokill_flags auto_flag);
void clear_auto_flags(Article *ap);
void perform_auto_flags(Article *ap, autokill_flags thread_autofl, autokill_flags subj_autofl, autokill_flags chain_autofl);
void edit_kfile();
void open_kfile(int local);
void kf_append(const char *cmd, bool local);

#endif
