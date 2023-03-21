/* opt.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "init.h"
#include "env.h"
#include "util.h"
#include "util2.h"
#include "intrp.h"
#include "final.h"
#include "art.h"
#include "cache.h"
#include "head.h"
#include "ng.h"
#include "mime.h"
#include "only.h"
#include "ngdata.h"
#include "rt-select.h"
#include "univ.h"
#include "rt-page.h"
#include "charsubst.h"
#include "term.h"
#include "sw.h"
#include "scan.h"
#include "scanart.h"
#include "scorefile.h"
#include "color.h"
#include "opt.h"

#ifdef MSDOS
#include <direct.h>
#include <io.h>
#endif

COMPEX g_optcompex;
char *g_ini_file{};
char *g_yesorno[2] = {"no", "yes"};
INI_WORDS g_options_ini[] = {
    { 0, "OPTIONS", 0 },

    { 0, "*Display Options", 0 },
    { 0, "Terse Output", "yes/no" },
    { 0, "Pager Line-Marking", "standout/underline/no" },
    { 0, "Erase Screen", "yes/no" },
    { 0, "Erase Each Line", "yes/no" },
    { 0, "Muck Up Clear", "yes/no" },
    { 0, "Background Spinner", "yes/no" },
    { 0, "Charset", "<e.g. patm>" },
    { 0, "Filter Control Characters", "yes/no" },

    { 0, "*Selector Options", 0 },
    { 0, "Use Universal Selector", "yes/no" },
    { 0, "Universal Selector Order", "natural/points" },
    { 0, "Universal Selector article follow", "yes/no" },
    { 0, "Universal Selector Commands", "<Last-page-cmd><Other-page-cmd>" },
    { 0, "Use Newsrc Selector", "yes/no" },
    { 0, "Newsrc Selector Commands", "<Last-page-cmd><Other-page-cmd>" },
    { 0, "Use Addgroup Selector", "yes/no" },
    { 0, "Addgroup Selector Commands", "<Last-page-cmd><Other-page-cmd>" },
    { 0, "Use Newsgroup Selector", "yes/no" },
    { 0, "Newsgroup Selector Order", "natural/group/count" },
    { 0, "Newsgroup Selector Commands", "<Last-page-cmd><Other-page-cmd>" },
    { 0, "Newsgroup Selector Display Styles", "<e.g. slm=short/long/med>" },
    { 0, "Use News Selector", "yes/no/<# articles>" },
    { 0, "News Selector Mode", "threads/subjects/articles" },
    { 0, "News Selector Order", "[reverse] date/subject/author/groups/cnt/points" },
    { 0, "News Selector Commands", "<Last-page-cmd><Other-page-cmd>" },
    { 0, "News Selector Display Styles", "<e.g. lms=long/medium/short>" },
    { 0, "Option Selector Commands", "<Last-page-cmd><Other-page-cmd>" },
    { 0, "Use Selector Numbers", "yes/no" },
    { 0, "Selector Number Auto-Goto", "yes/no" },

    { 0, "*Newsreading Options", 0 },
    { 0, "Use Threads", "yes/no" },
    { 0, "Select My Postings", "subthread/parent/thread/no" },
    { 0, "Initial Article Lines", "no/<# lines>" },
    { 0, "Article Tree Lines", "no/<# lines>" },
    { 0, "Word-Wrap Margin", "no/<# chars in margin>" },
    { 0, "Auto-Grow Groups", "yes/no" },
    { 0, "Compress Subjects", "yes/no" },
    { 0, "Join Subject Lines", "no/<# chars>" },
    { 0, "Line Num for Goto", "<# line (1-n)>" },
    { 0, "Ignore THRU on Select", "yes/no" },
    { 0, "Read Breadth First", "yes/no" },
    { 0, "Background Threading", "yes/no" },
    { 0, "Scan Mode Count", "no/<# articles>" },
    { 0, "Header Magic", "<[!]header,...>" },
    { 0, "Header Hiding", "<[!]header,...>" },

    { 0, "*Posting Options", 0 },
    { 0, "Cited Text String", "<e.g. '>'>" },
#if 0
    { 0, "Attribute string", "<e.g. ...>" },
    { 0, "Reply To", "<e.g. ...>" },
#endif

    { 0, "*Save Options", 0 },
    { 0, "Save Dir", "<directory path>" },
    { 0, "Auto Savename", "yes/no" },
    { 0, "Default Savefile Type", "norm/mail/ask" },

    { 0, "*Mouse Options", 0 },
    { 0, "Use XTerm Mouse", "yes/no" },
    { 0, "Mouse Modes", "<e.g. acjlptwK>" },
    { 0, "Universal Selector Mousebar", "<e.g. [PgUp]< [PgDn]> Z [Quit]q>" },
    { 0, "Newsrc Selector Mousebar", "<e.g. [PgUp]< [PgDn]> Z [Quit]q>" },
    { 0, "Addgroup Selector Mousebar", "<e.g. [Top]^ [Bot]$ [ OK ]Z>" },
    { 0, "Newsgroup Selector Mousebar", "<e.g. [ OK ]Z [Quit]q [Help]?>" },
    { 0, "News Selector Mousebar", "<e.g. [KillPg]D [Read]^j [Quit]Q>" },
    { 0, "Option Selector Mousebar", "<e.g. [Save]S [Use]^I [Abandon]q>" },
    { 0, "Article Pager Mousebar", "<e.g. [Next]n J [Sel]+ [Quit]q>" },

    { 0, "*MIME Options", 0 },
    { 0, "Multipart Separator", "<string>" },
    { 0, "Auto-View Inline", "yes/no" },

    { 0, "*Misc Options", 0 },
    { 0, "Check for New Groups", "yes/no" },
    { 0, "Restriction Includes Empty Groups", "yes/no" },
    { 0, "Append Unsubscribed Groups", "yes/no" },
    { 0, "Initial Group List", "no/<# groups>" },
    { 0, "Restart At Last Group", "yes/no" },
    { 0, "Eat Type-Ahead", "yes/no" },
    { 0, "Verify Input", "yes/no" },
    { 0, "Fuzzy Newsgroup Names", "yes/no" },
    { 0, "Auto Arrow Macros", "regular/alternate/no" },
    { 0, "Checkpoint Newsrc Frequency", "<# articles>" },
    { 0, "Default Refetch Time", "never/<1 day 5 hours 8 mins>" },
    { 0, "Novice Delays", "yes/no" },
    { 0, "Old Mthreads Database", "yes/no" },

    { 0, "*Article Scan Mode Options", 0 },
    { 0, "Follow Threads", "yes/no" },
    { 0, "Fold Subjects", "yes/no" },
    { 0, "Re-fold Subjects", "yes/no" },
    { 0, "Mark Without Moving", "yes/no" },
    { 0, "VI Key Movement Allowed", "yes/no" },
    { 0, "Display Item Numbers", "yes/no" },
    { 0, "Display Article Number", "yes/no" },
    { 0, "Display Author", "yes/no" },
    { 0, "Display Score", "yes/no" },
    { 0, "Display Subject Count", "yes/no" },
    { 0, "Display Subject", "yes/no" },
    { 0, "Display Summary", "yes/no" },
    { 0, "Display Keywords", "yes/no" },

    { 0, "*Scoring Options", 0 },
    { 0, "Verbose scoring", "yes/no" },

    { 0, 0, 0 }
};
char **g_option_def_vals{};
char **g_option_saved_vals{};
char *g_option_flags{};
int g_sel_page_op{};
int g_sel_next_op{};

static char* hidden_list();
static char* magic_list();
static void set_header_list(int flag, int defflag, char *str);
static int parse_mouse_buttons(char **cpp, const char *btns);
static char *expand_mouse_buttons(char *cp, int cnt);

void opt_init(int argc, char *argv[], char **tcbufptr)
{
    int i;
    char* s;

    g_sel_grp_dmode = savestr(g_sel_grp_dmode) + 1;
    g_sel_art_dmode = savestr(g_sel_art_dmode) + 1;
    UnivSelBtnCnt = parse_mouse_buttons(&UnivSelBtns,
                                        "[Top]^ [PgUp]< [PgDn]> [ OK ]^j [Quit]q [Help]?");
    NewsrcSelBtnCnt = parse_mouse_buttons(&NewsrcSelBtns,
                                          "[Top]^ [PgUp]< [PgDn]> [ OK ]^j [Quit]q [Help]?");
    AddSelBtnCnt = parse_mouse_buttons(&AddSelBtns,
                                       "[Top]^ [Bot]$ [PgUp]< [PgDn]> [ OK ]Z [Quit]q [Help]?");
    OptionSelBtnCnt = parse_mouse_buttons(&OptionSelBtns,
                                          "[Find]/ [FindNext]/^j [Top]^ [Bot]$ [PgUp]< [PgDn]> [Use]^i [Save]S [Abandon]q [Help]?");
    NewsgroupSelBtnCnt = parse_mouse_buttons(&NewsgroupSelBtns,
                                             "[Top]^ [PgUp]< [PgDn]> [ OK ]Z [Quit]q [Help]?");
    NewsSelBtnCnt = parse_mouse_buttons(&NewsSelBtns,
                                        "[Top]^ [Bot]$ [PgUp]< [PgDn]> [KillPg]D [ OK ]Z [Quit]q [Help]?");
    ArtPagerBtnCnt = parse_mouse_buttons(&ArtPagerBtns,
                                         "[Next]n [Sel]+ [Quit]q [Help]h");

    prep_ini_words(g_options_ini);
    if (argc >= 2 && !strcmp(argv[1],"-c"))
	checkflag=true;			/* so we can optimize for -c */
    interp(*tcbufptr,TCBUF_SIZE,GLOBINIT);
    opt_file(*tcbufptr,tcbufptr,false);

    g_option_def_vals = (char**)safemalloc(INI_LEN(g_options_ini)*sizeof(char*));
    memset((char*)g_option_def_vals,0,(g_options_ini)[0].checksum * sizeof (char*));
    /* Set DEFHIDE and DEFMAGIC to current values and clear g_user_htype list */
    set_header_list(HT_DEFHIDE,HT_HIDE,"");
    set_header_list(HT_DEFMAGIC,HT_MAGIC,"");

    if (use_threads)
	s = get_val("TRNRC","%+/trnrc");
    else
	s = get_val("RNRC","%+/rnrc");
    g_ini_file = savestr(filexp(s));

    s = filexp("%+");
    if (stat(s,&filestat) < 0 || !S_ISDIR(filestat.st_mode)) {
	printf("Creating the directory %s.\n",s);
	if (makedir(s,MD_DIR) != 0) {
	    printf("Unable to create `%s'.\n",s);
	    finalize(1); /*$$??*/
	}
    }
    if (stat(g_ini_file,&filestat) == 0)
	opt_file(g_ini_file,tcbufptr,true);
    if (!use_threads || (s = getenv("TRNINIT")) == nullptr)
	s = getenv("RNINIT");
    if (*safecpy(*tcbufptr,s,TCBUF_SIZE)) {
	if (*s == '-' || *s == '+' || isspace(*s))
	    sw_list(*tcbufptr);
	else
	    sw_file(tcbufptr);
    }
    g_option_saved_vals = (char**)safemalloc(INI_LEN(g_options_ini)*sizeof(char*));
    memset((char*)g_option_saved_vals,0,(g_options_ini)[0].checksum * sizeof (char*));
    g_option_flags = (char*)safemalloc(INI_LEN(g_options_ini)*sizeof(char));
    memset(g_option_flags,0,(g_options_ini)[0].checksum * sizeof (char));

    if (argc > 1) {
	for (i = 1; i < argc; i++)
	    decode_switch(argv[i]);
    }
    init_compex(&g_optcompex);
}

void opt_file(char *filename, char **tcbufptr, bool bleat)
{
    char* filebuf = *tcbufptr;
    char* s;
    char* section;
    char* cond;
    int fd = open(filename,0);
	
    if (fd >= 0) {
	fstat(fd,&filestat);
	if (filestat.st_size >= TCBUF_SIZE-1) {
	    filebuf = saferealloc(filebuf,(MEM_SIZE)filestat.st_size+2);
	    *tcbufptr = filebuf;
	}
	if (filestat.st_size) {
	    int len = read(fd,filebuf,(int)filestat.st_size);
	    filebuf[len] = '\0';
	    prep_ini_data(filebuf,filename);
	    s = filebuf;
	    while ((s = next_ini_section(s,&section,&cond)) != nullptr) {
		if (*cond && !check_ini_cond(cond))
		    continue;
		if (!strcmp(section,"options")) {
		    s = parse_ini_section(s, g_options_ini);
		    if (!s)
			break;
		    set_options(INI_VALUES(g_options_ini));
		}
		else if (!strcmp(section,"environment")) {
		    while (*s && *s != '[') {
			section = s;
			s += strlen(s) + 1;
			export_var(section,s);
			s += strlen(s) + 1;
		    }
		}
		else if (!strcmp(section,"termcap")) {
		    while (*s && *s != '[') {
			section = s;
			s += strlen(s) + 1;
			add_tc_string(section, s);
			s += strlen(s) + 1;
		    }
		}
		else if (!strcmp(section,"attribute")) {
		    while (*s && *s != '[') {
			section = s;
			s += strlen(s) + 1;
			color_rc_attribute(section, s);
			s += strlen(s) + 1;
		    }
		}
	    }
	}
	close(fd);
    }
    else if (bleat) {
	printf(cantopen,filename) FLUSH;
	/*termdown(1);*/
    }

    *filebuf = '\0';
}

#define YES(s) (*(s) == 'y' || *(s) == 'Y')
#define NO(s)  (*(s) == 'n' || *(s) == 'N')

void set_options(char **vals)
{
    int limit = INI_LEN(g_options_ini);
    int i;
    for (i = 1; i < limit; i++) {
	if (*++vals)
	    set_option(i, *vals);
    }
}

void set_option(int num, char *s)
{
    if (g_option_saved_vals) {
	if (!g_option_saved_vals[num]) {
	    g_option_saved_vals[num] = savestr(option_value(num));
	    if (!g_option_def_vals[num])
		g_option_def_vals[num] = g_option_saved_vals[num];
	}
    }
    else if (g_option_def_vals) {
	if (!g_option_def_vals[num])
	    g_option_def_vals[num] = savestr(option_value(num));
    }
    switch (num) {
      case OI_USE_THREADS:
	use_threads = YES(s);
	break;
      case OI_USE_MOUSE:
	UseMouse = YES(s);
	if (UseMouse) {
	    /* set up the Xterm mouse sequence */
	    set_macro("\033[M+3","\003");
	}
	break;
      case OI_MOUSE_MODES:
	safecpy(MouseModes, s, sizeof MouseModes);
	break;
      case OI_USE_UNIV_SEL:
	UseUnivSelector = YES(s);
	break;
      case OI_UNIV_SEL_CMDS:
	*UnivSelCmds = *s;
	if (s[1])
	    UnivSelCmds[1] = s[1];
	break;
      case OI_UNIV_SEL_BTNS:
	UnivSelBtnCnt = parse_mouse_buttons(&UnivSelBtns,s);
	break;
      case OI_UNIV_SEL_ORDER:
	set_sel_order(SM_UNIVERSAL,s);
	break;
      case OI_UNIV_FOLLOW:
	univ_follow = YES(s);
	break;
      case OI_USE_NEWSRC_SEL:
	UseNewsrcSelector = YES(s);
	break;
      case OI_NEWSRC_SEL_CMDS:
	*NewsrcSelCmds = *s;
	if (s[1])
	    NewsrcSelCmds[1] = s[1];
	break;
      case OI_NEWSRC_SEL_BTNS:
	NewsrcSelBtnCnt = parse_mouse_buttons(&NewsrcSelBtns,s);
	break;
      case OI_USE_ADD_SEL:
	UseAddSelector = YES(s);
	break;
      case OI_ADD_SEL_CMDS:
	*AddSelCmds = *s;
	if (s[1])
	    AddSelCmds[1] = s[1];
	break;
      case OI_ADD_SEL_BTNS:
	AddSelBtnCnt = parse_mouse_buttons(&AddSelBtns,s);
	break;
      case OI_USE_NEWSGROUP_SEL:
	UseNewsgroupSelector = YES(s);
	break;
      case OI_NEWSGROUP_SEL_ORDER:
	set_sel_order(SM_NEWSGROUP,s);
	break;
      case OI_NEWSGROUP_SEL_CMDS:
	*NewsgroupSelCmds = *s;
	if (s[1])
	    NewsgroupSelCmds[1] = s[1];
	break;
      case OI_NEWSGROUP_SEL_BTNS:
	NewsgroupSelBtnCnt = parse_mouse_buttons(&NewsgroupSelBtns,s);
	break;
      case OI_NEWSGROUP_SEL_STYLES:
	free(option_value(OI_NEWSGROUP_SEL_STYLES)-1);
	g_sel_grp_dmode = safemalloc(strlen(s)+2);
	*g_sel_grp_dmode++ = '*';
	strcpy(g_sel_grp_dmode, s);
	break;
      case OI_USE_NEWS_SEL:
	if (isdigit(*s))
	    UseNewsSelector = atoi(s);
	else
	    UseNewsSelector = YES(s)-1;
	break;
      case OI_NEWS_SEL_MODE: {
	int save_sel_mode = g_sel_mode;
	set_sel_mode(*s);
	if (save_sel_mode != SM_ARTICLE && save_sel_mode != SM_SUBJECT
	 && save_sel_mode != SM_THREAD) {
	    g_sel_mode = save_sel_mode;
	    set_selector(0,0);
	}
	break;
      }
      case OI_NEWS_SEL_ORDER:
	set_sel_order(g_sel_defaultmode,s);
	break;
      case OI_NEWS_SEL_CMDS:
	*NewsSelCmds = *s;
	if (s[1])
	    NewsSelCmds[1] = s[1];
	break;
      case OI_NEWS_SEL_BTNS:
	NewsSelBtnCnt = parse_mouse_buttons(&NewsSelBtns,s);
	break;
      case OI_NEWS_SEL_STYLES:
	free(option_value(OI_NEWS_SEL_STYLES)-1);
	g_sel_art_dmode = safemalloc(strlen(s)+2);
	*g_sel_art_dmode++ = '*';
	strcpy(g_sel_art_dmode, s);
	break;
      case OI_OPTION_SEL_CMDS:
	*OptionSelCmds = *s;
	if (s[1])
	    OptionSelCmds[1] = s[1];
	break;
      case OI_OPTION_SEL_BTNS:
	OptionSelBtnCnt = parse_mouse_buttons(&OptionSelBtns,s);
	break;
      case OI_AUTO_SAVE_NAME:
	if (!checkflag) {
	    if (YES(s)) {
		export_var("SAVEDIR",  "%p/%c");
		export_var("SAVENAME", "%a");
	    }
	    else if (!strcmp(get_val("SAVEDIR",""),"%p/%c")
		  && !strcmp(get_val("SAVENAME",""),"%a")) {
		export_var("SAVEDIR", "%p");
		export_var("SAVENAME", "%^C");
	    }
	}
	break;
      case OI_BKGND_THREADING:
	thread_always = !YES(s);
	break;
      case OI_AUTO_ARROW_MACROS: {
	int prev = auto_arrow_macros;
	if (YES(s) || *s == 'r' || *s == 'R')
	    auto_arrow_macros = 2;
	else
	    auto_arrow_macros = !NO(s);
	if (mode != 'i' && auto_arrow_macros != prev) {
	    char tmpbuf[1024];
	    arrow_macros(tmpbuf);
	}
	break;
      }
      case OI_READ_BREADTH_FIRST:
	breadth_first = YES(s);
	break;
      case OI_BKGND_SPINNER:
	bkgnd_spinner = YES(s);
	break;
      case OI_CHECKPOINT_NEWSRC_FREQUENCY:
	g_docheckwhen = atoi(s);
	break;
      case OI_SAVE_DIR:
	if (!checkflag) {
	    savedir = savestr(s);
	    if (cwd) {
		chdir(cwd);
		free(cwd);
	    }
	    cwd = savestr(filexp(s));
	}
	break;
      case OI_ERASE_SCREEN:
	erase_screen = YES(s);
	break;
      case OI_NOVICE_DELAYS:
	novice_delays = YES(s);
	break;
      case OI_CITED_TEXT_STRING:
	indstr = savestr(s);
	break;
      case OI_GOTO_LINE_NUM:
	g_gline = atoi(s)-1;
	break;
      case OI_FUZZY_NEWSGROUP_NAMES:
	fuzzyGet = YES(s);
	break;
      case OI_HEADER_MAGIC:
	if (!checkflag)
	    set_header_list(HT_MAGIC, HT_DEFMAGIC, s);
	break;
      case OI_HEADER_HIDING:
	set_header_list(HT_HIDE, HT_DEFHIDE, s);
	break;
      case OI_INITIAL_ARTICLE_LINES:
	initlines = atoi(s);
	break;
      case OI_APPEND_UNSUBSCRIBED_GROUPS:
	append_unsub = YES(s);
	break;
      case OI_FILTER_CONTROL_CHARACTERS:
	dont_filter_control = !YES(s);
	break;
      case OI_JOIN_SUBJECT_LINES:
	if (isdigit(*s))
	    change_join_subject_len(atoi(s));
	else
	    change_join_subject_len(YES(s)? 30 : 0);
	break;
      case OI_IGNORE_THRU_ON_SELECT:
	kill_thru_kludge = YES(s);
	break;
      case OI_AUTO_GROW_GROUPS:
	keep_the_group_static = !YES(s);
	break;
      case OI_MUCK_UP_CLEAR:
	muck_up_clear = YES(s);
	break;
      case OI_ERASE_EACH_LINE:
	erase_each_line = YES(s);
	break;
      case OI_SAVEFILE_TYPE:
	mbox_always = (*s == 'm' || *s == 'M');
	norm_always = (*s == 'n' || *s == 'N');
	break;
      case OI_PAGER_LINE_MARKING:
	if (isdigit(*s))
	    marking_areas = atoi(s);
	else
	    marking_areas = HALFPAGE_MARKING;
	if (NO(s))
	    marking = NOMARKING;
	else if (*s == 'u')
	    marking = UNDERLINE;
	else
	    marking = STANDOUT;
	break;
      case OI_OLD_MTHREADS_DATABASE:
	if (isdigit(*s))
	    olden_days = atoi(s);
	else
	    olden_days = YES(s);
	break;
      case OI_SELECT_MY_POSTS:
	if (NO(s))
	    auto_select_postings = 0;
	else {
	    switch (*s) {
	      case 't':
		auto_select_postings = '+';
		break;
	      case 'p':
		auto_select_postings = 'p';
		break;
	      default:
		auto_select_postings = '.';
		break;
	    }
	}
	break;
      case OI_MULTIPART_SEPARATOR:
	g_multipart_separator = savestr(s);
	break;
      case OI_AUTO_VIEW_INLINE:
	g_auto_view_inline = YES(s);
	break;
      case OI_NEWGROUP_CHECK:
	quickstart = !YES(s);
	break;
      case OI_RESTRICTION_INCLUDES_EMPTIES:
	g_empty_only_char = YES(s)? 'o' : 'O';
	break;
      case OI_CHARSET:
	g_charsets = savestr(s);
	break;
      case OI_INITIAL_GROUP_LIST:
	if (isdigit(*s)) {
	    countdown = atoi(s);
	    suppress_cn = (countdown == 0);
	}
	else {
	    suppress_cn = NO(s);
	    if (!suppress_cn)
		countdown = 5;
	}
	break;
      case OI_RESTART_AT_LAST_GROUP:
	findlast = YES(s) * (mode == 'i'? 1 : -1);
	break;
      case OI_SCANMODE_COUNT:
	if (isdigit(*s))
	    scanon = atoi(s);
	else
	    scanon = YES(s)*3;
	break;
      case OI_TERSE_OUTPUT:
	verbose = !YES(s);
	if (!verbose)
	    novice_delays = false;
	break;
      case OI_EAT_TYPEAHEAD:
	allow_typeahead = !YES(s);
	break;
      case OI_COMPRESS_SUBJECTS:
	unbroken_subjects = !YES(s);
	break;
      case OI_VERIFY_INPUT:
	verify = YES(s);
	break;
      case OI_ARTICLE_TREE_LINES:
	if (isdigit(*s)) {
	    if ((max_tree_lines = atoi(s)) > 11)
		max_tree_lines = 11;
	} else
	    max_tree_lines = YES(s) * 6;
	break;
      case OI_WORD_WRAP_MARGIN:
	if (isdigit(*s))
	    word_wrap_offset = atoi(s);
	else if (YES(s))
	    word_wrap_offset = 8;
	else
	    word_wrap_offset = -1;
	break;
      case OI_DEFAULT_REFETCH_TIME:
	defRefetchSecs = text2secs(s, DEFAULT_REFETCH_SECS);
	break;
      case OI_ART_PAGER_BTNS:
	ArtPagerBtnCnt = parse_mouse_buttons(&ArtPagerBtns,s);
	break;
      case OI_SCAN_ITEMNUM:
	s_itemnum = YES(s);
	break;
      case OI_SCAN_VI:
	s_mode_vi = YES(s);
	break;
      case OI_SCANA_FOLLOW:
	sa_follow = YES(s);
	break;
      case OI_SCANA_FOLD:
	sa_mode_fold = YES(s);
	break;
      case OI_SCANA_UNZOOMFOLD:
	sa_unzoomrefold = YES(s);
	break;
      case OI_SCANA_MARKSTAY:
	sa_mark_stay = YES(s);
	break;
      case OI_SCANA_DISPANUM:
	sa_mode_desc_artnum = YES(s);
	break;
      case OI_SCANA_DISPAUTHOR:
	sa_mode_desc_author = YES(s);
	break;
      case OI_SCANA_DISPSCORE:
	sa_mode_desc_score = YES(s);
	break;
      case OI_SCANA_DISPSUBCNT:
	sa_mode_desc_threadcount = YES(s);
	break;
      case OI_SCANA_DISPSUBJ:
#if 0
	/* CAA: for now, always on. */
	sa_mode_desc_subject = YES(s);
#endif
	break;
      case OI_SCANA_DISPSUMMARY:
	sa_mode_desc_summary = YES(s);
	break;
      case OI_SCANA_DISPKEYW:
	sa_mode_desc_keyw = YES(s);
	break;
      case OI_SC_VERBOSE:
	sf_verbose = YES(s);
	break;
      case OI_USE_SEL_NUM:
	UseSelNum = YES(s);
	break;
      case OI_SEL_NUM_GOTO:
	SelNumGoto = YES(s);
	break;
      default:
	printf("*** Internal error: Unknown Option ***\n");
	break;
    }
}

void save_options(const char *filename)
{
    int i;
    int fd_in;
    FILE* fp_out;
    char* filebuf = nullptr;
    char* line = nullptr;
    static bool first_time = true;

    sprintf(buf,"%s.new",filename);
    fp_out = fopen(buf,"w");
    if (!fp_out) {
	printf(cantcreate,buf);
	return;
    }
    if ((fd_in = open(filename,0)) >= 0) {
	char* cp;
	char* nlp = nullptr;
	char* comments = nullptr;
	fstat(fd_in,&filestat);
	if (filestat.st_size) {
	    int len;
	    filebuf = safemalloc((MEM_SIZE)filestat.st_size+2);
	    len = read(fd_in,filebuf,(int)filestat.st_size);
	    filebuf[len] = '\0';
	}
	close(fd_in);

	for (line = filebuf; line && *line; line = nlp) {
	    cp = line;
	    nlp = strchr(cp, '\n');
	    if (nlp)
		*nlp++ = '\0';
	    while (isspace(*cp)) cp++;
	    if (*cp == '[' && !strncmp(cp+1,"options]",8)) {
		for (cp += 9; isspace(*cp); cp++) ;
		if (!*cp)
		    break;
	    }
	    fputs(line, fp_out);
	    fputc('\n', fp_out);
	}
	for (line = nlp; line && *line; line = nlp) {
	    cp = line;
	    nlp = strchr(cp, '\n');
	    if (nlp)
		nlp++;
	    while (*cp != '\n' && isspace(*cp)) cp++;
	    if (*cp == '[')
		break;
	    if (isalpha(*cp))
		comments = nullptr;
	    else if (!comments)
		comments = line;
	}
	if (comments)
	    line = comments;
    }
    else {
	char *t = use_threads? "T" : "";
	printf("\n\
This is the first save of the option file, %s.\n\
By default this file overrides your %sRNINIT variable, but if you\n\
want to continue to use an old-style init file (that overrides the\n\
settings in the option file), edit the option file and change the\n\
line that sets %sRNINIT.\n", g_ini_file, t, t);
	get_anything();
	fprintf(fp_out, "# trnrc file auto-generated\n[environment]\n");
	write_init_environment(fp_out);
	fprintf(fp_out, "%sRNINIT = ''\n\n", t);
    }
    fprintf(fp_out,"[options]\n");
    for (i = 1; g_options_ini[i].checksum; i++) {
	if (*g_options_ini[i].item == '*')
	    fprintf(fp_out,"# ==%s========\n",g_options_ini[i].item+1);
	else {
	    fprintf(fp_out,"%s = ",g_options_ini[i].item);
	    if (!g_option_def_vals[i])
		fputs("#default of ",fp_out);
	    fprintf(fp_out,"%s\n",quote_string(option_value(i)));
	    if (g_option_saved_vals[i]) {
		if (g_option_saved_vals[i] != g_option_def_vals[i])
		    free(g_option_saved_vals[i]);
		g_option_saved_vals[i] = nullptr;
	    }
	}
    }
    if (line) {
	/*putc('\n',fp_out);*/
	fputs(line,fp_out);
    }
    fclose(fp_out);

    safefree(filebuf);

    if (first_time) {
	if (fd_in >= 0) {
	    sprintf(buf,"%s.old",filename);
	    remove(buf);
	    rename(filename,buf);
	}
	first_time = false;
    }
    else
	remove(filename);

    sprintf(buf,"%s.new",filename);
    rename(buf,filename);
}

char *option_value(int num)
{
    switch (num) {
      case OI_USE_THREADS:
	return YESorNO(use_threads);
      case OI_USE_MOUSE:
	return YESorNO(UseMouse);
      case OI_MOUSE_MODES:
	return MouseModes;
      case OI_USE_UNIV_SEL:
	return YESorNO(UseUnivSelector);
      case OI_UNIV_SEL_CMDS:
	return UnivSelCmds;
      case OI_UNIV_SEL_BTNS:
	return expand_mouse_buttons(UnivSelBtns,UnivSelBtnCnt);
      case OI_UNIV_SEL_ORDER:
	return get_sel_order(SM_UNIVERSAL);
      case OI_UNIV_FOLLOW:
	return YESorNO(univ_follow);
	break;
      case OI_USE_NEWSRC_SEL:
	return YESorNO(UseNewsrcSelector);
      case OI_NEWSRC_SEL_CMDS:
	return NewsrcSelCmds;
      case OI_NEWSRC_SEL_BTNS:
	return expand_mouse_buttons(NewsrcSelBtns,NewsrcSelBtnCnt);
      case OI_USE_ADD_SEL:
	return YESorNO(UseAddSelector);
      case OI_ADD_SEL_CMDS:
	return AddSelCmds;
      case OI_ADD_SEL_BTNS:
	return expand_mouse_buttons(AddSelBtns,AddSelBtnCnt);
      case OI_USE_NEWSGROUP_SEL:
	return YESorNO(UseNewsgroupSelector);
      case OI_NEWSGROUP_SEL_ORDER:
	return get_sel_order(SM_NEWSGROUP);
      case OI_NEWSGROUP_SEL_CMDS:
	return NewsgroupSelCmds;
      case OI_NEWSGROUP_SEL_BTNS:
	return expand_mouse_buttons(NewsgroupSelBtns,NewsgroupSelBtnCnt);
      case OI_NEWSGROUP_SEL_STYLES: {
	char* s = g_sel_grp_dmode;
	while (s[-1] != '*') s--;
	return s;
      }
      case OI_USE_NEWS_SEL:
	if (UseNewsSelector < 1)
	    return YESorNO(UseNewsSelector+1);
	sprintf(buf,"%d",UseNewsSelector);
	return buf;
      case OI_NEWS_SEL_MODE: {
	int save_sel_mode = g_sel_mode;
	int save_Threaded = g_threaded_group;
	char* s;
	g_threaded_group = true;
	set_selector(g_sel_defaultmode, 0);
	s = g_sel_mode_string;
	g_sel_mode = save_sel_mode;
	g_threaded_group = save_Threaded;
	set_selector(0, 0);
	return s;
      }
      case OI_NEWS_SEL_ORDER:
	return get_sel_order(g_sel_defaultmode);
      case OI_NEWS_SEL_CMDS:
	return NewsSelCmds;
      case OI_NEWS_SEL_BTNS:
	return expand_mouse_buttons(NewsSelBtns,NewsSelBtnCnt);
      case OI_NEWS_SEL_STYLES: {
	char* s = g_sel_art_dmode;
	while (s[-1] != '*') s--;
	return s;
      }
      case OI_OPTION_SEL_CMDS:
	return OptionSelCmds;
      case OI_OPTION_SEL_BTNS:
	return expand_mouse_buttons(OptionSelBtns,OptionSelBtnCnt);
      case OI_AUTO_SAVE_NAME:
	return YESorNO(!strcmp(get_val("SAVEDIR",SAVEDIR),"%p/%c"));
      case OI_BKGND_THREADING:
	return YESorNO(!thread_always);
      case OI_AUTO_ARROW_MACROS:
	switch (auto_arrow_macros) {
	  case 2:
	    return "regular";
	  case 1:
	    return "alternate";
	  default:
	    return YESorNO(0);
	}
      case OI_READ_BREADTH_FIRST:
	return YESorNO(breadth_first);
      case OI_BKGND_SPINNER:
	return YESorNO(bkgnd_spinner);
      case OI_CHECKPOINT_NEWSRC_FREQUENCY:
	sprintf(buf,"%d",g_docheckwhen);
	return buf;
      case OI_SAVE_DIR:
	return savedir? savedir : "%./News";
      case OI_ERASE_SCREEN:
	return YESorNO(erase_screen);
      case OI_NOVICE_DELAYS:
	return YESorNO(novice_delays);
      case OI_CITED_TEXT_STRING:
	return indstr;
      case OI_GOTO_LINE_NUM:
	sprintf(buf,"%d",g_gline+1);
	return buf;
      case OI_FUZZY_NEWSGROUP_NAMES:
	return YESorNO(fuzzyGet);
      case OI_HEADER_MAGIC:
	return magic_list();
      case OI_HEADER_HIDING:
	return hidden_list();
      case OI_INITIAL_ARTICLE_LINES:
	if (!g_option_def_vals[OI_INITIAL_ARTICLE_LINES])
	    return "$LINES";
	sprintf(buf,"%d",initlines);
    	return buf;
      case OI_APPEND_UNSUBSCRIBED_GROUPS:
	return YESorNO(append_unsub);
      case OI_FILTER_CONTROL_CHARACTERS:
	return YESorNO(!dont_filter_control);
      case OI_JOIN_SUBJECT_LINES:
	if (join_subject_len) {
	    sprintf(buf,"%d",join_subject_len);
	    return buf;
	}
	return YESorNO(0);
      case OI_IGNORE_THRU_ON_SELECT:
	return YESorNO(kill_thru_kludge);
      case OI_AUTO_GROW_GROUPS:
	return YESorNO(!keep_the_group_static);
      case OI_MUCK_UP_CLEAR:
	return YESorNO(muck_up_clear);
      case OI_ERASE_EACH_LINE:
	return YESorNO(erase_each_line);
      case OI_SAVEFILE_TYPE:
	return mbox_always? "mail" : (norm_always? "norm" : "ask");
      case OI_PAGER_LINE_MARKING:
	if (marking == NOMARKING)
	    return YESorNO(0);
	if (marking_areas != HALFPAGE_MARKING)
	    sprintf(buf,"%d",marking_areas);
	else
	    *buf = '\0';
	if (marking == UNDERLINE)
	    strcat(buf,"underline");
	else
	    strcat(buf,"standout");
	return buf;
      case OI_OLD_MTHREADS_DATABASE:
	if (olden_days <= 1)
	    return YESorNO(olden_days);
	sprintf(buf,"%d",olden_days);
	return buf;
      case OI_SELECT_MY_POSTS:
	switch (auto_select_postings) {
	  case '+':
	    return "thread";
	  case 'p':
	    return "parent";
	  case '.':
	    return "subthread";
	  default:
	    break;
	}
	return YESorNO(0);
      case OI_MULTIPART_SEPARATOR:
	return g_multipart_separator;
      case OI_AUTO_VIEW_INLINE:
	return YESorNO(g_auto_view_inline);
      case OI_NEWGROUP_CHECK:
	return YESorNO(!quickstart);
      case OI_RESTRICTION_INCLUDES_EMPTIES:
	return YESorNO(g_empty_only_char == 'o');
      case OI_CHARSET:
	return g_charsets;
      case OI_INITIAL_GROUP_LIST:
	if (suppress_cn)
	    return YESorNO(0);
	sprintf(buf,"%d",countdown);
	return buf;
      case OI_RESTART_AT_LAST_GROUP:
	return YESorNO(findlast != 0);
      case OI_SCANMODE_COUNT:
	sprintf(buf,"%d",scanon);
	return buf;
      case OI_TERSE_OUTPUT:
	return YESorNO(!verbose);
      case OI_EAT_TYPEAHEAD:
	return YESorNO(!allow_typeahead);
      case OI_COMPRESS_SUBJECTS:
	return YESorNO(!unbroken_subjects);
      case OI_VERIFY_INPUT:
	return YESorNO(verify);
      case OI_ARTICLE_TREE_LINES:
	sprintf(buf,"%d",max_tree_lines);
	return buf;
      case OI_WORD_WRAP_MARGIN:
	if (word_wrap_offset >= 0) {
	    sprintf(buf,"%d",word_wrap_offset);
	    return buf;
	}
	return YESorNO(0);
      case OI_DEFAULT_REFETCH_TIME:
	return secs2text(defRefetchSecs);
      case OI_ART_PAGER_BTNS:
	return expand_mouse_buttons(ArtPagerBtns,ArtPagerBtnCnt);
      case OI_SCAN_ITEMNUM:
	return YESorNO(s_itemnum);
      case OI_SCAN_VI:
	return YESorNO(s_mode_vi);
      case OI_SCANA_FOLLOW:
	return YESorNO(sa_follow);
      case OI_SCANA_FOLD:
	return YESorNO(sa_mode_fold);
      case OI_SCANA_UNZOOMFOLD:
	return YESorNO(sa_unzoomrefold);
      case OI_SCANA_MARKSTAY:
	return YESorNO(sa_mark_stay);
      case OI_SCANA_DISPANUM:
	return YESorNO(sa_mode_desc_artnum);
      case OI_SCANA_DISPAUTHOR:
	return YESorNO(sa_mode_desc_author);
      case OI_SCANA_DISPSCORE:
	return YESorNO(sa_mode_desc_score);
      case OI_SCANA_DISPSUBCNT:
	return YESorNO(sa_mode_desc_threadcount);
      case OI_SCANA_DISPSUBJ:
	return YESorNO(sa_mode_desc_subject);
      case OI_SCANA_DISPSUMMARY:
	return YESorNO(sa_mode_desc_summary);
      case OI_SCANA_DISPKEYW:
	return YESorNO(sa_mode_desc_keyw);
      case OI_SC_VERBOSE:
	return YESorNO(sf_verbose);
      case OI_USE_SEL_NUM:
	return YESorNO(UseSelNum);
	break;
      case OI_SEL_NUM_GOTO:
	return YESorNO(SelNumGoto);
	break;
      default:
	printf("*** Internal error: Unknown Option ***\n");
	break;
    }
    return "<UNKNOWN>";
}

static char *hidden_list()
{
    int i;
    buf[0] = '\0';
    buf[1] = '\0';
    for (i = 1; i < g_user_htype_cnt; i++) {
	sprintf(buf+strlen(buf),",%s%s", g_user_htype[i].flags? "" : "!",
		g_user_htype[i].name);
    }
    return buf+1;
}

static char *magic_list()
{
    int i;
    buf[0] = '\0';
    buf[1] = '\0';
    for (i = HEAD_FIRST; i < HEAD_LAST; i++) {
	if (!(g_htype[i].flags & HT_MAGIC) != !(g_htype[i].flags & HT_DEFMAGIC)) {
	    sprintf(buf+strlen(buf),",%s%s",
		    (g_htype[i].flags & HT_DEFMAGIC)? "!" : "",
		    g_htype[i].name);
	}
    }
    return buf+1;
}

static void set_header_list(int flag, int defflag, char *str)
{
    int i;
    bool setit;
    char* cp;

    if (flag == HT_HIDE || flag == HT_DEFHIDE) {
	/* Free old g_user_htype list */
	while (g_user_htype_cnt > 1)
	    free(g_user_htype[--g_user_htype_cnt].name);
	memset((char*)g_user_htypeix,0,sizeof g_user_htypeix);
    }

    if (!*str)
	str = " ";
    for (i = HEAD_FIRST; i < HEAD_LAST; i++)
	g_htype[i].flags = ((g_htype[i].flags & defflag)
			? (g_htype[i].flags | flag)
			: (g_htype[i].flags & ~flag));
    for (;;) {
	if ((cp = strchr(str,',')) != nullptr)
	    *cp = '\0';
	if (*str == '!') {
	    setit = false;
	    str++;
	}
	else
	    setit = true;
	set_header(str,flag,setit);
	if (!cp)
	    break;
	*cp = ',';
	str = cp+1;
    }
}

void set_header(char *s, int flag, bool setit)
{
    int i;
    int len = strlen(s);
    for (i = HEAD_FIRST; i < HEAD_LAST; i++) {
	if (!len || !strncasecmp(s,g_htype[i].name,len)) {
	    if (setit && (flag != HT_MAGIC || (g_htype[i].flags & HT_MAGICOK)))
		g_htype[i].flags |= flag;
	    else
		g_htype[i].flags &= ~flag;
	}
    }
    if (flag == HT_HIDE && *s && isalpha(*s)) {
	char ch = isupper(*s)? tolower(*s) : *s;
	int add_at = 0, killed = 0;
	bool save_it = true;
	for (i = g_user_htypeix[ch - 'a']; *g_user_htype[i].name == ch; i--) {
	    if (len <= g_user_htype[i].length
	     && !strncasecmp(s,g_user_htype[i].name,len)) {
		free(g_user_htype[i].name);
		g_user_htype[i].name = nullptr;
		killed = i;
	    }
	    else if (len > g_user_htype[i].length
		  && !strncasecmp(s,g_user_htype[i].name,g_user_htype[i].length)) {
		if (!add_at) {
		    if (g_user_htype[i].flags == (setit? flag : 0))
			save_it = false;
		    add_at = i+1;
		}
	    }
	}
	if (save_it) {
	    if (!killed || (add_at && g_user_htype[add_at].name)) {
		if (g_user_htype_cnt >= g_user_htype_max) {
		    g_user_htype = (USER_HEADTYPE*)
			realloc(g_user_htype, (g_user_htype_max += 10)
					    * sizeof (USER_HEADTYPE));
		}
		if (!add_at) {
		    add_at = g_user_htypeix[ch - 'a']+1;
		    if (add_at == 1)
			add_at = g_user_htype_cnt;
		}
		for (i = g_user_htype_cnt; i > add_at; i--)
		    g_user_htype[i] = g_user_htype[i-1];
		g_user_htype_cnt++;
	    }
	    else if (!add_at)
		add_at = killed;
	    g_user_htype[add_at].length = len;
	    g_user_htype[add_at].flags = setit? flag : 0;
	    g_user_htype[add_at].name = savestr(s);
	    for (s = g_user_htype[add_at].name; *s; s++) {
		if (isupper(*s))
		    *s = tolower(*s);
	    }
	}
	if (killed) {
	    while (killed < g_user_htype_cnt && g_user_htype[killed].name != nullptr)
		killed++;
	    for (i = killed+1; i < g_user_htype_cnt; i++) {
		if (g_user_htype[i].name != nullptr)
		    g_user_htype[killed++] = g_user_htype[i];
	    }
	    g_user_htype_cnt = killed;
	}
	memset((char*)g_user_htypeix,0,sizeof g_user_htypeix);
	for (i = 1; i < g_user_htype_cnt; i++)
	    g_user_htypeix[*g_user_htype[i].name - 'a'] = i;
    }
}

static int parse_mouse_buttons(char **cpp, const char *btns)
{
    char* t = *cpp;
    int cnt = 0;

    safefree(t);
    while (*btns == ' ') btns++;
    t = *cpp = safemalloc(strlen(btns)+1);

    while (*btns) {
	if (*btns == '[') {
	    if (!btns[1])
		break;
	    while (*btns) {
		if (*btns == '\\' && btns[1])
		    btns++;
		else if (*btns == ']')
		    break;
		*t++ = *btns++;
	    }
	    *t++ = '\0';
	    if (*btns)
		while (*++btns == ' ') ;
	}
	while (*btns && *btns != ' ') *t++ = *btns++;
	while (*btns == ' ') btns++;
	*t++ = '\0';
	cnt++;
    }

    return cnt;
}

static char *expand_mouse_buttons(char *cp, int cnt)
{
    *buf = '\0';
    while (cnt--) {
	if (*cp == '[') {
	    int len = strlen(cp);
	    cp[len] = ']';
	    safecat(buf,cp,sizeof buf);
	    cp += len;
	    *cp++ = '\0';
	}
	else
	    safecat(buf,cp,sizeof buf);
	cp += strlen(cp)+1;
    }
    return buf;
}

char *quote_string(char *val)
{
    static char* bufptr = nullptr;
    char* cp;
    int needs_quotes = 0;
    int ticks = 0;
    int quotes = 0;
    int backslashes = 0;

    safefree(bufptr);
    bufptr = nullptr;

    if (isspace(*val))
	needs_quotes = 1;
    for (cp = val; *cp; cp++) {
	switch (*cp) {
	  case ' ':
	  case '\t':
	    if (!cp[1] || isspace(cp[1]))
		needs_quotes++;
	    break;
	  case '\n':
	  case '#':
	    needs_quotes++;
	    break;
	  case '\'':
	    ticks++;
	    break;
	  case '"':
	    quotes++;
	    break;
	  case '\\':
	    backslashes++;
	    break;
	}
    }

    if (needs_quotes || ticks || quotes || backslashes) {
	char usequote = quotes > ticks? '\'' : '"';
	bufptr = safemalloc(strlen(val)+2+(quotes > ticks? ticks : quotes)
			    +backslashes+1);
	cp = bufptr;
	*cp++ = usequote;
	while (*val) {
	    if (*val == usequote || *val == '\\')
		*cp++ = '\\';
	    *cp++ = *val++;
	}
	*cp++ = usequote;
	*cp = '\0';
	return bufptr;
    }
    return val;
}

void cwd_check()
{
    char tmpbuf[LBUFLEN];

    if (!cwd)
	cwd = savestr(filexp("~/News"));
    strcpy(tmpbuf,cwd);
    if (chdir(cwd) != 0) {
	safecpy(tmpbuf,filexp(cwd),sizeof tmpbuf);
	if (makedir(tmpbuf,MD_DIR) != 0 || chdir(tmpbuf) != 0) {
	    interp(cmd_buf, (sizeof cmd_buf), "%~/News");
	    if (makedir(cmd_buf,MD_DIR) != 0)
		strcpy(tmpbuf,g_home_dir);
	    else
		strcpy(tmpbuf,cmd_buf);
	    chdir(tmpbuf);
	    if (verbose)
		printf("\
Cannot make directory %s--\n\
	articles will be saved to %s\n\
\n\
",cwd,tmpbuf) FLUSH;
	    else
		printf("\
Can't make %s--\n\
	using %s\n\
\n\
",cwd,tmpbuf) FLUSH;
	}
    }
    free(cwd);
    trn_getwd(tmpbuf, sizeof(tmpbuf));
    if (eaccess(tmpbuf,2)) {
	if (verbose)
	    printf("\
Current directory %s is not writeable--\n\
	articles will be saved to home directory\n\n\
",tmpbuf) FLUSH;
	else
	    printf("%s not writeable--using ~\n\n",tmpbuf) FLUSH;
	strcpy(tmpbuf,g_home_dir);
    }
    cwd = savestr(tmpbuf);
}
