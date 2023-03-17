/* nntpinit.h
 */

int init_nntp();
int server_init(const char *machine);
void cleanup_nntp();
int get_tcp_socket(const char *machine, int port, const char *service);
