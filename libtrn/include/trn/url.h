/* trn/url.h
 *
 * Routines for handling WWW URL references.
 */
// This file Copyright 1993 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_URL_H
#define TRN_URL_H

#include <string_view>

bool fetch_http(const char *host, int port, const char *path, const char *outname);
bool fetch_ftp(const char *host, const char *origpath, const char *outname);
bool parse_url(std::string_view url);
bool url_get(const char *url, const char *outfile);

#endif
