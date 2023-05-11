/* univ.c
 */

/* Universal selector
 *
 */

#include "common.h"
#include "univ.h"

#include "cache.h"
#include "env.h"
#include "final.h"
#include "hash.h"
#include "head.h"
#include "help.h"
#include "list.h"
#include "ng.h"
#include "ngdata.h"
#include "rcstuff.h"
#include "rt-select.h"
#include "rt-util.h"
#include "score.h"
#include "string-algos.h"
#include "term.h"
#include "trn.h"
#include "url.h"
#include "util.h"
#include "util2.h"

/* TODO:
 *
 * Be friendlier when a file has no contents.
 * Implement virtual groups (largely done)
 * Help scan mode replacement
 * Lots more to do...
 */

int  g_univ_level{};         /* How deep are we in the tree? */
bool g_univ_ng_virtflag{};   /* if true, we are in the "virtual group" second pass */
bool g_univ_read_virtflag{}; /* if true, we are reading an article from a "virtual group" */
bool g_univ_default_cmd{};   /* "follow"-related stuff (virtual groups) */
bool g_univ_follow{true};
bool g_univ_follow_temp{};

/* items which must be saved in context */
UNIV_ITEM  *g_first_univ{};
UNIV_ITEM  *g_last_univ{};
UNIV_ITEM  *sel_page_univ{};
UNIV_ITEM  *g_sel_next_univ{};
char       *g_univ_fname{};  /* current filename (may be null) */
std::string g_univ_label;    /* current label (may be null) */
std::string g_univ_title;    /* title of current level */
std::string g_univ_tmp_file; /* temp. file (may be null) */
HASHTABLE  *g_univ_ng_hash{};
HASHTABLE  *g_univ_vg_hash{};
/* end of items that must be saved */

static bool       s_univ_virt_pass_needed{}; //
static int        s_univ_item_counter{1};    //
static bool       s_univ_done_startup{};     //
static int        s_univ_min_score{};        /* this score is part of the line format, so it is not ifdefed */
static bool       s_univ_use_min_score{};    //
static bool       s_univ_begin_found{};      //
static char      *s_univ_begin_label{};      /* label to start working with */
static char      *s_univ_line_desc{};        /* if non-nullptr, the description (printing name) of the entry */
static UNIV_ITEM *s_current_vg_ui{};         //
static bool       s_univ_usrtop{};           /* if true, the user has loaded their own top univ. config file */

static void univ_free_data(UNIV_ITEM *ui);
static bool univ_DoMatch(const char *text, const char *p);
static bool univ_use_file(const char *fname, const char *label);
static bool univ_include_file(const char *fname);
static void univ_do_line_ext1(const char *desc, char *line);
static bool univ_do_line(char *line);
static char*univ_edit_new_userfile();
static void univ_vg_addart(ART_NUM a);
static void univ_vg_addgroup();
static int  univ_order_number(const UNIV_ITEM**ui1, const UNIV_ITEM**ui2);
static int  univ_order_score(const UNIV_ITEM**ui1, const UNIV_ITEM**ui2);

void univ_init()
{
    g_univ_level = 0;
}

void univ_startup()
{
    /* later: make user top file an option or environment variable? */
    if (!univ_file_load("%+/univ/top","Top Level",nullptr)) {
        univ_open();
	g_univ_title = "Top Level";
	g_univ_fname = savestr("%+/univ/usertop");

	/* read in trn default top file */
	(void)univ_include_file("%X/sitetop");		/* pure local */
	bool sys_top_load = univ_include_file("%X/trn4top");
	bool user_top_load = univ_use_file("%+/univ/usertop", nullptr);

	if (!(sys_top_load || user_top_load)) {
	    /* last resort--all newsgroups */
	    univ_close();
	    univ_mask_load(savestr("*"),"All Newsgroups");
	}
	if (user_top_load) {
	    s_univ_usrtop = true;
	}
    } else {
	s_univ_usrtop = true;
    }
    s_univ_done_startup = true;
}

void univ_open()
{
    g_first_univ = nullptr;
    g_last_univ = nullptr;
    sel_page_univ = nullptr;
    g_sel_next_univ = nullptr;
    g_univ_fname = nullptr;
    g_univ_title.clear();
    g_univ_label.clear();
    g_univ_tmp_file.clear();
    s_univ_virt_pass_needed = false;
    g_univ_ng_hash = nullptr;
    g_univ_vg_hash = nullptr;
    g_univ_level++;
}

void univ_close()
{
    UNIV_ITEM* nextnode;

    for (UNIV_ITEM *node = g_first_univ; node; node = nextnode) {
	univ_free_data(node);
	safefree(node->desc);
	nextnode = node->next;
	free((char*)node);
    }
    if (!g_univ_tmp_file.empty()) {
	remove(g_univ_tmp_file.c_str());
	g_univ_tmp_file.clear();
    }
    safefree(g_univ_fname);
    g_univ_title.clear();
    g_univ_label.clear();
    if (g_univ_ng_hash) {
	hashdestroy(g_univ_ng_hash);
	g_univ_ng_hash = nullptr;
    }
    if (g_univ_vg_hash) {
	hashdestroy(g_univ_vg_hash);
	g_univ_vg_hash = nullptr;
    }
    g_first_univ = nullptr;
    g_last_univ = nullptr;
    sel_page_univ = nullptr;
    g_sel_next_univ = nullptr;
    g_univ_level--;
}

UNIV_ITEM *univ_add(univ_item_type type, const char *desc)
{
    UNIV_ITEM *node = (UNIV_ITEM*)safemalloc(sizeof(UNIV_ITEM));

    node->flags = UF_NONE;
    if (desc)
	node->desc = savestr(desc);
    else
	node->desc = nullptr;
    node->type = type;
    node->num = s_univ_item_counter++;
    node->score = 0;		/* consider other default scores? */
    node->next = nullptr;
    node->prev = g_last_univ;
    if (g_last_univ)
	g_last_univ->next = node;
    else
	g_first_univ = node;
    g_last_univ = node;

    return node;
}

static void univ_free_data(UNIV_ITEM *ui)
{
    if (!ui)
	return;

    switch (ui->type) {
      case UN_NONE:	/* cases where nothing is needed. */
      case UN_TXT:
      case UN_HELPKEY:
	break;
      case UN_DEBUG1:	/* methods that use the string */
	safefree(ui->data.str);
	break;
      case UN_GROUPMASK:	/* methods that have custom data */
	safefree(ui->data.gmask.title);
	safefree(ui->data.gmask.masklist);
	break;
      case UN_CONFIGFILE:
	safefree(ui->data.cfile.title);
	safefree(ui->data.cfile.fname);
	safefree(ui->data.cfile.label);
	break;
      case UN_NEWSGROUP:
      case UN_GROUP_DESEL:
	safefree(ui->data.group.ng);
	break;
      case UN_ARTICLE:
	safefree(ui->data.virt.ng);
	safefree(ui->data.virt.from);
	safefree(ui->data.virt.subj);
	break;
      case UN_VGROUP:
      case UN_VGROUP_DESEL:
	safefree(ui->data.vgroup.ng);
	break;
      case UN_TEXTFILE:
	safefree(ui->data.textfile.fname);
	break;
      case UN_DATASRC:	/* unimplemented methods */
      case UN_VIRTUAL1:
      default:
	break;
    }
}

/* not used now, but may be used later... */
//UNIV_ITEM* ui;				/* universal item */
//int linenum;				/* which line to describe (0 base) */
char *univ_desc_line(UNIV_ITEM *ui, int linenum)
{
    return ui->desc;
}

void univ_add_text(const char *txt)
{
    /* later check text for bad things */
    (void)univ_add(UN_TXT,txt);
}

/* temp for testing */
void univ_add_debug(const char *desc, const char *txt)
{
    /* later check text for bad things */
    UNIV_ITEM *ui = univ_add(UN_DEBUG1, desc);
    ui->data.str = savestr(txt);
}

void univ_add_group(const char *desc, const char *grpname)
{
    UNIV_ITEM* ui;

    const char *s = grpname;
    if (!s)
	return;
    /* later check grpname for bad things? */

    if (!g_univ_ng_hash)
	g_univ_ng_hash = hashcreate(701, HASH_DEFCMPFUNC);

    HASHDATUM data = hashfetch(g_univ_ng_hash, grpname, strlen(grpname));

    if (data.dat_ptr) {
	/* group was already added */
	/* perhaps it is marked as deleted? */
	for (ui = g_first_univ; ui; ui = ui->next) {
	    if ((ui->type == UN_GROUP_DESEL) && ui->data.group.ng
	     && !strcmp(ui->data.group.ng,grpname)) {
		/* undelete the newsgroup */
		ui->type = UN_NEWSGROUP;
	    }
	}
	return;
    }
    ui = univ_add(UN_NEWSGROUP,desc);
    ui->data.group.ng = savestr(grpname);
    data.dat_ptr = ui->data.group.ng;
    hashstorelast(data);
}

void univ_add_mask(const char *desc, const char *mask)
{
    UNIV_ITEM *ui = univ_add(UN_GROUPMASK, desc);
    ui->data.gmask.masklist = savestr(mask);
    ui->data.gmask.title = savestr(desc);
}

//char* fname;				/* May be URL */
void univ_add_file(const char *desc, const char *fname, const char *label)
{
    UNIV_ITEM *ui = univ_add(UN_CONFIGFILE, desc);
    ui->data.cfile.title = savestr(desc);
    ui->data.cfile.fname = savestr(fname);
    if (label && *label)
	ui->data.cfile.label = savestr(label);
    else
	ui->data.cfile.label = nullptr;
}

UNIV_ITEM *univ_add_virt_num(const char *desc, const char *grp, ART_NUM art)
{
    UNIV_ITEM *ui = univ_add(UN_ARTICLE, desc);
    ui->data.virt.ng = savestr(grp);
    ui->data.virt.num = art;
    ui->data.virt.subj = nullptr;
    ui->data.virt.from = nullptr;
    return ui;
}

void univ_add_textfile(const char *desc, char *name)
{
    UNIV_ITEM* ui;
    char* p;
    static char lbuf[1024];

    char *s = name;
    switch (*s) {
      /* later add URL handling */
      case ':':
	s++;
      default:
	/* XXX later have error checking on length */
	strcpy(lbuf,g_univ_fname);
	for (p = lbuf+strlen(lbuf); p > lbuf && *p != '/'; p--) ;
	if (p) {
	    *p++ = '/';
	    *p = '\0';
	    strcat(lbuf,s);
	    s = lbuf;
	}
	/* FALL THROUGH */
      case '~':	/* ...or full file names */
      case '%':
      case '/':
	ui = univ_add(UN_TEXTFILE,desc);
	ui->data.textfile.fname = savestr(filexp(s));
	break;
    }
}

/* mostly the same as the newsgroup stuff */
void univ_add_virtgroup(const char *grpname)
{
    UNIV_ITEM* ui;

    if (!grpname)
	return;

    /* later check grpname for bad things? */

    /* perhaps leave if group has no unread, or other factor */
    if (!g_univ_vg_hash)
	g_univ_vg_hash = hashcreate(701, HASH_DEFCMPFUNC);

    s_univ_virt_pass_needed = true;
    HASHDATUM data = hashfetch(g_univ_vg_hash, grpname, strlen(grpname));
    if (data.dat_ptr) {
	/* group was already added */
	/* perhaps it is marked as deleted? */
	for (ui = g_first_univ; ui; ui = ui->next) {
	    if ((ui->type == UN_VGROUP_DESEL) && ui->data.vgroup.ng
	     && !strcmp(ui->data.vgroup.ng,grpname)) {
		/* undelete the newsgroup */
		ui->type = UN_VGROUP;
	    }
	}
	return;
    }
    ui = univ_add(UN_VGROUP,nullptr);
    ui->data.vgroup.flags = UF_VG_NONE;
    if (s_univ_use_min_score) {
	ui->data.vgroup.flags |= UF_VG_MINSCORE;
	ui->data.vgroup.minscore = s_univ_min_score;
    }
    ui->data.vgroup.ng = savestr(grpname);
    data.dat_ptr = ui->data.vgroup.ng;
    hashstorelast(data);
}

/* univ_DoMatch uses a modified Wildmat function which is
 * based on Rich $alz's wildmat, reduced to the simple case of *
 * and text.  The complete version can be found in wildmat.c.
 */

/*
**  Match text and p, return true, false.
*/
static bool univ_DoMatch(const char *text, const char *p)
{
    for ( ; *p; text++, p++) {
	if (*p == '*') {
            p = skip_eq(p, '*'); /* Consecutive stars act just like one. */
	    if (*p == '\0')
		/* Trailing star matches everything. */
		return true;
	    while (*text)
	    {
                if (univ_DoMatch(text++, p))
	            return true;
	    }
	    return false;
	}
	if (*text != *p) {
	    return false;
	}
    }
    return *text == '\0';
}

/* type: 0=newsgroup, 1=virtual (more in future?) */
void univ_use_pattern(const char *pattern, int type)
{
    const char* s = pattern;
    NGDATA* np;
    UNIV_ITEM* ui;

    if (!s || !*s) {
	printf("\ngroup pattern: empty regular expression\n") FLUSH;
	return;
    }
    /* XXX later: match all newsgroups in current datasrc to the pattern. */
    /* XXX later do a quick check to see if the group is a simple one. */

    if (*s == '!') {
	s++;
	switch (type) {
	  case 0:
	    for (ui = g_first_univ; ui; ui = ui->next) {
		if (ui->type == UN_NEWSGROUP && ui->data.group.ng
		  && univ_DoMatch(ui->data.group.ng,s) == true) {
		    ui->type = UN_GROUP_DESEL;
		}
	    }
	    break;
	  case 1:
	    for (ui = g_first_univ; ui; ui = ui->next) {
		if (ui->type == UN_VGROUP && ui->data.vgroup.ng
		  && univ_DoMatch(ui->data.vgroup.ng,s) == true) {
		    ui->type = UN_VGROUP_DESEL;
		}
	    }
	    break;
	}
    }
    else {
	switch (type) {
	  case 0:
	    for (np = g_first_ng; np; np = np->next) {
		if (univ_DoMatch(np->rcline,s) == true) {
		    univ_add_group(np->rcline,np->rcline);
		}
	    }
	    break;
	  case 1:
	    for (np = g_first_ng; np; np = np->next) {
		if (univ_DoMatch(np->rcline,s) == true) {
		    univ_add_virtgroup(np->rcline);
		}
	    }
	    break;
	}
    }
}

/* interprets a line of newsgroups, adding or subtracting each pattern */
/* Newsgroup patterns are separated by spaces and/or commas */
void univ_use_group_line(char *line, int type)
{
    char* p;

    char *s = line;
    if (!s || !*s)
	return;

    /* newsgroup patterns will be separated by space(s) and/or comma(s) */
    while (*s) {
	while (*s == ' ' || *s == ',') s++;
	for (p = s; *p && *p != ' ' && *p != ','; p++) ;
	char ch = *p;
	*p = '\0';
	univ_use_pattern(s,type);
	*p = ch;
	s = p;
    }
}

/* returns true on success, false otherwise */
static bool univ_use_file(const char *fname, const char *label)
{
    static char lbuf[LBUFLEN];

    bool begin_top = true;	/* default assumption (might be changed later) */
    char *p = nullptr;

    if (!fname)
	return false;	/* bad argument */

    const char *s = fname;
    const char *open_name = fname;
    /* open URLs and translate them into local temporary filenames */
    if (!strncasecmp(fname,"URL:",4)) {
        open_name = temp_filename();
	g_univ_tmp_file = open_name;
	if (!url_get(fname+4,open_name))
	    open_name = nullptr;
	begin_top = false;	/* we will need a "begin group" */
    } else if (*s == ':') {	/* relative to last file's directory */
	printf("Colon filespec not supported for |%s|\n",s) FLUSH;
	open_name = nullptr;
    }
    if (!open_name)
	return false;
    s_univ_begin_found = begin_top;
    safefree0(s_univ_begin_label);
    if (label)
	s_univ_begin_label = savestr(label);
    FILE *fp = fopen(filexp(open_name), "r");
    if (!fp)
	return false;		/* unsuccessful (XXX: complain) */
/* Later considerations:
 * 1. Long lines
 * 2. Backslash continuations
 */
    while (fgets(lbuf,sizeof lbuf,fp) != nullptr) {
	if (!univ_do_line(lbuf))
	    break;	/* end of useful file */
    }
    fclose(fp);
    if (!s_univ_begin_found)
	printf("\"begin group\" not found.\n") FLUSH;
    if (s_univ_begin_label)
	printf("label not found: %s\n",s_univ_begin_label);
    if (s_univ_virt_pass_needed) {
	univ_virt_pass();
    }
    sort_univ();
    return true;
}

static bool univ_include_file(const char *fname)
{
    char *old_univ_fname = g_univ_fname;
    g_univ_fname = savestr(fname);	/* LEAK */
    bool retval = univ_use_file(g_univ_fname, nullptr);
    g_univ_fname = old_univ_fname;
    return retval;
}

/* do the '$' extensions of the line. */
//char* line;			/* may be temporarily edited */
static void univ_do_line_ext1(const char *desc, char *line)
{
    char* p;
    char* q;
    ART_NUM a;
    char ch;

    char *s = line;

    s++;
    switch (*s) {
      case 'v':
        s++;
	switch (*s) {
	  case '0':		/* test vector: "desc" $v0 */
	    s++;
	    (void)univ_add_virt_num(desc? desc : s,
                                    "news.software.readers",(ART_NUM)15000);
	    break;
	  case '1':		/* "desc" $v1 1500 news.admin */
	    /* XXX error checking */
	    s++;
	    s = skip_space(s);
	    p = skip_digits(s);
	    ch = *p;
	    *p = '\0';
	    a = (ART_NUM)atoi(s);
	    *p = ch;
	    if (*p) {
		p++;
	        (void)univ_add_virt_num(desc ? desc : s, p, a);
	    }
	    break;
	  case 'g':		/* $vg [scorenum] news.* !news.foo.* */
	    p = s;
	    p++;
	    p = skip_space(p);
	    q = p;
	    if ((*p=='+') || (*p=='-'))
	      p++;
	    p = skip_digits(p);
	    if (isspace(*p)) {
	      *p = '\0';
	      s_univ_min_score = atoi(q);
	      s_univ_use_min_score = true;
	      s = p;
	      s++;
	    }
	    univ_use_group_line(s,1);
	    s_univ_use_min_score = false;
	    break;
	}
	break;
      case 't':		/* text file */
        s++;
	switch (*s) {
	  case '0':		/* test vector: "desc" $t0 */
	    univ_add_textfile(desc? desc : s, "/home/c/caadams/ztext");
	    break;
	}
	break;
	default:
	  break;
    }
}

/* returns false when no more lines should be interpreted */
static bool univ_do_line(char *line)
{
    char* p;

    char *s = line + strlen(line) - 1;
    if (*s == '\n')
	*s = '\0';				/* delete newline */

    s = skip_space(line);
    if (*s == '\0')
	return true;	/* empty line */

    if (!s_univ_begin_found) {
	if (strncasecmp(s,"begin group",11))
	    return true;	/* wait until "begin group" is found */
	s_univ_begin_found = true;
    }
    if (s_univ_begin_label) {
	if (*s == '>' && s[1] == ':' && !strcmp(s+2,s_univ_begin_label)) {
	    safefree0(s_univ_begin_label); /* interpret starting at next line */
	}
	return true;
    }
    safefree0(s_univ_line_desc);
    if (*s == '"') {	/* description name */
	p = cpytill(s,s+1,'"');
	if (!*p) {
	    printf("univ: unmatched quote in string:\n\"%s\"\n", s) FLUSH;
	    return true;
	}
	*p = '\0';
	s_univ_line_desc = savestr(s);
	s = p+1;
    }
    s = skip_space(s);
    if (!strncasecmp(s,"end group",9))
	return false;
    if (!strncasecmp(s,"URL:",4)) {
        p = skip_ne(s, '>');
	if (*p) {
	    p++;
	    if (!*p)		/* empty label */
		p = nullptr;
	    /* XXX later do more error checking */
	} else
	    p = nullptr;
	/* description defaults to name */
	univ_add_file(s_univ_line_desc? s_univ_line_desc : s, s, p);
    }
    else {
	switch (*s) {
	  case '#':	/* comment */
	    break;
	  case ':':	/* relative to g_univ_fname */
	    /* XXX hack the variable and fall through */
	    if (g_univ_fname && strlen(g_univ_fname)+strlen(s) < 1020) {
		static char lbuf[1024];
		strcpy(lbuf,g_univ_fname);
		for (p = lbuf+strlen(lbuf); p > lbuf && *p != '/'; p--) ;
		if (p) {
		    *p++ = '/';
		    *p = '\0';
		    s++;
		    strcat(lbuf,s);
		    s = lbuf;
		}
	    } /* XXX later have else which will complain */
	    /* FALL THROUGH */
	  case '~':	/* ...or full file names */
	  case '%':
	  case '/':
            p = skip_ne(s, '>');
	    if (*p) {
		if (strlen(s) < 1020) {
		    static char lbuf[1024];
		    strcpy(lbuf,s);
		    s = lbuf;

		    p = skip_ne(s, '>'); /* XXX Ick! */
		    *p++ = '\0';	/* separate label */

		    if (!*p)		/* empty label */
			p = nullptr;
		    /* XXX later do more error checking */
		}
	    } else
		p = nullptr;
	    /* description defaults to name */
	    univ_add_file(s_univ_line_desc? s_univ_line_desc : s, filexp(s), p);
	    break;
	  case '-':	/* label within same file */
	    s++;
	    if (*s++ != '>') {
		/* XXX give an error message later */
		break;
	    }
	    if (!g_univ_tmp_file.empty())
	    {
		static char buff[1024];
		strcpy(buff, g_univ_tmp_file.c_str());
	        p = buff;
	    }
	    else
		p = g_univ_fname;
	    univ_add_file(s_univ_line_desc? s_univ_line_desc : s, g_univ_fname, s);
	    break;
	  case '>':
	    if (s[1] == ':')
		return false;	/* label found, end of previous block */
	    break;	/* just ignore the line (print warning later?) */
	  case '@':	/* virtual newsgroup file */
	    break;	/* not used now */
	  case '&':     /* text file shortcut (for help files) */
	    s++;
	    univ_add_textfile(s_univ_line_desc? s_univ_line_desc : s, s);
	    break;
	  case '$':	/* extension 1 */
	    univ_do_line_ext1(s_univ_line_desc,s);
	    break;
	  default:
	    /* if there is a description, this must be a restriction list */
	    if (s_univ_line_desc) {
		univ_add_mask(s_univ_line_desc,s);
		break;
	    }
	    /* one or more newsgroups instead */
	    univ_use_group_line(s,0);
	    break;
	}
    }
    return true;	/* continue reading */
}

/* features to return later (?):
 *   text files
 */

/* level generator */
bool univ_file_load(const char *fname, const char *title, const char *label)
{
    univ_open();

    if (fname)
	g_univ_fname = savestr(fname);
    if (title)
	g_univ_title = title;
    if (label)
	g_univ_label = label;
    bool flag = univ_use_file(fname, label);
    if (!flag) {
	univ_close();
    }
    if (g_int_count) {
	g_int_count = 0;
    }
    if (finput_pending(true)) {
	/* later, *maybe* eat input */
    }
    return flag;
}

/* level generator */
void univ_mask_load(char *mask, const char *title)
{
    univ_open();

    univ_use_group_line(mask,0);
    if (title)
	g_univ_title = title;
    if (g_int_count) {
	g_int_count = 0;
    }
}

void univ_redofile()
{
    char *tmp_fname = (g_univ_fname ? savestr(g_univ_fname) : nullptr);
    const std::string tmp_title = g_univ_title;
    const std::string tmp_label = g_univ_label;

    univ_close();
    if (g_univ_level)
	(void)univ_file_load(tmp_fname,tmp_title.c_str(),tmp_label.c_str());
    else
	univ_startup();

    safefree(tmp_fname);
}


static char *univ_edit_new_userfile()
{
    char *s = savestr(filexp("%+/univ/usertop"));	/* LEAK */

    /* later, create a new user top file, and return its filename.
     * later perhaps ask whether to create or edit current file.
     * note: the user may need to restart in order to use the new file.
     *       (trn could do a univ_redofile, but it may be confusing.)
     */

    /* if the file exists, do not create a new one */
    FILE *fp = fopen(s, "r");
    if (fp) {
	fclose(fp);
	return g_univ_fname;	/* as if this function was not called */
    }

    makedir(s,MD_FILE);

    fp = fopen(s,"w");
    if (!fp) {
	printf("Could not create new user file.\n");
	printf("Editing current system file\n") FLUSH;
	(void)get_anything();
	return g_univ_fname;
    }
    fprintf(fp,"# User Toplevel (Universal Selector)\n");
    fclose(fp);
    printf("New User Toplevel file created.\n") FLUSH;
    printf("After editing this file, exit and restart trn to use it.\n") FLUSH;
    (void)get_anything();
    s_univ_usrtop = true;		/* do not overwrite this file */
    return s;
}

/* code adapted from edit_kfile in kfile.c */
/* XXX problem if elements expand to larger than g_cmd_buf */
void univ_edit()
{
    const char* s;

    if (s_univ_usrtop || !(s_univ_done_startup)) {
	if (!g_univ_tmp_file.empty()) {
	    s = g_univ_tmp_file.c_str();
	} else {
	    s = g_univ_fname;
	}
    } else {
	s = univ_edit_new_userfile();
    }

    /* later consider directory push/pop pair around editing */
    (void)edit_file(s);
}

/* later use some internal pager */
void univ_page_file(char *fname)
{
    if (!fname || !*fname)
	return;

    sprintf(g_cmd_buf,"%s ",
	    filexp(get_val("HELPPAGER",get_val("PAGER","more"))));
    strcat(g_cmd_buf, filexp(fname));
    termdown(3);
    resetty();			/* make sure tty is friendly */
    doshell(SH,g_cmd_buf);	/* invoke the shell */
    noecho();			/* and make terminal */
    crmode();			/*   unfriendly again */
    /* later: consider something else that will return the key, and
     *        returning different codes based on the key.
     */
    if (!strncmp(g_cmd_buf,"more ",5))
	get_anything();
}

/* virtual newsgroup second pass function */
/* called from within newsgroup */
void univ_ng_virtual()
{
    switch (s_current_vg_ui->type) {
      case UN_VGROUP:
	univ_vg_addgroup();
	break;
      case UN_ARTICLE:
	/* get article number from message-id */
	break;
      default:
	break;
    }

    /* later, get subjects and article numbers when needed */
    /* also do things like check scores, add newsgroups, etc. */
}

static void univ_vg_addart(ART_NUM a)
{
    char lbuf[70];

    int score = sc_score_art(a, false);
    if (s_univ_use_min_score && (score<s_univ_min_score))
	return;
    char *subj = fetchsubj(a, false);
    if (!subj || !*subj)
	return;
    char *from = fetchfrom(a, false);
    if (!from || !*from)
        from = "<No Author>";

    safecpy(lbuf,subj,sizeof lbuf - 4);
    /* later scan/replace bad characters */

    /* later consider author in description, scoring, etc. */
    UNIV_ITEM *ui = univ_add_virt_num(nullptr, g_ngname.c_str(), a);
    ui->score = score;
    ui->data.virt.subj = savestr(subj);
    ui->data.virt.from = savestr(from);
}


static void univ_vg_addgroup()
{
    /* later: allow was-read articles, etc... */
    for (ART_NUM a = article_first(g_firstart); a <= g_lastart; a = article_next(a)) {
	if (!article_unread(a))
	    continue;
	/* minimum score check */
	univ_vg_addart(a);
    }
}

/* returns do_newsgroup() value */
int univ_visit_group_main(const char *gname)
{
    if (!gname || !*gname)
	return NG_ERROR;

    NGDATA *np = find_ng(gname);
    if (!np) {
	printf("Univ/Virt: newsgroup %s not found!", gname) FLUSH;
	return NG_ERROR;
    }
    /* unsubscribed, bogus, etc. groups are not visited */
    if (np->toread <= TR_UNSUB)
      return NG_ERROR;

    set_ng(np);
    if (np != g_current_ng) {
	/* probably unnecessary... */
	g_recent_ng = g_current_ng;
	g_current_ng = np;
    }
    bool old_threaded = g_threaded_group;
    g_threaded_group = (g_use_threads && !(np->flags & NF_UNTHREADED));
    printf("\nScanning newsgroup %s\n",gname);
    int ret = do_newsgroup("");
    g_threaded_group = old_threaded;
    return ret;
}

/* LATER: allow the loop to be interrupted */
void univ_virt_pass()
{
    g_univ_ng_virtflag = true;
    s_univ_virt_pass_needed = false;

    for (UNIV_ITEM *ui = g_first_univ; ui; ui = ui->next) {
	if (input_pending()) {
	    /* later consider cleaning up the remains */
	    break;
	}
	switch (ui->type) {
	  case UN_VGROUP:
	    if (!ui->data.vgroup.ng)
		break;			/* XXX whine */
	    s_current_vg_ui = ui;
	    if (ui->data.vgroup.flags & UF_VG_MINSCORE) {
		s_univ_use_min_score = true;
		s_univ_min_score = ui->data.vgroup.minscore;
	    }
	    (void)univ_visit_group(ui->data.vgroup.ng);
	    s_univ_use_min_score = false;
	    /* later do something with return value */
	    univ_free_data(ui);
	    safefree(ui->desc);
	    ui->type = UN_DELETED;
	    break;
	  case UN_ARTICLE:
	    /* if article number is not set, visit newsgroup with callback */
	    /* later also check for descriptions */
	    if ((ui->data.virt.num) && (ui->desc))
	      break;
	    if (ui->data.virt.subj)
	      break;
	    s_current_vg_ui = ui;
	    (void)univ_visit_group(ui->data.virt.ng);
	    /* later do something with return value */
	    break;
	  default:
	    break;
	}
    }
    g_univ_ng_virtflag = false;
}

static int univ_order_number(const UNIV_ITEM** ui1, const UNIV_ITEM** ui2)
{
    return (int)((*ui1)->num - (*ui2)->num) * g_sel_direction;
}

static int univ_order_score(const UNIV_ITEM** ui1, const UNIV_ITEM** ui2)
{
    if ((*ui1)->score != (*ui2)->score)
	return (int)((*ui2)->score - (*ui1)->score) * g_sel_direction;
    else
	return (int)((*ui1)->num - (*ui2)->num) * g_sel_direction;
}

void sort_univ()
{
    int cnt = 0;
    for (const UNIV_ITEM *i = g_first_univ; i; i = i->next) {
	cnt++;
    }

    if (cnt<=1)
	return;

    int (*sort_procedure)(const UNIV_ITEM** ui1, const UNIV_ITEM** ui2);
    switch (g_sel_sort) {
      case SS_SCORE:
	sort_procedure = univ_order_score;
	break;
      case SS_NATURAL:
      default:
	sort_procedure = univ_order_number;
	break;
    }

    UNIV_ITEM **univ_sort_list = (UNIV_ITEM**)safemalloc(cnt * sizeof(UNIV_ITEM*));
    UNIV_ITEM** lp = univ_sort_list;
    for (UNIV_ITEM *ui = g_first_univ; ui; ui = ui->next)
	*lp++ = ui;
    TRN_ASSERT(lp - univ_sort_list == cnt);

    qsort(univ_sort_list, cnt, sizeof(UNIV_ITEM *), (int(*)(void const *, void const *))sort_procedure);

    g_first_univ = univ_sort_list[0];
    lp = univ_sort_list;
    for (int i = cnt; --i; lp++) {
	lp[0]->next = lp[1];
	lp[1]->prev = lp[0];
    }
    g_last_univ = lp[0];
    g_last_univ->next = nullptr;

    free((char*)univ_sort_list);
}

/* return a description of the article */
/* do this better later, like the code in sadesc.c */
const char *univ_article_desc(const UNIV_ITEM *ui)
{
    static char dbuf[200];
    static char sbuf[200];
    static char fbuf[200];

    char *s = ui->data.virt.subj;
    const char *f = ui->data.virt.from;
    if (!f) {
	strcpy(fbuf,"<No Author> ");
    } else {
	safecpy(fbuf,compress_from(f,16),17);
    }
    if (!s) {
	strcpy(sbuf,"<No Subject>");
    } else {
	if ((s[0] == 'R') &&
	    (s[1] == 'e') &&
	    (s[2] == ':') &&
	    (s[3] == ' ')) {
	    sbuf[0] = '>';
	    safecpy(sbuf+1,s+4,79);
	} else {
	    safecpy(sbuf,s,80);
	}
    }
    fbuf[16] = '\0';
    sbuf[55] = '\0';
    sprintf(dbuf,"[%3d] %16s %s",ui->score,fbuf,sbuf);
    for (char *t = dbuf; *t; t++) {
	if ((*t==Ctl('h')) || (*t=='\t') || (*t=='\n') || (*t=='\r')) {
	    *t = ' ';
	}
    }
    dbuf[70] = '\0';
    return dbuf;
}

/* Help start */

/* later: add online help as a new item type, add appropriate item
 *        to the new level
 */
//int where;	/* what context were we in--use later for key help? */
void univ_help_main(help_location where)
{
    univ_open();
    g_univ_title = "Extended Help";

    /* first add help on current mode */
    UNIV_ITEM *ui = univ_add(UN_HELPKEY, nullptr);
    ui->data.i = where;

    /* later, do other mode sensitive stuff */

    /* site-specific help */
    univ_include_file("%X/sitehelp/top");

    /* read in main help file */
    g_univ_fname = savestr("%X/HelpFiles/top");
    bool flag = univ_use_file(g_univ_fname, g_univ_label.c_str());

    /* later: if flag is not true, then add message? */
}

void univ_help(help_location where)
{
    univ_visit_help(where);	/* push old selector info to stack */
}

const char *univ_keyhelp_modestr(const UNIV_ITEM *ui)
{
    switch (ui->data.i) {
      case UHELP_PAGE:
	return "Article Pager Mode";
      case UHELP_ART:
	return "Article Display/Selection Mode";
      case UHELP_NG:
	return "Newsgroup Browse Mode";
      case UHELP_NGSEL:
	return "Newsgroup Selector";
      case UHELP_ADDSEL:
	return "Add-Newsgroup Selector";
      case UHELP_SUBS:
	return "Escape Substitutions";
      case UHELP_ARTSEL:
	return "Thread/Subject/Article Selector";
      case UHELP_MULTIRC:
	return "Newsrc Selector";
      case UHELP_OPTIONS:
	return "Option Selector";
      case UHELP_SCANART:
	return "Article Scan Mode";
      case UHELP_UNIV:
	return "Universal Selector";
      default:
	return nullptr;
    }
}
