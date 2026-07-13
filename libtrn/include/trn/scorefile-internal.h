// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SCOREFILE_INTERNAL_H
#define TRN_SCOREFILE_INTERNAL_H

#include <trn/scorefile.h>

#include <string_view>

// Internal entry points for testing purposes.

using ScoreFileUrlGetter = bool (*)(std::string_view url, const char *outfile);

void sf_set_url_getter_for_test(ScoreFileUrlGetter getter);
void sf_clear_file_cache_for_test();

#endif
