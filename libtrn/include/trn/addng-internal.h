/* trn/addng-internal.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_ADDNG_INTERNAL_H
#define TRN_ADDNG_INTERNAL_H

#include <config/config2.h>
#include <trn/addng.h>

#include <string>
#include <string_view>

// Internal entry points for testing purposes.

AddGroupIndex add_to_list(std::string_view name, int to_read, char_int ch);
void          new_nntp_groups(DataSource *source);
void          new_local_groups(DataSource *source);

#endif
