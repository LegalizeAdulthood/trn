/* This file Copyright 1993 by Clifford A. Adams */
/* url.c
 *
 * Routines for handling WWW URL references.
 */

#include "EXTERN.h"
#include "common.h"
#ifdef USEURL
#include "term.h"
#include "util.h"
#include "util2.h"
#include "INTERN.h"
#include "url.h"
#include "url.ih"

/* Lower-level net routines grabbed from nntpinit.c.
 * The special cases (DECNET, EXCELAN, and NONETD) are not supported.
 */

/* NOTE: If running Winsock, NNTP must be enabled so that the Winsock
 *       initialization will be done.  (common.h will check for this)
 */
#ifdef WINSOCK
#include <winsock.h>
WSADATA wsaData;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#ifndef WINSOCK
unsigned long inet_addr _((char*));
struct servent* getservbyname();
struct hostent* gethostbyname();
#endif

static char url_buf[1030];
/* XXX just a little bit larger than necessary... */
static char url_type[256];
static char url_host[256];
static int  url_port;
static char url_path[1024];

static int
get_url_socket(machine,port)
char* machine;
int port;
{
    int s;
    struct sockaddr_in sin;
#ifdef __hpux
    int socksize = 0;
    int socksizelen = sizeof socksize;
#endif
    struct servent* sp;
    struct hostent* hp;
#ifdef h_addr
    int x = 0;
    register char** cp;
    static char* alist[1];
#endif /* h_addr */
    static struct hostent def;
    static struct in_addr defaddr;
    static char namebuf[256];

    if (port) {
	if ((sp = getservbyport(htons(port),"tcp")) == NULL) {
	    fprintf(stderr, "port %d/tcp: Unknown service.\n", port);
	    return -1;
	}
    }
    else {
	if ((sp = getservbyname("www", "tcp")) == NULL) {
	    fprintf(stderr, "www/tcp: Unknown service.\n");
	    return -1;
	}
    }
    /* If not a raw ip address, try nameserver */
    if (!isdigit(*machine)
#ifdef INADDR_NONE
     || (defaddr.s_addr = inet_addr(machine)) == INADDR_NONE)
#else
     || (long)(defaddr.s_addr = inet_addr(machine)) == -1)
#endif
	hp = gethostbyname(machine);
    else {
	/* Raw ip address, fake  */
	(void) strcpy(namebuf, machine);
	def.h_name = namebuf;
#ifdef h_addr
	def.h_addr_list = alist;
#endif
	def.h_addr = (char*)&defaddr;
	def.h_length = sizeof(struct in_addr);
	def.h_addrtype = AF_INET;
	def.h_aliases = 0;
	hp = &def;
    }
    if (hp == NULL) {
	fprintf(stderr, "%s: Unknown host.\n", machine);
	return -1;
    }

    bzero((char*)&sin, sizeof sin);
    sin.sin_family = hp->h_addrtype;
    sin.sin_port = sp->s_port;

    /* The following is kinda gross.  The name server under 4.3
    ** returns a list of addresses, each of which should be tried
    ** in turn if the previous one fails.  However, 4.2 hostent
    ** structure doesn't have this list of addresses.
    ** Under 4.3, h_addr is a #define to h_addr_list[0].
    ** We use this to figure out whether to include the NS specific
    ** code... */
#ifdef h_addr
    /* get a socket and initiate connection -- use multiple addresses */
    for (cp = hp->h_addr_list; cp && *cp; cp++) {
	extern char* inet_ntoa _((const struct in_addr));
	s = socket(hp->h_addrtype, SOCK_STREAM, 0);
	if (s < 0) {
	    perror("socket");
	    return -1;
	}
        bcopy(*cp, (char*)&sin.sin_addr, hp->h_length);
		
	if (x < 0)
	    fprintf(stderr, "trying %s\n", inet_ntoa(sin.sin_addr));
	x = connect(s, (struct sockaddr*)&sin, sizeof (sin));
	if (x == 0)
	    break;
        fprintf(stderr, "connection to %s: ", inet_ntoa(sin.sin_addr));
	perror("");
	(void) close(s);
    }
    if (x < 0) {
	fprintf(stderr, "giving up...\n");
	return -1;
    }
#else /* no name server */
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	perror("socket");
	return -1;
    }

    /* And then connect */

    bcopy(hp->h_addr, (char*)&sin.sin_addr, hp->h_length);
    if (connect(s, (struct sockaddr*)&sin, sizeof sin) < 0) {
	perror("connect");
	(void) close(s);
	return -1;
    }
#endif /* !h_addr */
#ifdef __hpux	/* recommended by raj@cup.hp.com */
#define	HPSOCKSIZE 0x8000
    getsockopt(s, SOL_SOCKET, SO_SNDBUF, (caddr_t)&socksize, (caddr_t)&socksizelen);
    if (socksize < HPSOCKSIZE) {
	socksize = HPSOCKSIZE;
	setsockopt(s, SOL_SOCKET, SO_SNDBUF, (caddr_t)&socksize, sizeof(socksize));
    }
    socksize = 0;
    socksizelen = sizeof(socksize);
    getsockopt(s, SOL_SOCKET, SO_RCVBUF, (caddr_t)&socksize, (caddr_t)&socksizelen);
    if (socksize < HPSOCKSIZE) {
	socksize = HPSOCKSIZE;
	setsockopt(s, SOL_SOCKET, SO_RCVBUF, (caddr_t)&socksize, sizeof(socksize));
    }
#endif
    return s;
}

/* returns TRUE if successful */
bool
fetch_http(host,port,path,outname)
char* host;
int port;
char* path;
char* outname;
{
    int sock;
    FILE* fp_out;
    int len;

    sock = get_url_socket(host,port);

    /* XXX length check */
    /* XXX later consider using HTTP/1.0 format (and user-agent) */
    sprintf(url_buf, "GET %s\n",path);
    /* Should I be writing the 0 char at the end? */
    if (write(sock, url_buf, strlen(url_buf)+1) < 0) {
	printf("\nError: writing on URL socket\n");
	close(sock);
	return FALSE;
    }

    fp_out = fopen(outname,"w");
    if (!fp_out) {
	printf("\nURL output file could not be opened.\n");
	return FALSE;
    }
    /* XXX some kind of URL timeout would be really nice */
    /* (the old nicebg code caused portability problems) */
    /* later consider larger buffers, spinner */
    while (1) {
	if ((len = read(sock, url_buf, 1024)) < 0) {
	    printf("\nError: reading URL reply\n");
	    return FALSE;
	}
	if (len == 0) {
	    break;	/* no data, end connection */
	}
	fwrite(url_buf,1,len,fp_out);
    }
    fclose(fp_out);
    close(sock);
    return TRUE;
}

/* add port support later? */
bool
fetch_ftp(host,origpath,outname)
char* host;
char* origpath;
char* outname;
{
#ifdef USEFTP
    static char cmdline[1024];
    static char path[512];	/* use to make writable copy */
    /* buffers used because because filexp overwrites previous call results */
    static char username[128];
    static char userhost[128];
    char* p;
    int status;
    char* cdpath;
    int x,y,l;

    safecpy(path,origpath,510);
    p = rindex(path, '/');	/* p points to last slash or NULL*/
    if (p == NULL) {
	printf("Error: URL:ftp path has no '/' character.\n") FLUSH;
	return FALSE;
    }
    if (p[1] == '\0') {
	printf("Error: URL:ftp path has no final filename.\n") FLUSH;
	return FALSE;
    }
    safecpy(username,filexp("%L"),120);
    safecpy(userhost,filexp("%H"),120);
    if (p != path) {	/* not of form /foo */
	*p = '\0';
	cdpath = path;
    } else
	cdpath = "/";

    sprintf(cmdline,"%s/ftpgrab %s ftp %s@%s %s %s %s",
	    filexp("%X"),host,username,userhost,cdpath,p+1,outname);

    /* modified escape_shell_cmd code from NCSA HTTPD util.c */
    /* serious security holes could result without this code */
    l = strlen(cmdline);
    for (x = 0; cmdline[x]; x++) {
	if (index("&;`'\"|*?~<>^()[]{}$\\",cmdline[x])) {
	    for (y = l+1; y > x; y--)
		cmdline[y] = cmdline[y-1];
	    l++; /* length has been increased */
	    cmdline[x] = '\\';
	    x++; /* skip the character */
	}
    }

#if 0
    printf("ftpgrab command:\n|%s|\n",cmdline);
#endif

    *p = '/';
    status = doshell(NULL,cmdline);
#if 0
    printf("\nFTP command status is %d\n",status) FLUSH;
    while (!input_pending()) ;
    eat_typeahead();
#endif
    return TRUE;
#else
    printf("\nThis copy of trn does not have URL:ftp support.\n");
    return FALSE;
#endif
}

/* right now only full, absolute URLs are allowed. */
/* use relative URLs later? */
/* later: pay more attention to long URLs */
bool
parse_url(url)
char* url;
{
    char* s;
    char* p;

    /* consider using 0 as default to look up the service? */
    url_port = 80;	/* the default */
    if (!url || !*url) {
	printf("Empty URL -- ignoring.\n") FLUSH;
	return FALSE;
    }
    p = url_type;
    for (s = url; *s && *s != ':'; *p++ = *s++) ;
    *p = '\0';
    if (!*s) {
	printf("Incomplete URL: %s\n",url) FLUSH;
	return FALSE;
    }
    s++;
    if (strnEQ(s,"//",2)) {
	/* normal URL type, will have host (optional portnum) */
	s += 2;
	p = url_host;
	while (*s && *s != '/' && *s != ':') *p++ = *s++;
	*p = '\0';
	if (!*s) {
	    printf("Incomplete URL: %s\n",url) FLUSH;
	    return FALSE;
	}
	if (*s == ':') {
	    s++;
	    p = url_buf;	/* temp space */
	    if (!isdigit(*s)) {
		printf("Bad URL (non-numeric portnum): %s\n",url) FLUSH;
		return FALSE;
	    }
	    while (isdigit(*s)) *p++ = *s++;
	    *p = '\0';
	    url_port = atoi(url_buf);
	}
    } else {
	if (!strEQ(url_type,"news")) {
	    printf("URL needs a hostname: %s\n",url);
	    return FALSE;
	}
    }
    /* finally, just do the path */
    if (*s != '/') {
	printf("Bad URL (path does not start with /): %s\n",url) FLUSH;
	return FALSE;
    }
    strcpy(url_path,s);
    return TRUE;
}

bool
url_get(url,outfile)
char* url;
char* outfile;
{
    bool flag;
    
    if (!parse_url(url))
	return FALSE;

    if (strEQ(url_type,"http"))
	flag = fetch_http(url_host,url_port,url_path,outfile);
    else if (strEQ(url_type,"ftp"))
	flag = fetch_ftp(url_host,url_path,outfile);
    else {
	if (url_type)
	    printf("\nURL type %s not supported (yet?)\n",url_type) FLUSH;
	flag = FALSE;
    }
    return flag;
}
#endif /* USEURL */
