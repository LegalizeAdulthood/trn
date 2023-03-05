/* env.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */


EXT char* g_home_dir INIT(NULL);	/* login directory */
EXT char* g_dot_dir INIT(NULL);		/* where . files go */
EXT char* g_trn_dir INIT(NULL);		/* usually %./.trn */
EXT char* g_lib INIT(NULL);		/* news library */
EXT char* g_rn_lib INIT(NULL);		/* private news program library */
EXT char* g_tmp_dir INIT(NULL);		/* where tmp files go */
EXT char* g_login_name INIT(NULL);	/* login id of user */
EXT char* g_real_name INIT(NULL);	/* real name of user */
EXT char* g_p_host_name INIT(NULL);	/* host name in a posting */
EXT char* g_local_host INIT(NULL);	/* local host name */

#ifdef SUPPORT_NNTP
EXT int g_net_speed INIT(20);		/* how fast our net-connection is */
#endif

/* DON'T EDIT BELOW THIS LINE OR YOUR CHANGES WILL BE LOST! */

bool env_init _((char*,bool_int));
bool setusername _((char*));
bool setphostname _((char*));
char* getval _((char*,char*));
char* export(const char*,const char*);
void un_export _((char*));
void re_export _((char*,char*,int));
