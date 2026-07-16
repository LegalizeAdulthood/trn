/* common.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <config/common.h>

// GLOBAL THINGS

// various things of type char

char g_msg[CMD_BUF_LEN];     // general purpose message buffer
char g_buf[LINE_BUF_LEN + 1]; // general purpose line buffer
char g_cmd_buf[CMD_BUF_LEN]; // buffer for formatting system commands

// Factored strings
const char *g_h_for_help{"Type h for help.\n"};
const char *g_unsub_to{"Unsubscribed to newsgroup %s\n"};
const char *g_cant_open{"Can't open %s\n"};
const char *g_cant_create{"Can't create %s\n"};
const char *g_no_cd{"Can't chdir to directory %s\n"};
