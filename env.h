/* env.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_ENV_H
#define TRN_ENV_H

#include <string>

extern char       *g_home_dir;    /* login directory */
extern std::string g_dot_dir;     /* where . files go */
extern std::string g_trn_dir;     /* usually %./.trn */
extern std::string g_lib;         /* news library */
extern std::string g_rn_lib;      /* private news program library */
extern const char *g_tmp_dir;     /* where tmp files go */
extern char       *g_login_name;  /* login id of user */
extern char       *g_real_name;   /* real name of user */
extern char       *g_p_host_name; /* host name in a posting */
extern char       *g_local_host;  /* local host name */
extern int         g_net_speed;   /* how fast our net-connection is */

bool  env_init(char *tcbuf, bool lax);
void  env_final();
char *get_val(const char *nam, char *def);
char *export_var(const char *nam, const char *val);
void  un_export(char *export_val);
void  re_export(char *export_val, const char *new_val, int limit);

#endif
