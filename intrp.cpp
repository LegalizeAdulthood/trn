/* intrp.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "common.h"
#include "intrp.h"

#include "artio.h"
#include "artsrch.h"
#include "bits.h"
#include "cache.h"
#include "datasrc.h"
#include "env.h"
#include "final.h"
#include "head.h"
#include "init.h"
#include "list.h"
#include "ng.h"
#include "ngdata.h"
#include "nntp.h"
#include "respond.h"
#include "rt-select.h"
#include "rt-util.h"
#include "search.h"
#include "term.h"
#include "trn.h"
#include "util.h"
#include "util2.h"

#ifdef HAS_UNAME
#include <sys/utsname.h>
struct utsname utsn;
#endif
#ifdef MSDOS
#include <stdio.h>
#define popen(s,m) _popen(s,m)
#define pclose(f) _pclose(f)
#endif

std::string g_origdir;    /* cwd when rn invoked */
char       *g_hostname{}; /* host name to match local postings */
std::string g_headname;
int         g_perform_cnt{};

#ifdef HAS_NEWS_ADMIN
const std::string g_newsadmin{NEWS_ADMIN}; /* news administrator */
int g_newsuid{};
#endif

static char *skipinterp(char *pattern, const char *stoppers);
static void abort_interp();

static char  *s_regexp_specials = "^$.*[\\/?%";
static char   s_orgname[] = ORGNAME;
static COMPEX s_cond_compex;
static char   s_empty[]{""};

void intrp_init(char *tcbuf, int tcbuf_len)
{
#if HOSTBITS != 0
    int i;
#endif

    init_compex(&s_cond_compex);
    
    /* get environmental stuff */

#ifdef HAS_NEWS_ADMIN
    {
#ifdef HAS_GETPWENT
	struct passwd* pwd = getpwnam(NEWS_ADMIN);

	if (pwd != nullptr)
	    g_newsuid = pwd->pw_uid;
#else
#ifdef TILDENAME
	char tildenews[2+sizeof NEWS_ADMIN];
	strcpy(tildenews, "~");
	strcat(tildenews, NEWS_ADMIN);
	(void) filexp(tildenews);
#else
	... "Define either HAS_GETPWENT or TILDENAME to get NEWS_ADMIN"
#endif  /* TILDENAME */
#endif	/* HAS_GETPWENT */
    }

    /* if this is the news admin then load his UID into g_newsuid */

    if (!strcmp(g_login_name,""))
	g_newsuid = getuid();
#endif

    if (g_checkflag)             /* that getwd below takes ~1/3 sec. */
        return;                  /* and we do not need it for -c */
    trn_getwd(tcbuf, tcbuf_len); /* find working directory name */
    g_origdir = tcbuf;           /* and remember it */

    /* name of header file (%h) */

    g_headname = filexp(HEADNAME);

    /* the hostname to use in local-article comparisons */
#if HOSTBITS != 0
    i = (HOSTBITS < 2? 2 : HOSTBITS);
    static char buff[128];
    strcpy(buff, g_p_host_name.c_str());
    g_hostname = buff+strlen(buff)-1;
    while (i && g_hostname != buff) {
	if (*--g_hostname == '.')
	    i--;
    }
    if (*g_hostname == '.')
	g_hostname++;
#else
    g_hostname = g_p_host_name.c_str();
#endif
}

void intrp_final()
{
    g_headname.clear();
    g_hostname = nullptr;
    g_origdir.clear();
}

/* skip interpolations */

static char *skipinterp(char *pattern, const char *stoppers)
{
#ifdef DEBUG
    if (debug & DEB_INTRP)
	printf("skipinterp %s (till %s)\n",pattern,stoppers?stoppers:"");
#endif

    while (*pattern && (!stoppers || !strchr(stoppers,*pattern))) {
	if (*pattern == '%' && pattern[1]) {
	switch_again:
	    switch (*++pattern) {
	    case '^':
	    case '_':
	    case '\\':
	    case '\'':
	    case '>':
	    case ')':
		goto switch_again;
	    case ':':
		pattern++;
		while (*pattern
		 && (*pattern=='.' || *pattern=='-' || isdigit(*pattern))) {
		    pattern++;
		}
		pattern--;
		goto switch_again;
	    case '{':
		for (pattern++; *pattern && *pattern != '}'; pattern++)
		    if (*pattern == '\\')
			pattern++;
		break;
	    case '[':
		for (pattern++; *pattern && *pattern != ']'; pattern++)
		    if (*pattern == '\\')
			pattern++;
		break;
	    case '(': {
		pattern = skipinterp(pattern+1,"!=");
		if (!*pattern)
		    goto getout;
		for (pattern++; *pattern && *pattern != '?'; pattern++)
		    if (*pattern == '\\')
			pattern++;
		if (!*pattern)
		    goto getout;
		pattern = skipinterp(pattern+1,":)");
		if (*pattern == ':')
		    pattern = skipinterp(pattern+1,")");
		break;
	    }
	    case '`': {
		pattern = skipinterp(pattern+1,"`");
		break;
	    }
	    case '"':
		pattern = skipinterp(pattern+1,"\"");
		break;
	    default:
		break;
	    }
	    pattern++;
	}
	else {
	    if (*pattern == '^'
	     && ((Uchar)pattern[1]>='?' || pattern[1]=='(' || pattern[1]==')'))
		pattern += 2;
	    else if (*pattern == '\\' && pattern[1])
		pattern += 2;
	    else
		pattern++;
	}
    }
getout:
    return pattern;			/* where we left off */
}

/* interpret interpolations */
char *dointerp(char *dest, int destsize, char *pattern, const char *stoppers, char *cmd)
{
    char* subj_buf = nullptr;
    char* ngs_buf = nullptr;
    char* refs_buf = nullptr;
    char* artid_buf = nullptr;
    char* reply_buf = nullptr;
    char* from_buf = nullptr;
    char* path_buf = nullptr;
    char* follow_buf = nullptr;
    char* dist_buf = nullptr;
    char* line_buf = nullptr;
    char* line_split = nullptr;
    char* orig_dest = dest;
    char scrbuf[8192];
    static char* input_str = nullptr;
    static int input_siz = 0;
    int metabit = 0;

    while (*pattern && (!stoppers || !strchr(stoppers, *pattern)))
    {
        if (*pattern == '%' && pattern[1])
        {
            char spfbuf[512];
            bool upper = false;
	    bool lastcomp = false;
	    bool re_quote = false;
	    int tick_quote = 0;
	    bool address_parse = false;
	    bool comment_parse = false;
	    bool proc_sprintf = false;
	    char *s = nullptr;
	    while (s == nullptr) {
		switch (*++pattern) {
		case '^':
		    upper = true;
		    break;
		case '_':
		    lastcomp = true;
		    break;
		case '\\':
		    re_quote = true;
		    break;
		case '\'':
		    tick_quote++;
		    break;
		case '>':
		    address_parse = true;
		    break;
		case ')':
		    comment_parse = true;
		    break;
		case ':':
		{
		    proc_sprintf = true;
		    char *h = spfbuf;
		    *h++ = '%';
		    pattern++;	/* Skip over ':' */
		    while (*pattern
                     && (*pattern=='.' || *pattern=='-' || isdigit(*pattern))) {
		        *h++ = *pattern++;
                     }
		    *h++ = 's';
		    *h++ = '\0';
		    pattern--;
		    break;
		}
		case '/':
		    s = scrbuf;
		    if (!cmd || !strchr("/?g",*cmd))
			*s++ = '/';
		    strcpy(s,g_lastpat.c_str());
		    s += strlen(s);
		    if (!cmd || *cmd != 'g') {
			if (cmd && strchr("/?",*cmd))
			    *s++ = *cmd;
			else
			    *s++ = '/';
			if (g_art_doread)
			    *s++ = 'r';
			if (g_art_howmuch != ARTSCOPE_SUBJECT) {
			    *s++ = g_scopestr[g_art_howmuch];
			    if (g_art_howmuch == ARTSCOPE_ONEHDR) {
				safecpy(s,g_htype[g_art_srchhdr].name,
					(sizeof scrbuf) - (s-scrbuf));
				if (!(s = strchr(s,':')))
				    s = scrbuf+(sizeof scrbuf)-1;
				else
				    s++;
			    }
			}
		    }
		    *s = '\0';
		    s = scrbuf;
		    break;
		case '{':
		{
		    pattern = cpytill(scrbuf,pattern+1,'}');
		    char *m = strchr(scrbuf, '-');
		    if (m != nullptr)
		        *m++ = '\0';
		    else
		        m = s_empty;
		    s = get_val(scrbuf,m);
		    break;
		}
		case '<':
		    pattern = cpytill(scrbuf,pattern+1,'>');
                    s = strchr(scrbuf, '-');
                    if (s != nullptr)
			*s++ = '\0';
		    else
			s = s_empty;
		    interp(scrbuf, 8192, get_val(scrbuf,s));
		    s = scrbuf;
		    break;
		case '[':
		    if (g_in_ng) {
			pattern = cpytill(scrbuf,pattern+1,']');
			header_line_type which_line;
                        if (*scrbuf && (which_line = get_header_num(scrbuf)) != SOME_LINE)
                        {
                            safefree(line_buf);
                            line_buf = fetchlines(g_art, which_line);
                            s = line_buf;
                        }
			else
			    s = s_empty;
		    }
		    else
			s = s_empty;
		    break;
		case '(': {
		    COMPEX *oldbra_compex = g_bra_compex;
		    char rch;
		    bool matched;
		    
		    pattern = dointerp(dest,destsize,pattern+1,"!=",cmd);
		    rch = *pattern;
		    if (rch == '!')
			pattern++;
		    if (*pattern != '=')
			goto getout;
		    pattern = cpytill(scrbuf,pattern+1,'?');
		    if (!*pattern)
			goto getout;
		    s = scrbuf;
		    char *h = spfbuf;
		    proc_sprintf = false;
		    do {
			switch (*s) {
			case '^':
			    *h++ = '\\';
			    break;
			case '\\':
			    *h++ = '\\';
			    *h++ = '\\';
			    break;
			case '%':
			    proc_sprintf = true;
			    break;
			}
			*h++ = *s;
		    } while (*s++);
		    if (proc_sprintf) {
			dointerp(scrbuf,sizeof scrbuf,spfbuf,nullptr,cmd);
			proc_sprintf = false;
		    }
                    s = compile(&s_cond_compex, scrbuf, true, true);
                    if (s != nullptr)
                    {
			printf("%s: %s\n",scrbuf,s) FLUSH;
			pattern += strlen(pattern);
			free_compex(&s_cond_compex);
			goto getout;
		    }
		    matched = (execute(&s_cond_compex,dest) != nullptr);
		    if (getbracket(&s_cond_compex, 0)) /* were there brackets? */
			g_bra_compex = &s_cond_compex;
		    if (matched==(rch == '=')) {
			pattern = dointerp(dest,destsize,pattern+1,":)",cmd);
			if (*pattern == ':')
			    pattern = skipinterp(pattern+1,")");
		    }
		    else {
			pattern = skipinterp(pattern+1,":)");
			if (*pattern == ':')
			    pattern++;
			pattern = dointerp(dest,destsize,pattern,")",cmd);
		    }
		    s = dest;
		    g_bra_compex = oldbra_compex;
		    free_compex(&s_cond_compex);
		    break;
		}
		case '`': {
		    pattern = dointerp(scrbuf,(sizeof scrbuf),pattern+1,"`",cmd);
		    FILE* pipefp = popen(scrbuf,"r");
		    if (pipefp != nullptr) {
                        int len = fread(scrbuf, sizeof(char), (sizeof scrbuf) - 1, pipefp);
			scrbuf[len] = '\0';
			pclose(pipefp);
		    }
		    else {
			printf("\nCan't run %s\n",scrbuf);
			*scrbuf = '\0';
		    }
		    for (s=scrbuf; *s; s++) {
			if (*s == '\n') {
			    if (s[1])
				*s = ' ';
			    else
				*s = '\0';
			}
		    }
		    s = scrbuf;
		    break;
		}
		case '"':
		{
		    pattern = dointerp(scrbuf,(sizeof scrbuf),pattern+1,"\"",cmd);
		    fputs(scrbuf,stdout) FLUSH;
		    resetty();
		    fgets(scrbuf, sizeof scrbuf, stdin);
		    noecho();
		    crmode();
		    int i = strlen(scrbuf);
		    if (scrbuf[i-1] == '\n') {
		        scrbuf[--i] = '\0';
		    }
		    growstr(&input_str, &input_siz, i+1);
		    safecpy(input_str, scrbuf, i+1);
		    s = input_str;
		    break;
		}
		case '~':
		    s = g_home_dir;
		    break;
		case '.':
                    strcpy(scrbuf, g_dot_dir.c_str());
                    s = scrbuf;
                    break;
                case '+':
                    strcpy(scrbuf, g_trn_dir.c_str());
                    s = scrbuf;
                    break;
                case '$':
                    sprintf(scrbuf, "%ld", g_our_pid);
		    s = scrbuf;
		    break;
		case '#':
                    if (upper)
                    {
                        static int counter = 0;
                        sprintf(scrbuf, "%d", ++counter);
                    }
                    else
                        sprintf(scrbuf, "%d", g_perform_cnt);
                    s = scrbuf;
		    break;
		case '?':
		    s = " ";
		    line_split = dest;
		    break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		    s = getbracket(g_bra_compex,*pattern - '0');
		    break;
		case 'a':
		    if (g_in_ng) {
			s = scrbuf;
			sprintf(s,"%ld",(long)g_art);
		    }
		    else
			s = s_empty;
		    break;
		case 'A':
		    if (g_in_ng) {
			if (g_datasrc->flags & DF_REMOTE) {
			    if (artopen(g_art,(ART_POS)0)) {
				nntp_finishbody(FB_SILENT);
				sprintf(s = scrbuf,"%s/%s",g_datasrc->spool_dir,
					nntp_artname(g_art, false));
			    }
			    else
				s = s_empty;
			}
			else
			    sprintf(s = scrbuf,"%s/%s/%ld",g_datasrc->spool_dir,
				    g_ngdir.c_str(),(long)g_art);
		    }
		    else
			s = s_empty;
		    break;
		case 'b':
		    strcpy(scrbuf, g_savedest.c_str());
		    s = scrbuf;
		    break;
		case 'B':
		    s = scrbuf;
		    sprintf(s,"%ld",(long)g_savefrom);
		    break;
		case 'c':
		    strcpy(scrbuf, g_ngdir.c_str());
		    s = scrbuf;
		    break;
		case 'C':
		    s = g_ngname? g_ngname : s_empty;
		    break;
		case 'd':
                    if (!g_ngdir.empty())
                    {
                        sprintf(scrbuf, "%s/%s", g_datasrc->spool_dir, g_ngdir.c_str());
                        s = scrbuf;
                    }
                    else
                        s = s_empty;
		    break;
		case 'D':
		    if (g_in_ng)
		    {
                        dist_buf = fetchlines(g_art, DIST_LINE);
                        s = dist_buf;
		    }
		    else
			s = s_empty;
		    break;
		case 'e':
		{
		    static char dash[]{"-"};
		    if(g_extractprog.empty())
			s = dash;
		    else
		    {
			strcpy(scrbuf, g_extractprog.c_str());
			s = scrbuf;
		    }
		    break;
		}
		case 'E':
		    if (g_extractdest.empty())
			s = s_empty;
		    else
		    {
			strcpy(scrbuf, g_extractdest.c_str());
			s = scrbuf;
		    }
		    break;
		case 'f':			/* from line */
		    if (g_in_ng) {
			parseheader(g_art);
			if (g_htype[REPLY_LINE].minpos >= 0 && !comment_parse) {
						/* was there a reply line? */
			    if (!(s=reply_buf))
			    {
                                reply_buf = fetchlines(g_art, REPLY_LINE);
                                s = reply_buf;
			    }
			}
			else if (!(s = from_buf))
			{
                            from_buf = fetchlines(g_art, FROM_LINE);
                            s = from_buf;
			}
		    }
		    else
			s = s_empty;
		    break;
		case 'F':
		    if (g_in_ng) {
			parseheader(g_art);
			if (g_htype[FOLLOW_LINE].minpos >= 0)
					/* is there a Followup-To line? */
			{
                            follow_buf = fetchlines(g_art, FOLLOW_LINE);
                            s = follow_buf;
			}
			else
			{
                            ngs_buf = fetchlines(g_art, NGS_LINE);
                            s = ngs_buf;
			}
		    }
		    else
			s = s_empty;
		    break;
		case 'g':			/* general mode */
		    scrbuf[0] = static_cast<char>(g_general_mode);
		    scrbuf[1] = '\0';
		    s = scrbuf;
		    break;
		case 'h':			/* header file name */
		    strcpy(scrbuf, g_headname.c_str());
		    s = scrbuf;
		    break;
		case 'H':			/* host name in postings */
		    strcpy(scrbuf, g_p_host_name.c_str());
		    s = scrbuf;
		    break;
		case 'i':
		    if (g_in_ng) {
			if (!(s=artid_buf))
			{
                            artid_buf = fetchlines(g_art, MSGID_LINE);
                            s = artid_buf;
			}
			if (*s && *s != '<') {
			    sprintf(scrbuf,"<%s>",artid_buf);
			    s = scrbuf;
			}
		    }
		    else
			s = s_empty;
		    break;
		case 'I':			/* indent string for quoting */
		    sprintf(scrbuf,"'%s'",g_indstr.c_str());
		    s = scrbuf;
		    break;
		case 'j':
		    s = scrbuf;
		    sprintf(scrbuf,"%d",g_just_a_sec*10);
		    break;
		case 'l':			/* news admin login */
#ifdef HAS_NEWS_ADMIN
		    strcpy(scrbuf, g_newsadmin.c_str());
#else
		    strcpy(scrbuf, "???");
#endif
		    s = scrbuf;
		    break;
		case 'L':			/* login id */
		    strcpy(scrbuf, g_login_name.c_str());
		    s = scrbuf;
		    break;
		case 'm':		/* current mode */
		    s = scrbuf;
		    *s = static_cast<char>(g_mode);
		    s[1] = '\0';
		    break;
		case 'M':
		    sprintf(scrbuf,"%ld",(long)g_dmcount);
		    s = scrbuf;
		    break;
		case 'n':			/* newsgroups */
		    if (g_in_ng)
		    {
                        ngs_buf = fetchlines(g_art, NGS_LINE);
                        s = ngs_buf;
		    }
		    else
			s = s_empty;
		    break;
		case 'N':			/* full name */
                    strcpy(scrbuf, g_real_name.c_str());
                    s = get_val("NAME", scrbuf);
		    break;
		case 'o':			/* organization */
#ifdef IGNOREORG
		    s = get_val("NEWSORG",s_orgname); 
#else
		    s = get_val("NEWSORG",nullptr);
		    if (s == nullptr) 
			s = get_val("ORGANIZATION",s_orgname); 
#endif
		    s = filexp(s);
		    if (FILE_REF(s)) {
			FILE* ofp = fopen(s,"r");

			if (ofp) {
			    if (fgets(scrbuf,sizeof scrbuf,ofp) == nullptr)
			    	*scrbuf = '\0';
			    fclose(ofp);
			    s = scrbuf+strlen(scrbuf)-1;
			    if (*scrbuf && *s == '\n')
				*s = '\0';
			    s = scrbuf;
			}
			else
			    s = s_empty;
		    }
		    break;
		case 'O':
		    strcpy(scrbuf, g_origdir.c_str());
		    s = scrbuf;
		    break;
		case 'p':
		    s = g_cwd;
		    break;
		case 'P':
		    s = g_datasrc? g_datasrc->spool_dir : s_empty;
		    break;
		case 'q':
		    s = input_str;
		    break;
		case 'r':
		    if (g_in_ng) {
			parseheader(g_art);
			safefree0(refs_buf);
			if (g_htype[REFS_LINE].minpos >= 0) {
			    refs_buf = fetchlines(g_art,REFS_LINE);
			    normalize_refs(refs_buf);
                            s = strrchr(refs_buf, '<');
                            if (s != nullptr)
				break;
			}
		    }
		    s = s_empty;
		    break;
		case 'R': {
		    int len, j;

		    if (!g_in_ng) {
			s = s_empty;
			break;
		    }
		    parseheader(g_art);
		    safefree0(refs_buf);
		    if (g_htype[REFS_LINE].minpos >= 0) {
			refs_buf = fetchlines(g_art,REFS_LINE);
			len = strlen(refs_buf)+1;
			normalize_refs(refs_buf);
			/* no more than 3 prior references PLUS the
			** root article allowed, including the one
			** concatenated below */
                        s = strrchr(refs_buf, '<');
                        if (s != nullptr && s > refs_buf)
                        {
			    *s = '\0';
			    char *h = strrchr(refs_buf,'<');
			    *s = '<';
			    if (h && h > refs_buf) {
				s = strchr(refs_buf+1,'<');
				if (s < h)
				    safecpy(s,h,len);
			    }
			}
		    }
		    else
			len = 0;
		    if (!artid_buf)
			artid_buf = fetchlines(g_art,MSGID_LINE);
		    int i = refs_buf? strlen(refs_buf) : 0;
		    j = strlen(artid_buf) + (i? 1 : 0)
		      + (artid_buf[0] == '<'? 0 : 2) + 1;
		    if (len < i + j)
			refs_buf = saferealloc(refs_buf, i + j);
		    if (i)
			refs_buf[i++] = ' ';
		    if (artid_buf[0] == '<')
			strcpy(refs_buf+i, artid_buf);
		    else if (artid_buf[0])
			sprintf(refs_buf+i, "<%s>", artid_buf);
		    else
			refs_buf[i] = '\0';
		    s = refs_buf;
		    break;
		}
		case 's':
		case 'S': {
		    char* str;
		    if (!g_in_ng || !g_art || !g_artp) {
			s = s_empty;
			break;
		    }
                    str = subj_buf;
                    if (str == nullptr)
                    {
                        subj_buf = fetchsubj(g_art, true);
                        str = subj_buf;
                    }
		    subject_has_Re(str,&str);
		    char *h;
                    if (*pattern == 's' && (h = in_string(str, "- (nf", true)) != nullptr)
			*h = '\0';
		    s = str;
		    break;
		}
		case 't':
		case 'T':
		    if (!g_in_ng) {
			s = s_empty;
			break;
		    }
		    parseheader(g_art);
		    if (g_htype[REPLY_LINE].minpos >= 0) {
					/* was there a reply line? */
			if (!(s=reply_buf))
			{
                            reply_buf = fetchlines(g_art, REPLY_LINE);
                            s = reply_buf;
			}
		    }
		    else if (!(s = from_buf))
		    {
                        from_buf = fetchlines(g_art, FROM_LINE);
                        s = from_buf;
		    }
		    else
			s = "noname";
		    if (*pattern == 'T') {
			if (g_htype[PATH_LINE].minpos >= 0) {
					/* should we substitute path? */
                            path_buf = fetchlines(g_art, PATH_LINE);
                            s = path_buf;
			}
			int i = strlen(g_p_host_name.c_str());
			if (!strncmp(g_p_host_name.c_str(),s,i) && s[i] == '!')
			    s += i + 1;
		    }
		    address_parse = true;	/* just the good part */
		    break;
		case 'u':
		    if (g_in_ng) {
			sprintf(scrbuf,"%ld",(long)g_ngptr->toread);
			s = scrbuf;
		    }
		    else
			s = s_empty;
		    break;
		case 'U': {
		    int unseen;

		    if (!g_in_ng) {
			s = s_empty;
			break;
		    }
		    unseen = (g_art <= g_lastart) && !was_read(g_art);
		    if (g_selected_only) {
			int selected;

			selected = g_curr_artp && (g_curr_artp->flags & AF_SEL);
			sprintf(scrbuf,"%ld",
				(long)g_selected_count - (selected && unseen));
		    }
		    else
			sprintf(scrbuf,"%ld",(long)g_ngptr->toread - unseen);
		    s = scrbuf;
		    break;
		}
		case 'v': {
                    if (g_in_ng) {
                        int selected = g_curr_artp && (g_curr_artp->flags & AF_SEL);
                        int unseen = (g_art <= g_lastart) && !was_read(g_art);
                        sprintf(scrbuf, "%ld", (long)g_ngptr->toread - g_selected_count - (!selected && unseen));
                        s = scrbuf;
		    }
		    else
			s = s_empty;
		    break;
		}
		case 'V':
		    s = g_patchlevel + 1;
		    break;
		case 'W':
		    s = g_datasrc? g_datasrc->thread_dir : s_empty;
		    break;
		case 'x':			/* news library */
		    strcpy(scrbuf, g_lib.c_str());
		    s = scrbuf;
		    break;
		case 'X':			/* rn library */
		    strcpy(scrbuf, g_rn_lib.c_str());
		    s = scrbuf;
		    break;
		case 'y':	/* from line with *-shortening */
		    if (!g_in_ng) {
			s = s_empty;
			break;
		    }
		    /* XXX Rewrite this! */
		    {	/* sick, but I don't want to hunt down a buf... */
			static char tmpbuf[1024];
			char* s2;
			char* s3;
			int i = 0;

			s2 = fetchlines(g_art,FROM_LINE);
			strcpy(tmpbuf,s2);
			free(s2);
			for (s2=tmpbuf;
			     (*s2 && (*s2 != '@') && (*s2 !=' '));s2++)
				; /* EMPTY */
			if (*s2 == '@') {	/* we have normal form... */
			    for (s3=s2+1;(*s3 && (*s3 != ' '));s3++)
				if (*s3 == '.')
				    i++;
			}
			if (i>1) { /* more than one dot */
			    s3 = s2;	/* will be incremented before use */
			    while (i>=2) {
				s3++;
				if (*s3 == '.')
				    i--;
			    }
			    s2++;
			    *s2 = '*';
			    s2++;
			    *s2 = '\0';
			    from_buf = (char*)safemalloc(
				(strlen(tmpbuf)+strlen(s3)+1)*sizeof(char));
			    strcpy(from_buf,tmpbuf);
			    strcat(from_buf,s3);
			} else {
			    from_buf = savestr(tmpbuf);
			}
			s = from_buf;
		    }
		    break;
		case 'Y':
		    from_buf = savestr(g_tmp_dir);
		    s = from_buf;
		    break;
		case 'z':
		    if (!g_in_ng) {
			s = s_empty;
			break;
		    }
		    s = scrbuf;
		    sprintf(s,"%ld",(long)g_art);
		    if (stat(s,&g_filestat) < 0)
			g_filestat.st_size = 0L;
		    sprintf(scrbuf,"%5ld",(long)g_filestat.st_size);
		    s = scrbuf;
		    break;
		case 'Z':
		    sprintf(scrbuf,"%ld",(long)g_selected_count);
		    s = scrbuf;
		    break;
		case '\0':
		    s = s_empty;
		    break;
		default:
		    if (--destsize <= 0)
			abort_interp();
		    *dest++ = *pattern | metabit;
		    s = s_empty;
		    break;
		}
	    }
	    if (proc_sprintf) {
		sprintf(scrbuf,spfbuf,s);
		s = scrbuf;
	    }
	    if (*pattern)
		pattern++;
	    if (upper || lastcomp) {
		char* t;

		if (s != scrbuf) {
		    safecpy(scrbuf,s,sizeof scrbuf);
		    s = scrbuf;
		}
		if (upper || !(t = strrchr(s,'/')))
		    t = s;
		while (*t && !isalpha(*t))
		    t++;
		if (islower(*t))
		    *t = toupper(*t);
	    }
	    /* Do we have room left? */
	    int i = strlen(s);
	    if (destsize <= i)
		abort_interp();
	    destsize -= i;	/* adjust the size now. */

	    /* A maze of twisty little conditions, all alike... */
	    if (address_parse || comment_parse) {
		if (s != scrbuf) {
		    safecpy(scrbuf,s,sizeof scrbuf);
		    s = scrbuf;
		}
		decode_header(s, s, strlen(s));
		if (address_parse) {
                    char *h = strchr(s, '<');
		    if (h != nullptr) { /* grab the good part */
			s = h+1;
			if ((h=strchr(s,'>')) != nullptr)
			    *h = '\0';
		    } else if ((h=strchr(s,'(')) != nullptr) {
			while (h-- != s && *h == ' ')
			    ;
			h[1] = '\0';		/* or strip the comment */
		    }
		} else {
		    if (!(s = extract_name(s)))
			s = s_empty;
		}
	    }
	    if (metabit) {
		/* set meta bit while copying. */
		i = metabit;		/* maybe get into register */
		if (s == dest) {
		    while (*dest)
			*dest++ |= i;
		} else {
		    while (*s)
			*dest++ = *s++ | i;
		}
	    } else if (re_quote || tick_quote) {
		/* put a backslash before regexp specials while copying. */
		if (s == dest) {
		    /* copy out so we can copy in. */
		    safecpy(scrbuf, s, sizeof scrbuf);
		    s = scrbuf;
		    if (i > sizeof scrbuf)	/* we truncated, ack! */
			abort_interp();
		}
		while (*s) {
		    if ((re_quote && strchr(s_regexp_specials, *s))
		     || (tick_quote == 2 && *s == '"')) {
			if (--destsize <= 0)
			    abort_interp();
			*dest++ = '\\';
		    }
		    else if (re_quote && *s == ' ' && s[1] == ' ') {
			*dest++ = ' ';
			*dest++ = '*';
			while (*++s == ' ') ;
			continue;
		    }
		    else if (tick_quote && *s == '\'') {
			if ((destsize -= 3) <= 0)
			    abort_interp();
			*dest++ = '\'';
			*dest++ = '\\';
			*dest++ = '\'';
		    }
		    *dest++ = *s++;
		}
	    } else {
		/* straight copy. */
		if (s == dest) {
		    dest += i;
		} else {
		    while (*s)
			*dest++ = *s++;
		}
	    }
	}
	else {
	    if (--destsize <= 0)
		abort_interp();
	    if (*pattern == '^' && pattern[1]) {
		pattern++;
		int i = *(Uchar*)pattern;	/* get char after arrow into a register */
		if (i == '?')
		    *dest++ = '\177' | metabit;
		else if (i == '(') {
		    metabit = 0200;
		    destsize++;
		}
		else if (i == ')') {
		    metabit = 0;
		    destsize++;
		}
		else if (i >= '@')
		    *dest++ = (i & 037) | metabit;
		else
		    *dest++ = *--pattern | metabit;
		pattern++;
	    }
	    else if (*pattern == '\\' && pattern[1]) {
		++pattern;		/* skip backslash */
		pattern = interp_backslash(dest, pattern) + 1;
		*dest++ |= metabit;
	    }
	    else if (*pattern)
		*dest++ = *pattern++ | metabit;
	}
    }
    *dest = '\0';
    if (line_split != nullptr)
	if ((int)strlen(orig_dest) > 79)
	    *line_split = '\n';
getout:
    safefree(subj_buf);		/* return any checked out storage */
    safefree(ngs_buf);
    safefree(refs_buf);
    safefree(artid_buf);
    safefree(reply_buf);
    safefree(from_buf);
    safefree(path_buf);
    safefree(follow_buf);
    safefree(dist_buf);
    safefree(line_buf);

    return pattern; /* where we left off */
}

char *interp_backslash(char *dest, char *pattern)
{
    int i = *pattern;

    if (i >= '0' && i <= '7') {
	i = 0;
	while (i < 01000 && *pattern >= '0' && *pattern <= '7') {
	    i <<= 3;
	    i += *pattern++ - '0';
	}
	*dest = (char)(i & 0377);
	return pattern - 1;
    }
    switch (i) {
    case 'b':
	*dest = '\b';
	break;
    case 'f':
	*dest = '\f';
	break;
    case 'n':
	*dest = '\n';
	break;
    case 'r':
	*dest = '\r';
	break;
    case 't':
	*dest = '\t';
	break;
    case '\0':
	*dest = '\\';
	return pattern - 1;
    default:
	*dest = (char)i;
	break;
    }
    return pattern;
}

/* helper functions */

char *interp(char *dest, int destsize, char *pattern)
{
    return dointerp(dest,destsize,pattern,nullptr,nullptr);
}

char *interpsearch(char *dest, int destsize, char *pattern, char *cmd)
{
    return dointerp(dest,destsize,pattern,nullptr,cmd);
}

/* normalize a references line in place */

void normalize_refs(char *refs)
{
    char* t = refs;
    
    for (char *f = refs; *f;)
    {
	if (*f == '<') {
	    while (*f && (*t++ = *f++) != '>') ;
	    while (*f == ' ' || *f == '\t' || *f == '\n' || *f == ',') f++;
	    if (f != t)
		*t++ = ' ';
	}
	else
	    f++;
    }
    if (t != refs && t[-1] == ' ')
	t--;
    *t = '\0';
} 

static void abort_interp()
{
    fputs("\n% interp buffer overflow!\n",stdout) FLUSH;
    sig_catcher(0);
}
