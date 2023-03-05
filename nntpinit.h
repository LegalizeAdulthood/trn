/* nntpinit.h
 */

/* DON'T EDIT BELOW THIS LINE OR YOUR CHANGES WILL BE LOST! */

int init_nntp(void);
int server_init(const char *machine);
void cleanup_nntp(void);
int get_tcp_socket(const char *machine, int port, const char *service);
