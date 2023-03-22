/* This file Copyright 1993 by Clifford A. Adams */
/* url.c
 *
 * Routines for handling WWW URL references.
 */

#include "common.h"
#include "nntpinit.h"
#include "util.h"
#include "util2.h"
#include "url.h"

/* Lower-level net routines grabbed from nntpinit.c.
 */

/* NOTE: If running Winsock, NNTP must be enabled so that the Winsock
 *       initialization will be done.  (common.h will check for this)
 */
#ifdef WINSOCK
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#ifdef MSDOS
#include <io.h>
#endif

#ifndef WINSOCK
unsigned long inet_addr (char*);
struct servent* getservbyname();
struct hostent* gethostbyname();
#endif

static char s_url_buf[1030];
/* XXX just a little bit larger than necessary... */
static char s_url_type[256];
static char s_url_host[256];
static int  s_url_port;
static char s_url_path[1024];

/* returns true if successful */
bool fetch_http(const char *host, int port, const char *path, const char *outname)
{
    int sock;
    FILE* fp_out;
    int len;

    sock = get_tcp_socket(host,port,"http");

    /* XXX length check */
    /* XXX later consider using HTTP/1.0 format (and user-agent) */
    sprintf(s_url_buf, "GET %s\n",path);
    /* Should I be writing the 0 char at the end? */
    if (write(sock, s_url_buf, strlen(s_url_buf)+1) < 0) {
	printf("\nError: writing on URL socket\n");
	close(sock);
	return false;
    }

    fp_out = fopen(outname,"w");
    if (!fp_out) {
	printf("\nURL output file could not be opened.\n");
	return false;
    }
    /* XXX some kind of URL timeout would be really nice */
    /* (the old nicebg code caused portability problems) */
    /* later consider larger buffers, spinner */
    while (true) {
	if ((len = read(sock, s_url_buf, 1024)) < 0) {
	    printf("\nError: reading URL reply\n");
	    return false;
	}
	if (len == 0) {
	    break;	/* no data, end connection */
	}
	fwrite(s_url_buf,1,len,fp_out);
    }
    fclose(fp_out);
    close(sock);
    return true;
}

/* add port support later? */
bool fetch_ftp(const char *host, const char *origpath, const char *outname)
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
    p = strrchr(path, '/');	/* p points to last slash or nullptr*/
    if (p == nullptr) {
	printf("Error: URL:ftp path has no '/' character.\n") FLUSH;
	return false;
    }
    if (p[1] == '\0') {
	printf("Error: URL:ftp path has no final filename.\n") FLUSH;
	return false;
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
	if (strchr("&;`'\"|*?~<>^()[]{}$\\",cmdline[x])) {
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
    status = doshell(nullptr,cmdline);
#if 0
    printf("\nFTP command status is %d\n",status) FLUSH;
    while (!input_pending()) ;
    eat_typeahead();
#endif
    return true;
#else
    printf("\nThis copy of trn does not have URL:ftp support.\n");
    return false;
#endif
}

/* right now only full, absolute URLs are allowed. */
/* use relative URLs later? */
/* later: pay more attention to long URLs */
bool parse_url(const char *url)
{
    const char* s;
    char* p;

    /* consider using 0 as default to look up the service? */
    s_url_port = 80;	/* the default */
    if (!url || !*url) {
	printf("Empty URL -- ignoring.\n") FLUSH;
	return false;
    }
    p = s_url_type;
    for (s = url; *s && *s != ':'; *p++ = *s++) ;
    *p = '\0';
    if (!*s) {
	printf("Incomplete URL: %s\n",url) FLUSH;
	return false;
    }
    s++;
    if (!strncmp(s,"//",2)) {
	/* normal URL type, will have host (optional portnum) */
	s += 2;
	p = s_url_host;
	/* check for address literal: news://[ip:v6:address]:port/ */
	if (*s == '[') {
	    while (*s && *s != ']')
		*p++ = *s++;
	    if (!*s) {
		printf("Bad address literal: %s\n",url) FLUSH;
		return false;
	    }
	    s++;	/* skip ] */
	} else
	    while (*s && *s != '/' && *s != ':') *p++ = *s++;
	*p = '\0';
	if (!*s) {
	    printf("Incomplete URL: %s\n",url) FLUSH;
	    return false;
	}
	if (*s == ':') {
	    s++;
	    p = s_url_buf;	/* temp space */
	    if (!isdigit(*s)) {
		printf("Bad URL (non-numeric portnum): %s\n",url) FLUSH;
		return false;
	    }
	    while (isdigit(*s)) *p++ = *s++;
	    *p = '\0';
	    s_url_port = atoi(s_url_buf);
	}
    } else {
	if (!!strcmp(s_url_type,"news")) {
	    printf("URL needs a hostname: %s\n",url);
	    return false;
	}
    }
    /* finally, just do the path */
    if (*s != '/') {
	printf("Bad URL (path does not start with /): %s\n",url) FLUSH;
	return false;
    }
    strcpy(s_url_path,s);
    return true;
}

bool url_get(const char *url, const char *outfile)
{
    bool flag;
    
    if (!parse_url(url))
	return false;

    if (!strcmp(s_url_type,"http"))
	flag = fetch_http(s_url_host,s_url_port,s_url_path,outfile);
    else if (!strcmp(s_url_type,"ftp"))
	flag = fetch_ftp(s_url_host,s_url_path,outfile);
    else {
	if (s_url_type)
	    printf("\nURL type %s not supported (yet?)\n",s_url_type) FLUSH;
	flag = false;
    }
    return flag;
}
