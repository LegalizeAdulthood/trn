/* env.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

EXT char *g_home_dir INIT(NULL);	/* login directory */
EXT char *g_dot_dir INIT(NULL);		/* where . files go */
EXT char *g_trn_dir INIT(NULL);		/* usually %./.trn */
EXT char *g_lib INIT(NULL);		/* news library */
EXT char *g_rn_lib INIT(NULL);		/* private news program library */
EXT char *g_tmp_dir INIT(NULL);		/* where tmp files go */
EXT char *g_login_name INIT(NULL);	/* login id of user */
EXT char *g_real_name INIT(NULL);	/* real name of user */
EXT char *g_p_host_name INIT(NULL);	/* host name in a posting */
EXT char *g_local_host INIT(NULL);	/* local host name */

#ifdef SUPPORT_NNTP
EXT int g_net_speed INIT(20);		/* how fast our net-connection is */
#endif

bool env_init(char *tcbuf, bool lax);
bool set_user_name(char *tmpbuf);
bool set_p_host_name(char *tmpbuf);
char *get_val(char *nam, char *def);
char *export_var(const char *nam, const char *val);
void un_export(char *export_val);
void re_export(char *export_val, char *new_val, int limit);
