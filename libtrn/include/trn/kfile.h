/* trn/kfile.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_KFILE_H
#define TRN_KFILE_H

#include <config/typedef.h>
#include <trn/enum-flags.h>

#include <cstdint>
#include <cstdio>
#include <string_view>

struct Article;
struct HashDatum;

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
    KFS_GLOBAL_THREAD_FILE = 0x1000
};
DECLARE_FLAGS_ENUM(KillFileStateFlags, std::uint16_t);

enum AutoKillFlags : std::uint16_t
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
    // The following defines are only valid as flags to function calls, used
    // in combination with the AUTO_* flags above.
    AFFECT_UNSEL = 0,
    AFFECT_ALL = 0x0001,
    ALSO_ECHO = 0x0002,    // only works with [un]select_article()
    SET_TO_RETURN = 0x0004 // only works with kill_*()
};
DECLARE_FLAGS_ENUM(AutoKillFlags, std::uint16_t);

enum
{
    KF_AGE_MASK = 0x003F
};

enum
{
    KF_MAX_DAYS = 30
};

struct KillFileProcessor
{
    void open(int local);
    void close(int local);
    void kill_unwanted(ArticleNum starting, std::string_view message, int entering);

    // TODO: private
    int do_file(std::FILE *kfp, int entering);

    std::FILE *local_kfp{};  // local (for this newsgroup) file
    std::FILE *global_kfp{}; // global article killer file
    ArticleNum first{};      // used as g_first_art when killing
    bool       exit_cmds{};
    bool       mentioned{};
};

extern KillFileProcessor g_kill;

struct KillFilePersistence
{
    void init();
    void rewrite_kill_file(ArticleNum thru);

    // TODO: private
    int write_local_thread_commands(int keylen, HashDatum *data, int extra);

    KillFileStateFlags state{};              // the state of our kill files
    KillFileStateFlags thread_change_set{};  // bits to set for thread changes
    KillFileStateFlags local_change_clear{}; // bits to clear local changes
    int                change_thread_cnt{};  // # entries changed from old to new
    int                thread_cnt{};         // # entries in the thread kfile
    long               day_num{};            // day number for thread killfile
    std::FILE         *new_fp{};
};

extern KillFilePersistence g_kill_store;

void update_thread_kill_file();
void edit_kill_file();
void kill_file_append(std::string_view cmd, bool local);

#endif
