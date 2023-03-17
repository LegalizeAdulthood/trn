/* This file Copyright 1993 by Clifford A. Adams */
/* url.h
 *
 * Routines for handling WWW URL references.
 */

bool fetch_http(char *, int, char *, char *);
bool fetch_ftp(char *, char *, char *);
bool parse_url(char *);
bool url_get(char *, char *);
