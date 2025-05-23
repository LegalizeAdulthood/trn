/* term.c
 * vi: set sw=4 ts=8 ai sm noet :
 */
// This software is copyrighted as detailed in the LICENSE file.

#include "config/common.h"
#include "trn/terminal.h"

#include "trn/datasrc.h"
#include "trn/art.h"
#include "trn/cache.h"
#include "trn/color.h"
#include "trn/final.h"
#include "trn/help.h"
#include "trn/init.h"
#include "trn/intrp.h"
#include "trn/ng.h"
#include "trn/opt.h"
#include "trn/rt-select.h"
#include "trn/sdisp.h"
#include "trn/string-algos.h"
#include "trn/trn.h"
#include "trn/univ.h"
#include "trn/utf.h"
#include "trn/util.h"
#include "util/env.h"
#include "util/util2.h"

#ifdef MSDOS
#include <conio.h>
#endif

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef u3b2
#undef TIOCGWINSZ
#endif

#undef  USETITE         // use terminal init/exit sequences (not recommended)
#undef  USEKSKE         // use keypad start/end sequences

#ifdef I_TERMIOS
termios g_tty;
termios g_oldtty;
int     g_tty_ch{2};
#endif

char          g_erase_char{}; // rubout character
char          g_kill_char{};  // line delete character
unsigned char g_last_char{};  //
bool          g_bizarre{};    // do we need to restore terminal?
int           g_univ_sel_btn_cnt{};
int           g_newsrc_sel_btn_cnt{};
int           g_add_sel_btn_cnt{};
int           g_newsgroup_sel_btn_cnt{};
int           g_news_sel_btn_cnt{};
int           g_option_sel_btn_cnt{};
int           g_art_pager_btn_cnt{};
char         *g_univ_sel_btns{};
char         *g_newsrc_sel_btns{};
char         *g_add_sel_btns{};
char         *g_newsgroup_sel_btns{};
char         *g_news_sel_btns{};
char         *g_option_sel_btns{};
char         *g_art_pager_btns{};
bool          g_muck_up_clear{};                  // -loco
bool          g_erase_screen{};                   // -e
bool          g_can_home{};                       //
bool          g_erase_each_line{};                // fancy -e
bool          g_allow_typeahead{};                // -T
bool          g_verify{};                         // -v
MarkingMode   g_marking{NO_MARKING};              // -m
MarkingAreas  g_marking_areas{HALF_PAGE_MARKING}; //
ArticleLine   g_init_lines{};                     // -i
bool          g_use_mouse{};                      //
char          g_mouse_modes[32]{"acjlptwvK"};     //
MinorMode     g_mode{MM_INITIALIZING};            // current state of trn
GeneralMode   g_general_mode{GM_INIT};            // general mode of trn

#ifdef HAS_TERMLIB
bool  g_tc_GT{};              // hardware tabs
char *g_tc_BC{};              // backspace character
char *g_tc_UP{};              // move cursor up one line
char *g_tc_CR{};              // get to left margin, somehow
char *g_tc_VB{};              // visible bell
char *g_tc_CE{};              // clear to end of line
char *g_tc_CM{};              // cursor motion
char *g_tc_HO{};              // home cursor
char *g_tc_IL{};              // insert line
char *g_tc_CD{};              // clear to end of display
char *g_tc_SO{};              // begin standout mode
char *g_tc_SE{};              // end standout mode
char *g_tc_US{};              // start underline mode
char *g_tc_UE{};              // end underline mode
char *g_tc_UC{};              // underline a character, if that's how it's done
bool  g_tc_UG{};              // blanks left by US and UE
bool  g_tc_AM{};              // does terminal have automatic margins?
bool  g_tc_XN{};              // does it eat 1st newline after automatic wrap?
int   g_fire_is_out{1};       //
int   g_tc_LINES{};           //
int   g_tc_COLS{};            //
int   g_term_line;            //
int   g_term_col;             //
int   g_term_scrolled;        // how many lines scrolled away
int   g_just_a_sec{960};      // 1 sec at current baud rate (number of nulls)
int   g_page_line{1};         // line number for paging in print_line (origin 1)
bool  g_error_occurred{};     //
int   g_mouse_bar_cnt{};      //
int   g_mouse_bar_width{};    //
bool  g_mouse_is_down{};      //
int   g_auto_arrow_macros{2}; // -A

static char *s_mouse_bar_btns{};
static int   s_mouse_bar_start{};
static bool  s_xmouse_is_on{};
static char *s_tc_CL{}; // home and clear screen
static char *s_tc_TI{}; // initialize terminal
static char *s_tc_TE{}; // reset terminal
static char *s_tc_KS{}; // enter `keypad transmit' mode
static char *s_tc_KE{}; // exit `keypad transmit' mode
static char  s_tc_PC{}; // pad character for use by tputs()
#ifdef _POSIX_SOURCE
static speed_t s_out_speed{}; // terminal output speed,
#else
static long s_out_speed{}; // for use by tputs()
#endif
#endif

struct KeyMap
{
    char km_type[128];
    union KeyMapUnion
    {
        KeyMap *km_km;
        char *km_str;
    } km_ptr[128];
};

enum
{
    KM_NOTHING = 0,
    KM_STRING = 1,
    KM_KEYMAP = 2,
    KM_BOGUS = 3,
    KM_TMASK = 3,
    KM_GSHIFT = 4,
    KM_GMASK = 7
};

enum
{
    TC_STRINGS = 48 // number of colors we can keep track of
};

static char        s_circle_buf[PUSH_SIZE]{};
static int         s_next_in{};
static int         s_next_out{};
static const char *s_read_err{"rn read error"};

#ifndef MSDOS
static char s_termcap_area[TC_SIZE]; // area for "compiled" termcap strings
#endif
static KeyMap *s_top_map{};
static char   *s_lines_export{};
static char   *s_cols_export{};
static int     s_left_cost{};
static int     s_up_cost{};
static bool    s_got_a_char{}; // true if we got a char since eating

static void    mac_init(char *tcbuf);
static KeyMap *new_key_map();
static void    show_key_map(KeyMap *curmap, char *prefix);
static int     echo_char(char_int ch);
static void    line_col_calcs();
static void    mouse_input(const char *cp);

// terminal initialization

void term_init()
{
    save_tty();                          // remember current tty state

#ifdef I_TERMIOS
    s_out_speed = cfgetospeed(&g_tty);  // for tputs() (output)
    g_erase_char = g_tty.c_cc[VERASE]; // for finish_command()
    g_kill_char = g_tty.c_cc[VKILL];   // for finish_command()
#else // !I_TERMIOS
#ifdef MSDOS
    s_out_speed = B19200;
    g_erase_char = '\b';
    g_kill_char = Ctl('u');
    g_tc_GT = true;
#else
    ..."Don't know how to initialize the terminal!"
#endif // !MSDOS
#endif // !I_TERMIOS

    // The following could be a table but I can't be sure that there isn't
    // some degree of sparsity out there in the world.

    switch (s_out_speed)                         // 1 second of padding
    {
#ifdef BEXTA
    case BEXTA:  g_just_a_sec = 1920; break;
#else
#ifdef B19200
    case B19200: g_just_a_sec = 1920; break;
#endif
#endif
    case B9600:  g_just_a_sec =  960; break;
    case B4800:  g_just_a_sec =  480; break;
    case B2400:  g_just_a_sec =  240; break;
    case B1800:  g_just_a_sec =  180; break;
    case B1200:  g_just_a_sec =  120; break;
    case B600:   g_just_a_sec =   60; break;
    case B300:   g_just_a_sec =   30; break;
    // do I really have to type the rest of this???
    case B200:   g_just_a_sec =   20; break;
    case B150:   g_just_a_sec =   15; break;
    case B134:   g_just_a_sec =   13; break;
    case B110:   g_just_a_sec =   11; break;
    case B75:    g_just_a_sec =    8; break;
    case B50:    g_just_a_sec =    5; break;
    default:     g_just_a_sec =  960; break;
                                    // if we are running detached I
    }                               // don't want to know about it!
}

#ifdef PENDING
# if !defined(FIONREAD) && !defined(HAS_RDCHK) && !defined(MSDOS)
int devtty;
# endif
#endif

#ifdef HAS_TERMLIB
#ifndef MSDOS
// guarantee capability pointer != nullptr
// (I believe terminfo will ignore the &tmpaddr argument.)
inline char *Tgetstr(const char *key)
{
    char *tmpaddr{};
    char *temp = tgetstr(key, &tmpaddr);
    static char s_empty[1]{};
    return temp ? temp : s_empty;
}
#endif
#endif

// set terminal characteristics

//char* tcbuf;          // temp area for "uncompiled" termcap entry
void term_set(char *tcbuf)
{
    char* tmpaddr;                      // must not be register
    char* tmpstr;
    char* s;
    int status;
#ifdef TIOCGWINSZ
    struct winsize winsize;
#endif

#ifdef PENDING
#if !defined (FIONREAD) && !defined (HAS_RDCHK) && !defined(MSDOS)
    // do no delay reads on something that always gets closed on exit

    devtty = fileno(stdin);
    if (isatty(devtty))
    {
        devtty = open("/dev/tty",0);
        if (devtty < 0)
        {
            std::printf(cantopen,"/dev/tty");
            finalize(1);
        }
        fcntl(devtty,F_SETFL,O_NDELAY);
    }
#endif
#endif

    // get all that good termcap stuff

#ifdef HAS_TERMLIB
#ifdef MSDOS
    g_tc_BC = "\b";
    g_tc_UP = "\033[A";
    g_tc_CR = "\r";
    g_tc_VB = "";
    s_tc_CL = "\033[H\033[2J";
    g_tc_CE = "\033[K";
    s_tc_TI = "";
    s_tc_TE = "";
    s_tc_KS = "";
    s_tc_KE = "";
    g_tc_CM = "\033[%d;%dH";
    g_tc_HO = "\033[H";
    g_tc_IL = "";
    g_tc_CD = "";
    g_tc_SO = "\033[7m";
    g_tc_SE = "\033[m";
    g_tc_US = "\033[7m";
    g_tc_UE = "\033[m";
    g_tc_UC = "";
    g_tc_AM = true;
#else
    s = getenv("TERM");
    status = tgetent(tcbuf,s? s : "dumb");      // get termcap entry
    if (status < 1)
    {
        std::printf("No termcap %s found.\n", status ? "file" : "entry");
        finalize(1);
    }
    tmpaddr = s_termcap_area;           // set up strange tgetstr pointer
    s = Tgetstr("pc");                  // get pad character
    s_tc_PC = *s;                       // get it where tputs wants it
    if (!tgetflag("bs"))                // is backspace not used?
    {
        g_tc_BC = Tgetstr("bc");        // find out what is
        if (empty(g_tc_BC))             // terminfo grok's 'bs' but not 'bc'
        {
            g_tc_BC = Tgetstr("le");
            if (empty(g_tc_BC))
            {
                g_tc_BC = "\b";         // better than nothing...
            }
        }
    }
    else
    {
        g_tc_BC = "\b";                 // make a backspace handy
    }
    g_tc_UP = Tgetstr("up");            // move up a line
    s_tc_CL = Tgetstr("cl");            // get clear string
    g_tc_CE = Tgetstr("ce");            // clear to end of line string
    s_tc_TI = Tgetstr("ti");            // initialize display
    s_tc_TE = Tgetstr("te");            // reset display
    s_tc_KS = Tgetstr("ks");            // enter `keypad transmit' mode
    s_tc_KE = Tgetstr("ke");            // exit `keypad transmit' mode
    g_tc_HO = Tgetstr("ho");            // home cursor
    g_tc_IL = Tgetstr("al");            // insert (add) line
    g_tc_CM = Tgetstr("cm");            // cursor motion
    g_tc_CD = Tgetstr("cd");            // clear to end of display
    if (!*g_tc_CE)
    {
        g_tc_CE = g_tc_CD;
    }
    g_tc_SO = Tgetstr("so");            // begin standout
    g_tc_SE = Tgetstr("se");            // end standout
    const bool tc_SG = tgetnum("sg") > 0; // standout glitch; blanks left by SG, SE
    g_tc_US = Tgetstr("us");            // start underline
    g_tc_UE = Tgetstr("ue");            // end underline
    g_tc_UG = tgetnum("ug") > 0;        // underline glitch
    if (*g_tc_US)
    {
        g_tc_UC = "";           // UC must not be nullptr
    }
    else
    {
        g_tc_UC = Tgetstr("uc");                // underline a character
    }
    if (!*g_tc_US && !*g_tc_UC)                 // no underline mode?
    {
        g_tc_US = g_tc_SO;                      // substitute standout mode
        g_tc_UE = g_tc_SE;
        g_tc_UG = tc_SG;
    }
    g_tc_LINES = tgetnum("li");         // lines per page
    g_tc_COLS = tgetnum("co");          // columns on page

#ifdef TIOCGWINSZ
    {
        struct winsize ws;
        if (ioctl(0, TIOCGWINSZ, &ws) >= 0 && ws.ws_row > 0 && ws.ws_col > 0)
        {
            g_tc_LINES = ws.ws_row;
            g_tc_COLS = ws.ws_col;
        }
    }
#endif

    g_tc_AM = tgetflag("am");           // terminal wraps automatically?
    g_tc_XN = tgetflag("xn");           // then eats next newline?
    g_tc_VB = Tgetstr("vb");
    if (!*g_tc_VB)
    {
        g_tc_VB = "\007";
    }
    g_tc_CR = Tgetstr("cr");
    if (!*g_tc_CR)
    {
        if (tgetflag("nc") && *g_tc_UP)
        {
            g_tc_CR = safe_malloc((MemorySize)std::strlen(g_tc_UP)+2);
            std::sprintf(g_tc_CR,"%s\r",g_tc_UP);
        }
        else
        {
            g_tc_CR = "\r";
        }
    }
#ifdef TIOCGWINSZ
    if (ioctl(1, TIOCGWINSZ, &winsize) >= 0)
    {
        if (winsize.ws_row > 0)
        {
            g_tc_LINES = winsize.ws_row;
        }
        if (winsize.ws_col > 0)
        {
            g_tc_COLS = winsize.ws_col;
        }
    }
# endif
#endif
    if (!*g_tc_UP)                      // no UP string?
    {
        g_marking = NO_MARKING;          // disable any marking
    }
    if (*g_tc_CM || *g_tc_HO)
    {
        g_can_home = true;
    }
    if (!*g_tc_CD || !g_can_home)               // can we CE, CD, and home?
    {
        g_erase_each_line = false;      // no, so disable use of clear eol
    }
    if (g_muck_up_clear)                        // this is for weird HPs
    {
        s_tc_CL = nullptr;
    }
    s_left_cost = std::strlen(g_tc_BC);
    s_up_cost = std::strlen(g_tc_UP);
#else // !HAS_TERMLIB
    ..."Don't know how to set the terminal!"
#endif // !HAS_TERMLIB
    termlib_init();
    line_col_calcs();
    no_echo();                           // turn off echo
    cr_mode();                           // enter cbreak mode
    std::sprintf(g_buf, "%d", g_tc_LINES);
    s_lines_export = export_var("LINES",g_buf);
    std::sprintf(g_buf, "%d", g_tc_COLS);
    s_cols_export = export_var("COLUMNS",g_buf);

    mac_init(tcbuf);
}

//char* seq;    // input sequence of keys
//char* def;    // definition
void set_macro(char *seq, char *def)
{
    mac_line(def,seq,0);
    // check for common (?) brain damage: ku/kd/etc sequence may be the
    // cursor move sequence instead of the input sequence.
    // (This happens on the local xterm definitions.)
    // Try to recognize and adjust for this case.
    //
    if (seq[0] == '\033' && seq[1] == '[' && seq[2])
    {
        char lbuf[LINE_BUF_LEN];     // copy of possibly non-writable string
        std::strcpy(lbuf,seq);
        lbuf[1] = 'O';
        mac_line(def,lbuf,0);
    }
    if (seq[0] == '\033' && seq[1] == 'O' && seq[2])
    {
        char lbuf[LINE_BUF_LEN];     // copy of possibly non-writable string
        std::strcpy(lbuf,seq);
        lbuf[1] = '[';
        mac_line(def,lbuf,0);
    }
}

static char* s_up[] = {
    "^@",
    // '(' at article or pager, '[' in thread sel, 'p' otherwise
    "%(%m=[ap]?\\(:%(%m=t?[:p))",
    // '(' at article or pager, '[' in thread sel, 'p' otherwise
    "%(%m=[ap]?\\(:%(%m=t?[:p))"
};
static char *s_down[] = {
    "^@",
    // ')' at article or pager, ']' in thread sel, 'n' otherwise
    "%(%m=[ap]?\\):%(%m=t?]:n))",
    // ')' at article or pager, ']' in thread sel, 'n' otherwise
    "%(%m=[ap]?\\):%(%m=t?]:n))"
};
static char *s_left[] = {
    "^@",
    // '[' at article or pager, 'Q' otherwise
    "%(%m=[ap]?\\[:Q)",
    // '[' at article or pager, '<' otherwise
    "%(%m=[ap]?\\[:<)"
};
static char *s_right[] = {
    "^@",
    // ']' at article or pager, CR otherwise
    "%(%m=[ap]?\\]:^j)",
    // CR at newsgroups, ']' at article or pager, '>' otherwise
    "%(%m=n?^j:%(%m=[ap]?\\]:>))"
};

// Turn the arrow keys into macros that do some basic trn functions.
// Code provided by Clifford Adams.
//
void arrow_macros(char *tmpbuf)
{
#ifdef HAS_TERMLIB
    char lbuf[256];                     // should be long enough
#ifndef MSDOS
    char* tmpaddr = tmpbuf;
#endif
    char* tmpstr;

    // If arrows are defined as single keys, we probably don't
    // want to redefine them.  (The tvi912c defines kl as ^H)
    //
#ifdef MSDOS
    std::strcpy(lbuf,"\035\110");
#else
    std::strcpy(lbuf,Tgetstr("ku"));         // up
#endif
    if ((int)std::strlen(lbuf) > 1)
    {
        set_macro(lbuf,s_up[g_auto_arrow_macros]);
    }

#ifdef MSDOS
    std::strcpy(lbuf,"\035\120");
#else
    std::strcpy(lbuf,Tgetstr("kd"));         // down
#endif
    if ((int)std::strlen(lbuf) > 1)
    {
        set_macro(lbuf,s_down[g_auto_arrow_macros]);
    }

#ifdef MSDOS
    std::strcpy(lbuf,"\035\113");
#else
    std::strcpy(lbuf,Tgetstr("kl"));         // left
#endif
    if ((int)std::strlen(lbuf) > 1)
    {
        set_macro(lbuf,s_left[g_auto_arrow_macros]);
    }

#ifdef MSDOS
    std::strcpy(lbuf,"\035\115");
#else
    std::strcpy(lbuf,Tgetstr("kr"));         // right
#endif
    if ((int)std::strlen(lbuf) > 1)
    {
        set_macro(lbuf,s_right[g_auto_arrow_macros]);
    }

    if (*lbuf == '\033')
    {
        set_macro("\033\033", "\033");
    }
#endif
}

static void mac_init(char *tcbuf)
{
    char tmpbuf[1024];

    if (g_auto_arrow_macros)
    {
        arrow_macros(tmpbuf);
    }
    std::FILE *macros;
    if (!g_use_threads
     || (macros = std::fopen(file_exp(get_val_const("TRNMACRO",TRNMACRO)),"r")) == nullptr)
    {
        macros = std::fopen(file_exp(get_val_const("RNMACRO",RNMACRO)),"r");
    }
    if (macros)
    {
        while (std::fgets(tcbuf,TCBUF_SIZE,macros) != nullptr)
        {
            mac_line(tcbuf,tmpbuf,sizeof tmpbuf);
        }
        std::fclose(macros);
    }
}

void mac_line(char *line, char *tmpbuf, int tbsize)
{
    char*       m;
    KeyMap*     curmap;
    int         garbage = 0;
    static char override[] = "\nkeymap overrides string\n";

    if (s_top_map == nullptr)
    {
        s_top_map = new_key_map();
    }
    if (*line == '#' || *line == '\n')
    {
        return;
    }
    int ch = std::strlen(line) - 1;
    if (line[ch] == '\n')
    {
        line[ch] = '\0';
    }
    // A 0 length signifies we already parsed the macro into tmpbuf,
    // so line is just the definition.
    if (tbsize)
    {
        m = do_interp(tmpbuf,tbsize,line," \t",nullptr);
    }
    else
    {
        m = line;
    }
    if (!*m)
    {
        return;
    }
    m = skip_hor_space(m);
    curmap=s_top_map;
    for (char *s = tmpbuf; *s; s++)
    {
        ch = *s & 0177;
        if (s[1] == '+' && std::isdigit(s[2]))
        {
            s += 2;
            garbage = (*s & KM_GMASK) << KM_GSHIFT;
        }
        else
        {
            garbage = 0;
        }
        if (s[1])
        {
            if ((curmap->km_type[ch] & KM_TMASK) == KM_STRING)
            {
                if (tbsize)
                {
                    std::fputs(override,stdout);
                    term_down(2);
                }
                std::free(curmap->km_ptr[ch].km_str);
                curmap->km_ptr[ch].km_str = nullptr;
            }
            curmap->km_type[ch] = KM_KEYMAP + garbage;
            if (curmap->km_ptr[ch].km_km == nullptr)
            {
                curmap->km_ptr[ch].km_km = new_key_map();
            }
            curmap = curmap->km_ptr[ch].km_km;
        }
        else
        {
            if (tbsize && (curmap->km_type[ch] & KM_TMASK) == KM_KEYMAP)
            {
                std::fputs(override,stdout);
                term_down(2);
            }
            else
            {
                curmap->km_type[ch] = KM_STRING + garbage;
                curmap->km_ptr[ch].km_str = save_str(m);
            }
        }
    }
}

static KeyMap *new_key_map()
{
    KeyMap* map;

#ifndef lint
    map = (KeyMap*)safe_malloc(sizeof(KeyMap));
#else
    map = nullptr;
#endif // lint
    for (int i = 127; i >= 0; i--)
    {
        map->km_ptr[i].km_km = nullptr;
        map->km_type[i] = KM_NOTHING;
    }
    return map;
}

void show_macros()
{
    char prebuf[64];

    if (s_top_map != nullptr)
    {
        print_lines("Macros:\n", STANDOUT);
        *prebuf = '\0';
        show_key_map(s_top_map,prebuf);
    }
    else
    {
        print_lines("No macros defined.\n", NO_MARKING);
    }
}

static void show_key_map(KeyMap *curmap, char *prefix)
{
    char* next = prefix + std::strlen(prefix);

    for (int i = 0; i < 128; i++)
    {
        int kt = curmap->km_type[i];
        if (kt != 0)
        {
            if (i < ' ')
            {
                std::sprintf(next,"^%c",i+64);
            }
            else if (i == ' ')
            {
                std::strcpy(next,"\\040");
            }
            else if (i == 127)
            {
                std::strcpy(next,"^?");
            }
            else
            {
                std::sprintf(next,"%c",i);
            }
            if ((kt >> KM_GSHIFT) & KM_GMASK)
            {
                std::sprintf(g_cmd_buf,"+%d", (kt >> KM_GSHIFT) & KM_GMASK);
                std::strcat(next,g_cmd_buf);
            }
            switch (kt & KM_TMASK)
            {
            case KM_NOTHING:
                std::sprintf(g_cmd_buf,"%s   %c\n",prefix,i);
                print_lines(g_cmd_buf, NO_MARKING);
                break;

            case KM_KEYMAP:
                show_key_map(curmap->km_ptr[i].km_km, prefix);
                break;

            case KM_STRING:
                std::sprintf(g_cmd_buf,"%s   %s\n",prefix,curmap->km_ptr[i].km_str);
                print_lines(g_cmd_buf, NO_MARKING);
                break;

            case KM_BOGUS:
                std::sprintf(g_cmd_buf,"%s   BOGUS\n",prefix);
                print_lines(g_cmd_buf, STANDOUT);
                break;
            }
        }
    }
}

void set_mode(GeneralMode new_gmode, MinorMode new_mode)
{
    if (g_general_mode != new_gmode || g_mode != new_mode)
    {
        g_general_mode = new_gmode;
        g_mode = new_mode;
        xmouse_check();
    }
}

// routine to pass to tputs

int put_char(char_int ch)
{
    std::putchar(ch);
#ifdef lint
    ch = '\0';
    ch = ch;
#endif
    return 0;
}

static int s_not_echoing{};

void hide_pending()
{
    s_not_echoing = 1;
    push_char(0200);
}

bool finput_pending(bool check_term)
{
    while (s_next_out != s_next_in)
    {
        if (s_circle_buf[s_next_out] != '\200')
        {
            return true;
        }
        switch (s_not_echoing)
        {
        case 0:
            return true;

        case 1:
            s_next_out++;
            s_next_out %= PUSH_SIZE;
            s_not_echoing = 0;
            break;

        default:
            s_circle_buf[s_next_out] = '\n';
            s_not_echoing = 0;
            return true;
        }
    }
#ifdef PENDING
    if (check_term)
    {
# ifdef FIONREAD
        long iocount;
        ioctl(0, FIONREAD, &iocount);
        return (int)iocount;
# else // !FIONREAD
#  ifdef HAS_RDCHK
        return rdchk(0);
#  else // !HAS_RDCHK
#   ifdef MSDOS
        return kbhit();
#   else // !MSDOS
        return circfill();
#   endif // !MSDOS
#  endif // !HAS_RDCHK
#  endif // !FIONREAD
    }
# endif // !PENDING
    return false;
}

// input the 2nd and succeeding characters of a multi-character command
// returns true if command finished, false if they rubbed out first character

static int s_buff_limit = LINE_BUF_LEN;

bool finish_command(int donewline)
{
    char *s = g_buf;
    if (s[1] != FINISH_CMD)              // someone faking up a command?
    {
        return true;
    }

    GeneralMode gmode_save = g_general_mode;
    set_mode(GM_INPUT,g_mode);
    if (s_not_echoing)
    {
        s_not_echoing = 2;
    }
    do
    {
        s = edit_buf(s, g_buf);
        if (s == g_buf)                         // entire string gone?
        {
            std::fflush(stdout);             // return to single char command mode
            set_mode(gmode_save,g_mode);
            return false;
        }
        if (s - g_buf == s_buff_limit)
        {
            break;
        }
        std::fflush(stdout);
        get_cmd(s);
        if (errno || *s == '\f')
        {
            *s = Ctl('r');              // force rewrite on CONT
        }
    } while (*s != '\r' && *s != '\n'); // until CR or NL (not echoed)
    g_mouse_is_down = false;

    while (s[-1] == ' ')
    {
        s--;
    }
    *s = '\0';                          // terminate the string nicely

    if (donewline)
    {
        newline();
    }

    set_mode(gmode_save,g_mode);
    return true;                        // retrn success
}

static int echo_char(char_int ch)
{
    if (((Uchar) ch & 0x7F) < ' ')
    {
        std::putchar('^');
        std::putchar((ch & 0x7F) | 64);
        return 2;
    }
    if (ch == '\177')
    {
        std::putchar('^');
        std::putchar('?');
        return 2;
    }
    std::putchar(ch);
    return 1;
}

static bool s_screen_is_dirty{}; // TODO: remove this?

// Process the character *s in the buffer g_buf returning the new 's'

char *edit_buf(char *s, const char *cmd)
{
    static bool quoteone = false;
    if (quoteone)
    {
        quoteone = false;
        if (s != g_buf)
        {
            goto echo_it;
        }
    }
    if (*s == '\033')           // substitution desired?
    {
        char  tmpbuf[4];
        char *cpybuf;

        tmpbuf[0] = '%';
        read_tty(&tmpbuf[1],1);
#ifdef RAWONLY
        tmpbuf[1] &= 0177;
#endif
        tmpbuf[2] = '\0';
        if (tmpbuf[1] == 'h')
        {
            (void) help_subs();
            *s = '\0';
            reprint();
        }
        else if (tmpbuf[1] == '\033')
        {
            *s = '\0';
            cpybuf = save_str(g_buf);
            interp_search(g_buf, sizeof g_buf, cpybuf, cmd);
            std::free(cpybuf);
            s = g_buf + std::strlen(g_buf);
            reprint();
        }
        else
        {
            interp_search(s, sizeof g_buf - (s-g_buf), tmpbuf, cmd);
            std::fputs(s,stdout);
            s += std::strlen(s);
        }
        return s;
    }
    if (*s == g_erase_char)                // they want to rubout a char?
    {
        if (s != g_buf)
        {
            rubout();
            s--;                        // discount the char rubbed out
            if (!at_norm_char(s))
            {
                rubout();
            }
        }
        return s;
    }
    if (*s == g_kill_char)                 // wipe out the whole line?
    {
        while (s != g_buf)              // emulate that many ERASEs
        {
            rubout();
            s--;
            if (!at_norm_char(s))
            {
                rubout();
            }
        }
        return s;
    }
    if (*s == Ctl('w'))            // wipe out one word?
    {
        if (s == g_buf)
        {
            return s;
        }
        *s-- = ' ';
        while (!std::isspace(*s) || std::isspace(s[1]))
        {
            rubout();
            if (!at_norm_char(s))
            {
                rubout();
            }
            if (s == g_buf)
            {
                return g_buf;
            }
            s--;
        }
        return s+1;
    }
    if (*s == Ctl('r'))
    {
        *s = '\0';
        reprint();
        return s;
    }
    if (*s == Ctl('v'))
    {
        std::putchar('^');
        backspace();
        std::fflush(stdout);
        get_cmd(s);
    }
    else if (*s == '\\')
    {
        quoteone = true;
    }

echo_it:
    if (!s_not_echoing)
    {
        echo_char(*s);
    }
    return s+1;
}

bool finish_dbl_char()
{
    int buflimit_save = s_buff_limit;
    int not_echoing_save = s_not_echoing;
    s_buff_limit = 2;
    bool ret = finish_command(false);
    s_buff_limit = buflimit_save;
    s_not_echoing = not_echoing_save;
    return ret;
}

// discard any characters typed ahead

void eat_typeahead()
{
    static double last_time = 0.;
    double this_time = current_time();

    // do not eat typeahead while creating virtual group
    if (g_univ_ng_virt_flag)
    {
      return;
    }
    // Don't eat twice before getting a character
    if (!s_got_a_char)
    {
        return;
    }
    s_got_a_char = false;

    // cancel only keyboard stuff
    if (!g_allow_typeahead && !g_mouse_is_down && !macro_pending() //
        && this_time - last_time > 0.3)
    {
#ifdef PENDING
        KeyMap*curmap = s_top_map;
        int    j;
        for (j = 0; input_pending();)
        {
            errno = 0;
            if (read_tty(&g_buf[j], 1) < 0)
            {
                if (errno && errno != EINTR)
                {
                    std::perror(s_read_err);
                    sig_catcher(0);
                }
                continue;
            }
            Uchar lc = *(Uchar*)g_buf;
            if ((lc & 0200) || curmap == nullptr)
            {
                curmap = s_top_map;
                j = 0;
                continue;
            }
            j++;
            for (int i = (curmap->km_type[lc] >> KM_GSHIFT) & KM_GMASK; i; i--)
            {
                if (!input_pending())
                {
                    goto dbl_break;
                }
                read_tty(&g_buf[j++],1);
            }

            switch (curmap->km_type[lc] & KM_TMASK)
            {
            case KM_STRING:           // a string?
            case KM_NOTHING:           // no entry?
                curmap = s_top_map;
                j = 0;
                continue;

            case KM_KEYMAP:           // another keymap?
                curmap = curmap->km_ptr[lc].km_km;
                break;
            }
        }
dbl_break:
        if (j)
        {
            // Don't delete a partial macro sequence
            g_buf[j] = '\0';
            push_string(g_buf,0);
        }
#else // this is probably v7
#ifdef I_TERMIOS
        tcsetattr(g_tty_ch, TCSAFLUSH, &g_tty);
#else
        ... "Don't know how to eat typeahead!"
#endif
#endif
    }
    last_time = this_time;
}

void save_typeahead(char *buf, int len)
{
    while (input_pending())
    {
        int cnt = read_tty(buf, len);
        buf += cnt;
        len -= cnt;
    }
    *buf = '\0';
}

void settle_down()
{
    dingaling();
    std::fflush(stdout);
    // sleep(1);
    s_next_out = s_next_in;                       // empty s_circlebuf
    s_not_echoing = 0;
    eat_typeahead();
}

static bool s_ignore_eintr = false;

#ifdef SIGALRM
Signal_t alarm_catcher(int signo)
{
    s_ignore_eintr = true;
    check_data_sources();
    sigset(SIGALRM,alarm_catcher);
    (void) alarm(DATASRC_ALARM_SECS);
}
#endif

// read a character from the terminal, with multi-character pushback

int read_tty(char *addr, int size)
{
    if (macro_pending())
    {
        *addr = s_circle_buf[s_next_out++];
        s_next_out %= PUSH_SIZE;
        return 1;
    }
#ifdef MSDOS
    *addr = getch();
    if (*addr == '\0')
    {
        *addr = Ctl('\035');
    }
    size = 1;
#else
    size = read(0,addr,size);
#endif
#ifdef RAWONLY
    *addr &= 0177;
#endif
    s_got_a_char = true;
    return size;
}

#ifdef PENDING
# if !defined(FIONREAD) && !defined(HAS_RDCHK) && !defined(MSDOS)
int circfill()
{
    int Howmany;

    errno = 0;
    Howmany = read(devtty,s_circlebuf+s_nextin,1);

    if (Howmany < 0 && (errno == EAGAIN || errno == EINTR))
    {
        Howmany = 0;
    }
    if (Howmany)
    {
        s_nextin += Howmany;
        s_nextin %= PUSHSIZE;
    }
    return Howmany;
}
# endif // FIONREAD
#endif // PENDING

void push_char(char_int c)
{
    s_next_out--;
    if (s_next_out < 0)
    {
        s_next_out = PUSH_SIZE - 1;
    }
    if (s_next_out == s_next_in)
    {
        std::fputs("\npushback buffer overflow\n",stdout);
        sig_catcher(0);
    }
    s_circle_buf[s_next_out] = c;
}

// print an underlined string, one way or another

void under_print(const char *s)
{
    TRN_ASSERT(g_tc_UC);
    if (*g_tc_UC)       // char by char underline?
    {
        while (*s)
        {
            if (!at_norm_char(s))
            {
                std::putchar('^');
                backspace();// back up over it
                underchar();// and do the underline
                std::putchar((*s & 0x7F) | 64);
                backspace();// back up over it
                underchar();// and do the underline
            }
            else
            {
                std::putchar(*s);
                backspace();// back up over it
                underchar();// and do the underline
            }
            s++;
        }
    }
    else                // start and stop underline
    {
        underline();    // start underlining
        while (*s)
        {
            echo_char(*s++);
        }
        un_underline(); // stop underlining
    }
}

// keep screen from flashing strangely on magic cookie terminals

#ifdef NO_FIREWORKS
void no_so_fire()
{
    // should we disable fireworks?
    if (!(g_fire_is_out & STANDOUT) && (g_term_line | g_term_col) == 0 && *g_tc_UP && *g_tc_SE)
    {
        newline();
        un_standout();
        up_line();
        carriage_return();
    }
}
#endif

#ifdef NO_FIREWORKS
void no_ul_fire()
{
    // should we disable fireworks?
    if (!(g_fire_is_out & UNDERLINE) && (g_term_line | g_term_col) == 0 && *g_tc_UP && *g_tc_US)
    {
        newline();
        un_underline();
        up_line();
        carriage_return();
    }
}
#endif

// get a character into a buffer

void get_cmd(char *whatbuf)
{
    int times = 0;                      // loop detector

    if (!input_pending())
    {
#ifdef SIGALRM
        sigset(SIGALRM,alarm_catcher);
        (void) alarm(DATASRC_ALARM_SECS);
#endif
    }

tryagain:
    KeyMap *curmap = s_top_map;
    bool no_macros = (whatbuf != g_buf && !s_xmouse_is_on);
    while (true)
    {
        g_int_count = 0;
        errno = 0;
        s_ignore_eintr = false;
        if (read_tty(whatbuf, 1) < 0)
        {
            if (!errno)
            {
                errno = EINTR;
            }
            if (errno == EINTR)
            {
                if (s_ignore_eintr)
                {
                    continue;
                }
#ifdef SIGALRM
                (void) alarm(0);
#endif
                return;
            }
            std::perror(s_read_err);
            sig_catcher(0);
        }
        g_last_char = *(Uchar*)whatbuf;
        if (g_last_char & 0200 || no_macros)
        {
            *whatbuf &= 0177;
            goto got_canonical;
        }
        if (curmap == nullptr)
        {
            goto got_canonical;
        }
        for (int i = (curmap->km_type[g_last_char] >> KM_GSHIFT) & KM_GMASK; i; i--)
        {
            read_tty(&whatbuf[i],1);
        }

        switch (curmap->km_type[g_last_char] & KM_TMASK)
        {
        case KM_NOTHING:               // no entry?
            if (curmap == s_top_map)     // unmapped canonical
            {
                goto got_canonical;
            }
            settle_down();
            goto tryagain;

        case KM_KEYMAP:               // another keymap?
            curmap = curmap->km_ptr[g_last_char].km_km;
            TRN_ASSERT(curmap != nullptr);
            break;

        case KM_STRING:               // a string?
            push_string(curmap->km_ptr[g_last_char].km_str,0200);
            if (++times > 20)           // loop?
            {
                std::fputs("\nmacro loop?\n",stdout);
                term_down(2);
                settle_down();
            }
            no_macros = false;
            goto tryagain;
        }
    }

got_canonical:
    // This hack is for mouse support
    if (s_xmouse_is_on && *whatbuf == Ctl('c'))
    {
        mouse_input(whatbuf+1);
        times = 0;
        goto tryagain;
    }
#ifdef I_SGTTY
    if (*whatbuf == '\r')
    {
        *whatbuf = '\n';
    }
#endif
    if (whatbuf == g_buf)
    {
        whatbuf[1] = FINISH_CMD;         // tell finish_command to work
    }
#ifdef SIGALRM
    (void) alarm(0);
#endif
}

void push_string(char *str, char_int bits)
{
    char tmpbuf[PUSH_SIZE];
    char* s = tmpbuf;

    TRN_ASSERT(str != nullptr);
    interp(tmpbuf,PUSH_SIZE,str);
    for (int i = std::strlen(s) - 1; i >= 0; i--)
    {
        push_char(s[i] ^ bits);
    }
}

int get_anything()
{
    char tmpbuf[64];
    MinorMode mode_save = g_mode;

reask_anything:
    unflush_output();                   // disable any ^O in effect
    color_object(COLOR_MORE, true);
    if (g_verbose)
    {
        std::fputs("[Type space to continue] ",stdout);
    }
    else
    {
        std::fputs("[MORE] ",stdout);
    }
    color_pop();        // of COLOR_MORE
    std::fflush(stdout);
    eat_typeahead();
    if (g_int_count)
    {
        return -1;
    }
    cache_until_key();
    set_mode(g_general_mode, MM_ANY_KEY_PROMPT);
    get_cmd(tmpbuf);
    set_mode(g_general_mode,mode_save);
    if (errno || *tmpbuf == '\f')
    {
        newline();                      // if return from stop signal
        goto reask_anything;            // give them a prompt again
    }
    if (*tmpbuf == 'h')
    {
        if (g_verbose)
        {
            std::fputs("\nType q to quit or space to continue.\n",stdout);
        }
        else
        {
            std::fputs("\nq to quit, space to continue.\n",stdout);
        }
        term_down(2);
        goto reask_anything;
    }
    else if (*tmpbuf != ' ' && *tmpbuf != '\n')
    {
        erase_line(false);      // erase the prompt
        return *tmpbuf == 'q' ? -1 : *tmpbuf;
    }
    if (*tmpbuf == '\n')
    {
        g_page_line = g_tc_LINES - 1;
        erase_line(false);
    }
    else
    {
        g_page_line = 1;
        if (g_erase_screen)             // -e?
        {
            clear();                    // clear screen
        }
        else
        {
            erase_line(false);          // erase the prompt
        }
    }
    return 0;
}

int pause_get_cmd()
{
    MinorMode mode_save = g_mode;

    unflush_output();                   // disable any ^O in effect
    color_object(COLOR_CMD, true);
    if (g_verbose)
    {
        std::fputs("[Type space or a command] ",stdout);
    }
    else
    {
        std::fputs("[CMD] ",stdout);
    }
    color_pop();        // of COLOR_CMD
    std::fflush(stdout);
    eat_typeahead();
    if (g_int_count)
    {
        return -1;
    }
    cache_until_key();
    set_mode(g_general_mode,MM_ANY_KEY_PROMPT);
    get_cmd(g_buf);
    set_mode(g_general_mode,mode_save);
    if (errno || *g_buf == '\f')
    {
        return 0;                       // if return from stop signal
    }
    if (*g_buf != ' ')
    {
        erase_line(false);      // erase the prompt
        return *g_buf;
    }
    return 0;
}

void in_char(const char *prompt, MinorMode newmode, const char *dflt)
{
    MinorMode   mode_save = g_mode;
    GeneralMode gmode_save = g_general_mode;
    const char  *s;
    int          newlines;

    for (newlines = 0, s = prompt; *s; s++)
    {
        if (*s == '\n')
        {
            newlines++;
        }
    }

reask_in_char:
    unflush_output();                   // disable any ^O in effect
    std::printf("%s [%s] ", prompt, dflt);
    std::fflush(stdout);
    term_down(newlines);
    eat_typeahead();
    set_mode(GM_PROMPT,newmode);
    get_cmd(g_buf);
    if (errno || *g_buf == '\f')
    {
        newline();                      // if return from stop signal
        goto reask_in_char;             // give them a prompt again
    }
    set_def(g_buf,dflt);
    set_mode(gmode_save,mode_save);
}

void in_answer(const char *prompt, MinorMode newmode)
{
    MinorMode   mode_save = g_mode;
    GeneralMode gmode_save = g_general_mode;

reask_in_answer:
    unflush_output();                   // disable any ^O in effect
    std::fputs(prompt,stdout);
    std::fflush(stdout);
    eat_typeahead();
    set_mode(GM_INPUT,newmode);
reinp_in_answer:
    get_cmd(g_buf);
    if (errno || *g_buf == '\f')
    {
        newline();                      // if return from stop signal
        goto reask_in_answer;           // give them a prompt again
    }
    if (*g_buf == g_erase_char)
    {
        goto reinp_in_answer;
    }
    if (*g_buf != '\n' && *g_buf != ' ')
    {
        if (!finish_command(false))
        {
            goto reinp_in_answer;
        }
    }
    else
    {
        g_buf[1] = '\0';
    }
    newline();
    set_mode(gmode_save,mode_save);
}

// If this takes more than one line, return false

bool in_choice(const char *prompt, char *value, char *choices, MinorMode newmode)
{
    MinorMode mode_save = g_mode;
    GeneralMode gmode_save = g_general_mode;

    unflush_output();                   // disable any ^O in effect
    eat_typeahead();
    set_mode(GM_CHOICE,newmode);
    s_screen_is_dirty = false;

    char prefixes[80];
    char *cp = choices;
    if (*cp == '[')
    {
        char *dest = prefixes;
        ++cp;
        while (*cp != ']')
        {
            if (*cp == '/')
            {
                *dest++ = '\0';
                cp++;
            }
            else
            {
                *dest++ = *cp++;
            }
        }
        *dest++ = '\0';
        *dest = '\0';
        if (*++cp == ' ')
        {
            cp++;
        }
    }
    else
    {
        prefixes[0] = '\0';
    }

    bool any_val_OK{};
    char tmpbuf[80];
    {
        char *dest = tmpbuf;
        while (*cp)
        {
            if (*cp == '/')
            {
                *dest++ = '\0';
                cp++;
            }
            else if (*cp == '<')
            {
                do
                {
                    *dest++ = *cp;
                } while (*cp++ != '>');
                any_val_OK = true; // flag that '<' was found
            }
            else
            {
                *dest++ = *cp++;
            }
        }
        cp = dest;
        *dest++ = '\0';
        *dest = '\0';
        std::strcpy(g_buf,value);
    }

    bool  value_changed;
    int   number_was = -1;
    char *prefix = nullptr;
reask_in_choice:
    int len = std::strlen(g_buf);
    char *bp = g_buf;
    if (*prefixes != '\0')
    {
        const char *start = prefix;
        for (prefix = prefixes; *prefix; prefix += std::strlen(prefix))
        {
            if (*prefix == *g_buf)
            {
                break;
            }
        }
        if (*prefix)
        {
            bp = skip_ne(g_buf, ' ');
            bp = skip_eq(bp, ' ');
        }
        else
        {
            prefix = nullptr;
        }
        value_changed = prefix != start;
    }
    else
    {
        prefix = nullptr;
        value_changed = false;
    }
    char *s = cp;
    while (true)
    {
        cp += std::strlen(cp) + 1;
        if (!*cp)
        {
            cp = tmpbuf;
        }
        if (*cp == '<' && (*g_buf == '<' || cp[1] != '#' || std::isdigit(*g_buf) || !*s))
        {
            prefix = nullptr;
            break;
        }
        if (s == cp)
        {
            if (!value_changed)
            {
                if (prefix)
                {
                    prefix = nullptr;
                }
                else
                {
                    dingaling();
                }
            }
            break;
        }
        if (!*bp || !std::strncmp(cp,bp,any_val_OK ? len : 1))
        {
            break;
        }
    }

    if (*cp == '<')
    {
        if (*g_buf == '<' || cp[1] == '#')
        {
            if (number_was >= 0)
            {
                std::sprintf(g_buf, "%d", number_was);
            }
            else
            {
                s = skip_digits(g_buf);
                *s = '\0';
            }
        }
    }
    else
    {
        if (prefix)
        {
            std::sprintf(g_buf, "%s ", prefix);
            std::strcat(g_buf,cp);
        }
        else
        {
            std::strcpy(g_buf,cp);
        }
    }
    s = g_buf + std::strlen(g_buf);
    carriage_return();
    erase_line(false);
    std::fputs(prompt,stdout);
    std::fputs(g_buf,stdout);
    len = std::strlen(prompt);
    number_was = -1;

reinp_in_choice:
    if ((s-g_buf) + len >= g_tc_COLS)
    {
        s_screen_is_dirty = true;
    }
    std::fflush(stdout);
    get_cmd(s);
    if (errno || *s == '\f')            // if return from stop signal
    {
        *s = '\n';
    }
    if (*s != '\n')
    {
        char ch = *s;
        if (*cp == '<' && ch != '\t' && (ch != ' ' || g_buf != s))
        {
            if (cp[1] == '#')
            {
                s = edit_buf(s, nullptr);
                if (s != g_buf)
                {
                    if (std::isdigit(s[-1]))
                    {
                        goto reinp_in_choice;
                    }
                    else
                    {
                        number_was = std::atoi(g_buf);
                    }
                }
            }
            else
            {
                s = edit_buf(s, nullptr);
                goto reinp_in_choice;
            }
        }
        *s = '\0';
        s = skip_ne(g_buf, ' ');
        if (*s == ' ')
        {
            s++;
        }
        if (is_hor_space(ch))
        {
            if (prefix)
            {
                *s = '\0';
            }
            else
            {
                *g_buf = '\0';
            }
        }
        else
        {
            char ch1 = g_buf[0];
            if (prefix)
            {
                if (ch == ch1)
                {
                    ch = *s;
                }
                else
                {
                    ch1 = ch;
                    ch = g_buf[0];
                }
            }
            std::sprintf(g_buf,"%c %c",ch == g_erase_char || ch == g_kill_char? '<' : ch, ch1);
        }
        goto reask_in_choice;
    }
    *s = '\0';

    set_mode(gmode_save,mode_save);
    return !s_screen_is_dirty;
}

int print_lines(const char *what_to_print, int hilite)
{
    for (const char *s = what_to_print; *s;)
    {
        int i = check_page_line();
        if (i)
        {
            return i;
        }
        if (hilite == STANDOUT)
        {
#ifdef NO_FIREWORKS
            if (g_erase_screen)
            {
                no_so_fire();
            }
#endif
            standout();
        }
        else if (hilite == UNDERLINE)
        {
#ifdef NO_FIREWORKS
            if (g_erase_screen)
            {
                no_ul_fire();
            }
#endif
            underline();
        }
        for (i = 0; *s && i < g_tc_COLS;)
        {
            if (at_norm_char(s))
            {
                // TODO: make this const friendly
                i += put_char_adv(const_cast<char**>(&s), true);
            }
            else if (*s == '\t')
            {
                std::putchar(*s);
                s++;
                i = ((i+8) & ~7);
            }
            else if (*s == '\n')
            {
                s++;
                i = 32000;
            }
            else
            {
                std::putchar('^');
                std::putchar(*s + 64);
                s++;
                i += 2;
            }
        }
        if (i)
        {
            if (hilite == STANDOUT)
            {
                un_standout();
            }
            else if (hilite == UNDERLINE)
            {
                un_underline();
            }
            if (g_tc_AM && i == g_tc_COLS)
            {
                std::fflush(stdout);
            }
            else
            {
                newline();
            }
        }
    }
    return 0;
}

int check_page_line()
{
    if (g_page_line < 0)
    {
        return -1;
    }
    if (g_page_line >= g_tc_LINES || g_int_count)
    {
        int cmd = -1;
        if (g_int_count || (cmd = get_anything()))
        {
            g_page_line = -1;           // disable further printing
            if (cmd > 0)
            {
                push_char(cmd);
            }
            return cmd;
        }
    }
    g_page_line++;
    return 0;
}

void page_start()
{
    g_page_line = 1;
    if (g_erase_screen)
    {
        clear();
    }
    else
    {
        newline();
    }
}

void error_msg(const char *str)
{
    if (g_general_mode == GM_SELECTOR)
    {
        if (str != g_msg)
        {
            std::strcpy(g_msg,str);
        }
        g_error_occurred = true;
    }
    else if (*str)
    {
        std::printf("\n%s\n", str);
        term_down(2);
    }
}

void warn_msg(const char *str)
{
    if (g_general_mode != GM_SELECTOR)
    {
        std::printf("\n%s\n", str);
        term_down(2);
        pad(g_just_a_sec/3);
    }
}

void pad(int num)
{
    for (int i = num; i; i--)
    {
        std::putchar(s_tc_PC);
    }
    std::fflush(stdout);
}

// echo the command just typed

void print_cmd()
{
    if (g_verify && g_buf[1] == FINISH_CMD)
    {
        if (!at_norm_char(g_buf))
        {
            std::putchar('^');
            std::putchar((*g_buf & 0x7F) | 64);
            backspace();
            backspace();
        }
        else
        {
            std::putchar(*g_buf);
            backspace();
        }
        std::fflush(stdout);
    }
}

void rubout()
{
    backspace();                        // do the old backspace,
    std::putchar(' ');                  // space,
    backspace();                        // backspace trick
}

void reprint()
{
    std::fputs("^R\n",stdout);
    term_down(1);
    for (char *s = g_buf; *s; s++)
    {
        echo_char(*s);
    }
    s_screen_is_dirty = true;
}

void erase_line(bool to_eos)
{
    carriage_return();
    if (to_eos)
    {
        clear_rest();
    }
    else
    {
        erase_eol();
    }
    carriage_return();          // Resets kernel's tab column counter to 0
    std::fflush(stdout);
}

void clear()
{
    g_term_line = 0;
    g_term_col = 0;
    g_fire_is_out = 0;
    if (s_tc_CL)
    {
        tputs(s_tc_CL,g_tc_LINES,put_char);
    }
    else if (g_tc_CD)
    {
        home_cursor();
        tputs(g_tc_CD,g_tc_LINES,put_char);
    }
    else
    {
        for (int i = 0; i < g_tc_LINES; i++)
        {
            put_char('\n');
        }
        home_cursor();
    }
    tputs(g_tc_CR,1,put_char);
}

void home_cursor()
{
    if (!*g_tc_HO)              // no home sequence?
    {
        if (!*g_tc_CM)                  // no cursor motion either?
        {
            std::fputs("\n\n\n", stdout);
            term_down(3);
            return;             // forget it.
        }
        tputs(tgoto(g_tc_CM, 0, 0), 1, put_char); // go to home via CM
    }
    else                        // we have home sequence
    {
        tputs(g_tc_HO, 1, put_char);// home via HO
    }
    carriage_return();  // Resets kernel's tab column counter to 0
    g_term_line = 0;
    g_term_col = 0;
}

void goto_xy(int to_col, int to_line)
{
    char*str;
    int  cmcost;

    if (g_term_col == to_col && g_term_line == to_line)
    {
        return;
    }
    if (*g_tc_CM && !g_muck_up_clear)
    {
        str = tgoto(g_tc_CM,to_col,to_line);
        cmcost = std::strlen(str);
    }
    else
    {
        str = nullptr;
        cmcost = 9999;
    }

    int ycost = (to_line - g_term_line);
    if (ycost < 0)
    {
        ycost = (s_up_cost? -ycost * s_up_cost : 7777);
    }
    else if (ycost > 0)
    {
        g_term_col = 0;
    }

    int xcost = (to_col - g_term_col);
    if (xcost < 0)
    {
        if (!to_col && ycost + 1 < cmcost)
        {
            carriage_return();
            xcost = 0;
        }
        else
        {
            xcost = -xcost * s_left_cost;
        }
    }
    else if (xcost > 0 && cmcost < 9999)
    {
        xcost = 9999;
    }

    if (cmcost <= xcost + ycost)
    {
        tputs(str,1,put_char);
        g_term_line = to_line;
        g_term_col = to_col;
        return;
    }

    if (ycost == 7777)
    {
        home_cursor();
    }

    if (to_line >= g_term_line)
    {
        while (g_term_line < to_line)
        {
            newline();
        }
    }
    else
    {
        while (g_term_line > to_line)
        {
            up_line();
        }
    }

    if (to_col >= g_term_col)
    {
        while (g_term_col < to_col)
        {
            g_term_col++;
            std::putchar(' ');
        }
    }
    else
    {
        while (g_term_col > to_col)
        {
            g_term_col--;
            backspace();
        }
    }
}

static void line_col_calcs()
{
    if (g_tc_LINES > 0)                 // is this a crt?
    {
        if (!g_init_lines || !g_option_def_vals[OI_INITIAL_ARTICLE_LINES])
        {
            // no -i or unreasonable value for g_initlines
            if (s_out_speed >= B9600)    // whole page at >= 9600 baud
            {
                g_init_lines = ArticleLine{g_tc_LINES};
            }
            else if (s_out_speed >= B4800)       // 16 lines at 4800
            {
                g_init_lines = ArticleLine{16};
            }
            else                        // otherwise just header
            {
                g_init_lines = ArticleLine{8};
            }
        }
        // Check for g_initlines bigger than the screen and fix it!
        g_init_lines = std::min(g_init_lines, ArticleLine{g_tc_LINES});
    }
    else                                // not a crt
    {
        g_tc_LINES = 30000;             // so don't page
        s_tc_CL = "\n\n";                       // put a couple of lines between
        if (!g_init_lines || !g_option_def_vals[OI_INITIAL_ARTICLE_LINES])
        {
            g_init_lines = ArticleLine{8};            // make g_initlines reasonable
        }
    }
    if (g_tc_COLS <= 0)
    {
        g_tc_COLS = 80;
    }
    s_resize_win();     // let various parts know
}

#ifdef SIGWINCH
Signal_t winch_catcher(int dummy)
{
    // Reset signal in case of System V dain bramage
    sigset(SIGWINCH, winch_catcher);

    // Come here if window size change signal received
#ifdef TIOCGWINSZ
    {
        struct winsize ws;
        if (ioctl(0, TIOCGWINSZ, &ws) >= 0 && ws.ws_row > 0 && ws.ws_col > 0)
        {
            if (g_tc_LINES != ws.ws_row || g_tc_COLS != ws.ws_col)
            {
                g_tc_LINES = ws.ws_row;
                g_tc_COLS = ws.ws_col;
                line_col_calcs();
                std::sprintf(s_lines_export, "%d", g_tc_LINES);
                std::sprintf(s_cols_export, "%d", g_tc_COLS);
                if (g_general_mode == 's' || g_mode == 'a' || g_mode == 'p')
                {
                    force_me("\f");      // cause a refresh
                                        // (defined only if TIOCSTI defined)
                }
            }
        }
    }
#else
    // Well, if SIGWINCH is defined, but TIOCGWINSZ isn't, there's
    // almost certainly something wrong.  Figure it out for yourself,
    // because I don't know how to deal with it :-)
    ERROR!
#endif
}
#endif

void termlib_init()
{
#ifdef USETITE
    if (s_tc_TI && *s_tc_TI)
    {
        tputs(s_tc_TI,1,putchr);
        std::fflush(stdout);
    }
#endif
#ifdef USEKSKE
    if (s_tc_KS && *s_tc_KS)
    {
        tputs(s_tc_KS,1,putchr);
        std::fflush(stdout);
    }
#endif
    g_term_line = g_tc_LINES-1;
    g_term_col = 0;
    g_term_scrolled = g_tc_LINES;
}

void termlib_reset()
{
#ifdef USETITE
    if (s_tc_TE && *s_tc_TE)
    {
        tputs(s_tc_TE,1,putchr);
        std::fflush(stdout);
    }
#endif
#ifdef USEKSKE
    if (s_tc_KE && *s_tc_KE)
    {
        tputs(s_tc_KE,1,putchr);
        std::fflush(stdout);
    }
#endif
}

void xmouse_init(const char *progname)
{
    if (!g_can_home || !g_use_threads)
    {
        return;
    }
    char *s = get_val("XTERMMOUSE");
    if (s && *s)
    {
        interp(g_msg, sizeof g_msg, s);
        set_option(OI_USE_MOUSE, g_msg);
    }
    else if (progname[std::strlen(progname) - 1] == 'x')
    {
        // an 'x' at the end means enable Xterm mouse tracking
        set_option(OI_USE_MOUSE, "y");
    }
}

void xmouse_check()
{
    g_mouse_bar_cnt = 0;
    if (g_use_mouse)
    {
        bool turn_it_on;
        MinorMode mmode = g_mode;
        if (g_general_mode == GM_PROMPT)
        {
            turn_it_on = true;
            mmode = MM_NONE;
        }
        else if (g_general_mode == GM_INPUT || g_general_mode == GM_PROMPT ||
                 (g_muck_up_clear && g_general_mode != GM_SELECTOR))
        {
            turn_it_on = false;
        }
        else
        {
            interp(g_msg, sizeof g_msg, g_mouse_modes);
            turn_it_on = (std::strchr(g_msg, static_cast<char>(g_mode)) != nullptr);
        }
        if (turn_it_on)
        {
            switch (mmode)
            {
            case MM_NEWSRC_SELECTOR:
                s_mouse_bar_btns = g_newsrc_sel_btns;
                g_mouse_bar_cnt = g_newsrc_sel_btn_cnt;
                break;

            case MM_ADD_GROUP_SELECTOR:
                s_mouse_bar_btns = g_add_sel_btns;
                g_mouse_bar_cnt = g_add_sel_btn_cnt;
                break;

            case MM_OPTION_SELECTOR:
                s_mouse_bar_btns = g_option_sel_btns;
                g_mouse_bar_cnt = g_option_sel_btn_cnt;
                break;

            case MM_THREAD_SELECTOR:
                s_mouse_bar_btns = g_news_sel_btns;
                g_mouse_bar_cnt = g_news_sel_btn_cnt;
                break;

            case MM_NEWSGROUP_SELECTOR:
                s_mouse_bar_btns = g_newsgroup_sel_btns;
                g_mouse_bar_cnt = g_newsgroup_sel_btn_cnt;
                break;

            case MM_ARTICLE:  case MM_PAGER:
                s_mouse_bar_btns = g_art_pager_btns;
                g_mouse_bar_cnt = g_art_pager_btn_cnt;
                break;

            case MM_UNIVERSAL:
                s_mouse_bar_btns = g_univ_sel_btns;
                g_mouse_bar_cnt = g_univ_sel_btn_cnt;
                break;

            default:
                s_mouse_bar_btns = "";
                // g_mousebar_cnt = 0;
                break;
            }
            char *s = s_mouse_bar_btns;
            g_mouse_bar_width = 0;
            for (int i = 0; i < g_mouse_bar_cnt; i++)
            {
                int j = std::strlen(s);
                if (*s == '[')
                {
                    g_mouse_bar_width += j;
                    s += j + 1;
                    j = std::strlen(s);
                }
                else
                {
                    g_mouse_bar_width += (j < 2? 3+1 : (j == 2? 4+1
                                                     : (j < 5? j: 5+1)));
                }
                s += j + 1;
            }
            xmouse_on();
        }
        else
        {
            xmouse_off();
        }
        g_mouse_is_down = false;
    }
}

void xmouse_on()
{
    if (!s_xmouse_is_on)
    {
        // save old highlight mouse tracking and enable mouse tracking
        std::fputs("\033[?1001s\033[?1000h",stdout);
        std::fflush(stdout);
        s_xmouse_is_on = true;
    }
}

void xmouse_off()
{
    if (s_xmouse_is_on)
    {
        // disable mouse tracking and restore old highlight mouse tracking
        std::fputs("\033[?1000l\033[?1001r",stdout);
        std::fflush(stdout);
        s_xmouse_is_on = false;
    }
}

void draw_mouse_bar(int limit, bool restore_cursor)
{
    int save_col = g_term_col;
    int save_line = g_term_line;

    g_mouse_bar_width = 0;
    if (g_mouse_bar_cnt == 0)
    {
        return;
    }

    char *s = s_mouse_bar_btns;
    char *t = g_msg;
    for (int i = 0; i < g_mouse_bar_cnt; i++)
    {
        if (*s == '[')
        {
            while (*++s)
            {
                *t++ = *s;
            }
            s++;
        }
        else
        {
            switch (std::strlen(s))
            {
            case 0:
                *t++ = ' ';
                // FALL THROUGH

            case 1:  case 2:
                *t++ = ' ';
                while (*s)
                {
                    *t++ = *s++;
                }
                *t++ = ' ';
                break;

            case 3:  case 4:
                while (*s)
                {
                    *t++ = *s++;
                }
                break;

            default:
                std::strncpy(t, s, 5);
                t += 5;
                break;
            }
        }
        s += std::strlen(s) + 1;
        *t++ = '\0';
    }
    g_mouse_bar_width = t - g_msg;
    s_mouse_bar_start = 0;

    s = g_msg;
    while (g_mouse_bar_width > limit)
    {
        int len = std::strlen(s) + 1;
        s += len;
        g_mouse_bar_width -= len;
        s_mouse_bar_start++;
    }

    goto_xy(g_tc_COLS - g_mouse_bar_width - 1, g_tc_LINES-1);
    for (int i = s_mouse_bar_start; i < g_mouse_bar_cnt; i++)
    {
        std::putchar(' ');
        color_string(COLOR_MOUSE,s);
        s += std::strlen(s) + 1;
    }
    g_term_col = g_tc_COLS-1;
    if (restore_cursor)
    {
        goto_xy(save_col, save_line);
    }
}

static void mouse_input(const char *cp)
{
    static int last_btn;
    static int last_x;
    static int last_y;

    if (cp[2] < ' ' || cp[2] > ' '+3)
    {
        return;
    }
    int btn = (cp[2] & 3);
    int x = cp[1] - 33;
    int y = cp[0] - 33;

    if (btn != 3)
    {
        static double last_time = 0.;
        double this_time = current_time();
        if (last_btn == btn && last_y == y && this_time - last_time <= 0.75 //
            && (last_x == x || last_x == x - 1 || last_x == x + 1))
        {
            btn |= 4;
        }
        last_time = this_time;
        last_btn = (btn & 3);
        last_x = x;
        last_y = y;
        g_mouse_is_down = true;
    }
    else
    {
        if (!g_mouse_is_down)
        {
            return;
        }
        g_mouse_is_down = false;
    }

    if (g_general_mode == GM_SELECTOR)
    {
        selector_mouse(btn, x,y, last_btn, last_x,last_y);
    }
    else if (g_mode == MM_ARTICLE || g_mode == MM_PAGER)
    {
        pager_mouse(btn, x,y, last_btn, last_x,last_y);
    }
    else
    {
        push_char(' ');
    }
}

bool check_mouse_bar(int btn, int x, int y, int btn_clk, int x_clk, int y_clk)
{
    char*s = s_mouse_bar_btns;
    int  col = g_tc_COLS - g_mouse_bar_width;

    if (g_mouse_bar_width != 0 && btn_clk == 0 && y_clk == g_tc_LINES - 1 //
        && (x_clk -= col - 1) > 0)
    {
        x -= col-1;
        for (int i = 0; i < s_mouse_bar_start; i++)
        {
            if (*s == '[')
            {
                s += std::strlen(s) + 1;
            }
            s += std::strlen(s) + 1;
        }
        while (true)
        {
            int j;
            int i = std::strlen(s);
            char *t = s;
            if (*s == '[')
            {
                s += i + 1;
                j = 0;
                while (*++t == ' ')
                {
                    j++;
                }
            }
            else if (i <= 2)
            {
                i = 3 + (i==2) + 1;
                j = 1;
            }
            else
            {
                i = (i < 5? i+1 : 5+1);
                j = 0;
            }
            if (x_clk < i)
            {
                int tcol = g_term_col;
                int tline = g_term_line;
                goto_xy(col+j,g_tc_LINES-1);
                if (btn == 3)
                {
                    color_object(COLOR_MOUSE, true);
                }
                if (s == t)
                {
                    for (int k = 0; k < 5 && *t; k++, t++)
                    {
                        g_term_col++;
                        std::putchar(*t);
                    }
                }
                else
                {
                    for (; *t && *t != ' '; t++)
                    {
                        g_term_col++;
                        std::putchar(*t);
                    }
                }
                if (btn == 3)
                {
                    color_pop();        // of COLOR_MOUSE
                }
                goto_xy(tcol,tline);
                std::fflush(stdout);
                if (btn == 3 && x > 0 && x < i)
                {
                    push_string(s,0);
                }
                break;
            }
            if (!(x_clk -= i))
            {
                break;
            }
            x -= i;
            col += i;
            s += std::strlen(s) + 1;
        }
        return true;
    }
    return false;
}

static int s_tc_string_cnt{};

struct ColorCapability
{
    char *capability; // name of capability, e.g. "foreground red"
    char *string;     // escape sequence, e.g. "\033[31m"
};
static ColorCapability s_tc_strings[TC_STRINGS];

// Parse a line from the [termcap] section of trnrc.
void add_tc_string(const char *capability, const char *string)
{
    int i;

    for (i = 0; i < s_tc_string_cnt; i++)
    {
        if (!std::strcmp(capability, s_tc_strings[i].capability))
        {
            std::free(s_tc_strings[i].string);
            break;
        }
    }
    if (i == s_tc_string_cnt)
    {
        if (s_tc_string_cnt == TC_STRINGS)
        {
            std::fprintf(stderr,"trn: too many colors in [termcap] section (max is %d).\n",
                    TC_STRINGS);
            finalize(1);
        }
        s_tc_string_cnt++;
        s_tc_strings[i].capability = save_str(capability);
    }

    s_tc_strings[i].string = save_str(string);
}

// Return the named termcap color capability's string.
char *tc_color_capability(const char *capability)
{
    for (int c = 0; c < s_tc_string_cnt; c++)
    {
        if (!std::strcmp(s_tc_strings[c].capability,capability))
        {
            return s_tc_strings[c].string;
        }
    }
    return nullptr;
}

#ifdef MSDOS
int tputs(char *str, int num, int (*func)(int))
{
    std::printf(str,num);
    return 0;
}
#endif

#ifdef MSDOS
char *tgoto(char *str, int x, int y)
{
    static char gbuf[32];
    std::sprintf(gbuf,str,y+1,x+1);
    return gbuf;
}
#endif
