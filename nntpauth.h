/* nntpauth.h
*/ 
/* This software is copyrighted as detailed in the LICENSE file. */

#ifdef SUPPORT_NNTP
int nntp_handle_auth_err();
#endif
#ifdef USE_GENAUTH
int nntp_auth(char *);
#endif
