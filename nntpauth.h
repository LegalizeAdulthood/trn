/* nntpauth.h
*/ 
/* This software is copyrighted as detailed in the LICENSE file. */

int nntp_handle_auth_err();
#ifdef USE_GENAUTH
int nntp_auth(char *);
#endif
