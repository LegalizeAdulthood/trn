/* trn/intrp.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_INTRP_H
#define TRN_INTRP_H

#include <cstddef>
#include <string>
#include <string_view>

extern std::string g_orig_dir;  // cwd when rn invoked
extern std::string g_host_name; // host name to match local postings
extern std::string g_head_name;
extern int         g_perform_count;

#ifdef HAS_NEWS_ADMIN
extern const std::string g_news_admin; // news administrator
extern int g_news_uid;
#endif

void  interp_init();
void  interp_final();
std::string do_interp(std::string_view pattern);
std::string do_interp(std::string_view &pattern, std::string_view stoppers, std::string_view cmd);
std::string interp_search(std::string_view pattern, std::string_view cmd);
std::size_t skip_interp(std::string_view pattern, std::string_view stoppers);
char        interp_backslash(std::string_view &pattern);
std::string normalize_refs(std::string_view refs);

#endif
