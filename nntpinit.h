/* nntpinit.h
 */

/* DON'T EDIT BELOW THIS LINE OR YOUR CHANGES WILL BE LOST! */

int init_nntp _((void));
int server_init _((char*));
void cleanup_nntp _((void));
#ifdef NONETDB
unsigned long inet_addr _((char*));
#endif
