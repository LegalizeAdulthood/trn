/* nntpauth.h
*/ 
/* This software is copyrighted as detailed in the LICENSE file. */


/* DON'T EDIT BELOW THIS LINE OR YOUR CHANGES WILL BE LOST! */

EXTERN_C_BEGIN

#ifdef SUPPORT_NNTP
int nntp_handle_auth_err(void);
#endif
#ifdef USE_GENAUTH
int nntp_auth(char *);
#endif

EXTERN_C_END
