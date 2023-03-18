/* env.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

EXT char *g_home_dir INIT(nullptr);	/* login directory */
EXT char *g_dot_dir INIT(nullptr);		/* where . files go */
EXT char *g_trn_dir INIT(nullptr);		/* usually %./.trn */
EXT char *g_lib INIT(nullptr);		/* news library */
EXT char *g_rn_lib INIT(nullptr);		/* private news program library */
EXT char *g_tmp_dir INIT(nullptr);		/* where tmp files go */
EXT char *g_login_name INIT(nullptr);	/* login id of user */
EXT char *g_real_name INIT(nullptr);	/* real name of user */
EXT char *g_p_host_name INIT(nullptr);	/* host name in a posting */
EXT char *g_local_host INIT(nullptr);	/* local host name */

EXT int g_net_speed INIT(20);		/* how fast our net-connection is */

bool env_init(char *tcbuf, bool lax);
bool set_user_name(char *tmpbuf);
bool set_p_host_name(char *tmpbuf);
char *get_val(char *nam, char *def);
char *export_var(const char *nam, const char *val);
void un_export(char *export_val);
void re_export(char *export_val, char *new_val, int limit);
