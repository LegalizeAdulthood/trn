/* trn/url.h
 *
 * Routines for handling WWW URL references.
 */
// This file Copyright 1993 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_URL_H
#define TRN_URL_H

#include <filesystem>
#include <string_view>

bool url_get(std::string_view url, const std::filesystem::path &outfile);

#endif
