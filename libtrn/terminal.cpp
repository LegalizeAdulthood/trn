/* terminal.cpp
 * vi: set sw=4 ts=8 ai sm noet :
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/terminal.h>

#include <config/common.h>
#include <config/env.h>
#include <trn/art.h>
#include <trn/cache.h>
#include <trn/color.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/help.h>
#include <trn/init.h>
#include <trn/intrp.h>
#include <trn/ng.h>
#include <trn/opt.h>
#include <trn/rt-select.h>
#include <trn/sdisp.h>
#include <trn/size_cast.h>
#include <trn/string-algos.h>
#include <trn/trn.h>
#include <trn/univ.h>
#include <trn/utf.h>
#include <trn/util.h>
#include <util/util2.h>

#include <fmt/format.h>
#include <fmt/printf.h>

#ifdef MSDOS
#include <conio.h>
#endif

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

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
MouseButtonList g_univ_sel_btns;
MouseButtonList g_newsrc_sel_btns;
MouseButtonList g_add_sel_btns;
MouseButtonList g_newsgroup_sel_btns;
MouseButtonList g_news_sel_btns;
MouseButtonList g_option_sel_btns;
MouseButtonList g_art_pager_btns;
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
std::string   g_mouse_modes{"acjlptwvK"};         //
MinorMode     g_mode{MM_INITIALIZING};            // current state of trn
GeneralMode   g_general_mode{GM_INIT};            // general mode of trn

#ifdef HAS_TERMLIB
bool  g_tc_GT{};              // hardware tabs
const char *g_tc_BC{};        // backspace character
const char *g_tc_UP{};        // move cursor up one line
const char *g_tc_CR{};        // get to left margin, somehow
const char *g_tc_VB{};        // visible bell
const char *g_tc_CE{};        // clear to end of line
const char *g_tc_CM{};        // cursor motion
const char *g_tc_HO{};        // home cursor
const char *g_tc_IL{};        // insert line
const char *g_tc_CD{};        // clear to end of display
const char *g_tc_SO{};        // begin standout mode
const char *g_tc_SE{};        // end standout mode
const char *g_tc_US{};        // start underline mode
const char *g_tc_UE{};        // end underline mode
const char *g_tc_UC{};        // underline a character, if that's how it's done
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

static const MouseButtonList *s_mouse_bar_btns{};
static int   s_mouse_bar_start{};
static bool  s_xmouse_is_on{};
static const char *s_tc_CL{}; // home and clear screen
static const char *s_tc_TI{}; // initialize terminal
static const char *s_tc_TE{}; // reset terminal
static const char *s_tc_KS{}; // enter `keypad transmit' mode
static const char *s_tc_KE{}; // exit `keypad transmit' mode
static char  s_tc_PC{}; // pad character for use by tputs()
#ifndef MSDOS
static std::string s_tc_CR_fallback;
#endif
#ifdef _POSIX_SOURCE
static speed_t s_out_speed{}; // terminal output speed,
#else
static long s_out_speed{}; // for use by tputs()
#endif
#endif

struct KeyMap
{
    char        km_type[128]{};
    KeyMap    *km_km[128]{};
    std::string km_str[128];
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
static int     s_left_cost{};
static int     s_up_cost{};
static bool    s_got_a_char{}; // true if we got a char since eating

#ifdef SIGALRM
static Signal_t alarm_catcher(int signo);
#endif
#ifdef PENDING
#if !defined(FIONREAD) && !defined(HAS_RDCHK) && !defined(MSDOS)
static int circfill();
#endif
#endif
static char   *edit_buf(char *s, const char *cmd);
static void    install_macro(std::string_view sequence, std::string_view definition,
                             bool report_overrides);
static void    mac_init(char *tcbuf);
static KeyMap *new_key_map();
static void    reprint();
static void    show_key_map(KeyMap *curmap, std::string &prefix);
static int     echo_char(char_int ch);
static void    line_col_calcs();
static void    mouse_input(const char *cp);
static void    xmouse_on();

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
inline const char *Tgetstr(const char *key)
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
    const char *s;
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
    const std::string term = get_env_var("TERM", "dumb");
    status = tgetent(tcbuf, term.c_str());      // get termcap entry
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
    s_tc_CR_fallback.clear();
    g_tc_CR = Tgetstr("cr");
    if (!*g_tc_CR)
    {
        if (tgetflag("nc") && *g_tc_UP)
        {
            s_tc_CR_fallback = fmt::format("{}\r", g_tc_UP);
            g_tc_CR = s_tc_CR_fallback.c_str();
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
    set_env_var("LINES", std::to_string(g_tc_LINES));
    set_env_var("COLUMNS", std::to_string(g_tc_COLS));

    mac_init(tcbuf);
}

void set_macro(std::string_view seq, std::string_view def)
{
    std::string seq_text{seq};
    std::string def_text{def};

    if (!def_text.empty() && def_text.back() == '\n')
    {
        def_text.pop_back();
    }
    install_macro(seq_text, def_text, false);
    // check for common (?) brain damage: ku/kd/etc sequence may be the
    // cursor move sequence instead of the input sequence.
    // (This happens on the local xterm definitions.)
    // Try to recognize and adjust for this case.
    //
    if (seq_text.size() > 2 && seq_text[0] == '\033' && seq_text[1] == '[')
    {
        std::string alt_seq{seq_text};
        alt_seq[1] = 'O';
        install_macro(alt_seq, def_text, false);
    }
    if (seq_text.size() > 2 && seq_text[0] == '\033' && seq_text[1] == 'O')
    {
        std::string alt_seq{seq_text};
        alt_seq[1] = '[';
        install_macro(alt_seq, def_text, false);
    }
}

static const char *s_up[] = {
    "^@",
    // '(' at article or pager, '[' in thread sel, 'p' otherwise
    "%(%m=[ap]?\\(:%(%m=t?[:p))",
    // '(' at article or pager, '[' in thread sel, 'p' otherwise
    "%(%m=[ap]?\\(:%(%m=t?[:p))"
};
static const char *s_down[] = {
    "^@",
    // ')' at article or pager, ']' in thread sel, 'n' otherwise
    "%(%m=[ap]?\\):%(%m=t?]:n))",
    // ')' at article or pager, ']' in thread sel, 'n' otherwise
    "%(%m=[ap]?\\):%(%m=t?]:n))"
};
static const char *s_left[] = {
    "^@",
    // '[' at article or pager, 'Q' otherwise
    "%(%m=[ap]?\\[:Q)",
    // '[' at article or pager, '<' otherwise
    "%(%m=[ap]?\\[:<)"
};
static const char *s_right[] = {
    "^@",
    // ']' at article or pager, CR otherwise
    "%(%m=[ap]?\\]:^j)",
    // CR at newsgroups, ']' at article or pager, '>' otherwise
    "%(%m=n?^j:%(%m=[ap]?\\]:>))"
};

// Turn the arrow keys into macros that do some basic trn functions.
// Code provided by Clifford Adams.
//
void arrow_macros()
{
#ifdef HAS_TERMLIB

    // If arrows are defined as single keys, we probably don't
    // want to redefine them.  (The tvi912c defines kl as ^H)
    //
#ifdef MSDOS
    const char *seq = "\035\110";
#else
    const char *seq = Tgetstr("ku"); // up
#endif
    if ((int) std::strlen(seq) > 1)
    {
        set_macro(seq, s_up[g_auto_arrow_macros]);
    }

#ifdef MSDOS
    seq = "\035\120";
#else
    seq = Tgetstr("kd"); // down
#endif
    if ((int) std::strlen(seq) > 1)
    {
        set_macro(seq, s_down[g_auto_arrow_macros]);
    }

#ifdef MSDOS
    seq = "\035\113";
#else
    seq = Tgetstr("kl"); // left
#endif
    if ((int) std::strlen(seq) > 1)
    {
        set_macro(seq, s_left[g_auto_arrow_macros]);
    }

#ifdef MSDOS
    seq = "\035\115";
#else
    seq = Tgetstr("kr"); // right
#endif
    if ((int) std::strlen(seq) > 1)
    {
        set_macro(seq, s_right[g_auto_arrow_macros]);
    }

    if (*seq == '\033')
    {
        set_macro("\033\033", "\033");
    }
#endif
}

static void mac_init(char *tcbuf)
{
    if (g_auto_arrow_macros)
    {
        arrow_macros();
    }
    std::FILE *macros;
    if (!g_use_threads || (macros = std::fopen(file_exp(get_env_var("TRNMACRO", TRNMACRO)).c_str(), "r")) == nullptr)
    {
        macros = std::fopen(file_exp(get_env_var("RNMACRO", RNMACRO)).c_str(), "r");
    }
    if (macros)
    {
        while (std::fgets(tcbuf,TCBUF_SIZE,macros) != nullptr)
        {
            mac_line(tcbuf);
        }
        std::fclose(macros);
    }
}

void mac_line(char *line)
{
    if (s_top_map == nullptr)
    {
        s_top_map = new_key_map();
    }
    std::string_view pattern{line};
    if (pattern.empty() || pattern.front() == '#' || pattern.front() == '\n')
    {
        return;
    }
    if (pattern.back() == '\n')
    {
        pattern.remove_suffix(1);
    }
    std::string_view  cursor{pattern};
    const std::string macro_sequence = do_interp(cursor, " \t", {});
    if (cursor.empty())
    {
        return;
    }
    cursor.remove_prefix(std::min(cursor.find_first_not_of(" \t"), cursor.size()));
    install_macro(macro_sequence, cursor, true);
}

static void install_macro(std::string_view sequence, std::string_view definition,
                          bool report_overrides)
{
    KeyMap*     curmap;
    int         garbage = 0;
    static constexpr char OVERRIDE[] = "\nkeymap overrides string\n";

    if (s_top_map == nullptr)
    {
        s_top_map = new_key_map();
    }
    curmap=s_top_map;
    for (std::size_t position = 0; position < sequence.size(); position++)
    {
        int ch = static_cast<unsigned char>(sequence[position]) & 0177;
        if (position + 2 < sequence.size() && sequence[position + 1] == '+'
            && std::isdigit(static_cast<unsigned char>(sequence[position + 2])))
        {
            position += 2;
            garbage = (static_cast<unsigned char>(sequence[position]) & KM_GMASK) << KM_GSHIFT;
        }
        else
        {
            garbage = 0;
        }
        if (position + 1 < sequence.size())
        {
            if ((curmap->km_type[ch] & KM_TMASK) == KM_STRING)
            {
                if (report_overrides)
                {
                    std::fputs(OVERRIDE,stdout);
                    term_down(2);
                }
                curmap->km_str[ch].clear();
            }
            curmap->km_type[ch] = KM_KEYMAP + garbage;
            if (curmap->km_km[ch] == nullptr)
            {
                curmap->km_km[ch] = new_key_map();
            }
            curmap = curmap->km_km[ch];
        }
        else
        {
            if (report_overrides && (curmap->km_type[ch] & KM_TMASK) == KM_KEYMAP)
            {
                std::fputs(OVERRIDE,stdout);
                term_down(2);
            }
            else
            {
                if ((curmap->km_type[ch] & KM_TMASK) == KM_KEYMAP)
                {
                    curmap->km_km[ch] = nullptr;
                }
                curmap->km_type[ch] = KM_STRING + garbage;
                if (definition.empty())
                {
                    curmap->km_str[ch].clear();
                }
                else
                {
                    curmap->km_str[ch].assign(definition.data(), definition.size());
                }
            }
        }
    }
}

static KeyMap *new_key_map()
{
#ifndef lint
    return new KeyMap{};
#else
    return nullptr;
#endif // lint
}

void show_macros()
{
    if (s_top_map != nullptr)
    {
        print_lines("Macros:\n", STANDOUT);
        std::string prefix;
        prefix.reserve(64);
        show_key_map(s_top_map, prefix);
    }
    else
    {
        print_lines("No macros defined.\n", NO_MARKING);
    }
}

static void show_key_map(KeyMap *curmap, std::string &prefix)
{
    const std::size_t prefix_size = prefix.size();

    for (int i = 0; i < 128; i++)
    {
        int kt = curmap->km_type[i];
        if (kt != 0)
        {
            prefix.resize(prefix_size);
            if (i < ' ')
            {
                prefix += '^';
                prefix += static_cast<char>(i + 64);
            }
            else if (i == ' ')
            {
                prefix += "\\040";
            }
            else if (i == 127)
            {
                prefix += "^?";
            }
            else
            {
                prefix += static_cast<char>(i);
            }
            if ((kt >> KM_GSHIFT) & KM_GMASK)
            {
                prefix += fmt::format("+{}", (kt >> KM_GSHIFT) & KM_GMASK);
            }
            switch (kt & KM_TMASK)
            {
            case KM_NOTHING:
            {
                const std::string line{fmt::format("{}   {}\n", prefix, static_cast<char>(i))};
                print_lines(line.c_str(), NO_MARKING);
                break;
            }

            case KM_KEYMAP:
                show_key_map(curmap->km_km[i], prefix);
                break;

            case KM_STRING:
            {
                const std::string line{fmt::format("{}   {}\n", prefix, curmap->km_str[i])};
                print_lines(line.c_str(), NO_MARKING);
                break;
            }

            case KM_BOGUS:
            {
                const std::string line{fmt::format("{}   BOGUS\n", prefix)};
                print_lines(line.c_str(), STANDOUT);
                break;
            }
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

static char *edit_buf(char *s, const char *cmd)
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
        std::string substitution{"% "};

        read_tty(substitution.data() + 1,1);
#ifdef RAWONLY
        substitution[1] &= 0177;
#endif
        if (substitution[1] == 'h')
        {
            (void) help_subs();
            *s = '\0';
            reprint();
        }
        else if (substitution[1] == '\033')
        {
            *s = '\0';
            const std::string cpybuf{g_buf};
            interp_search(g_buf, sizeof g_buf, cpybuf.c_str(), cmd);
            s = g_buf + std::strlen(g_buf);
            reprint();
        }
        else
        {
            interp_search(s, sizeof g_buf - (s-g_buf), substitution.c_str(), cmd);
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
                curmap = curmap->km_km[lc];
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
static Signal_t alarm_catcher(int signo)
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
static int circfill()
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
            curmap = curmap->km_km[g_last_char];
            TRN_ASSERT(curmap != nullptr);
            break;

        case KM_STRING:               // a string?
            push_string(curmap->km_str[g_last_char].c_str(),0200);
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

void push_string(const char *str, char_int bits)
{
    TRN_ASSERT(str != nullptr);
    std::string expanded(PUSH_SIZE, '\0');
    interp(expanded.data(), static_cast<int>(expanded.size()), str);
    const std::size_t expanded_end = expanded.find('\0');
    if (expanded_end != std::string::npos)
    {
        expanded.resize(expanded_end);
    }
    for (std::string::const_reverse_iterator ch = expanded.rbegin(); ch != expanded.rend(); ++ch)
    {
        push_char(*ch ^ bits);
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

bool in_choice(std::string_view prompt, std::string_view value, std::string_view choices, MinorMode newmode)
{
    MinorMode   mode_save = g_mode;
    GeneralMode gmode_save = g_general_mode;

    const auto set_buffer = [](std::string_view text)
    {
        TRN_ASSERT(text.size() <= LINE_BUF_LEN);
        std::copy(text.begin(), text.end(), g_buf);
        g_buf[text.size()] = '\0';
    };

    unflush_output(); // disable any ^O in effect
    eat_typeahead();
    set_mode(GM_CHOICE, newmode);
    s_screen_is_dirty = false;

    std::vector<std::string_view> prefixes;
    if (!choices.empty() && choices.front() == '[')
    {
        const std::size_t prefix_end = choices.find(']');
        TRN_ASSERT(prefix_end != std::string_view::npos);
        std::string_view prefix_text = choices.substr(1, prefix_end - 1);
        while (true)
        {
            const std::size_t separator = prefix_text.find('/');
            prefixes.push_back(prefix_text.substr(0, separator));
            if (separator == std::string_view::npos)
            {
                break;
            }
            prefix_text.remove_prefix(separator + 1);
        }
        choices.remove_prefix(prefix_end + 1);
        if (!choices.empty() && choices.front() == ' ')
        {
            choices.remove_prefix(1);
        }
    }

    bool                          any_value_allowed{};
    std::vector<std::string_view> choice_values;
    std::size_t                   choice_start{};
    for (std::size_t i = 0; i < choices.size(); ++i)
    {
        if (choices[i] == '<')
        {
            any_value_allowed = true;
            const std::size_t value_end = choices.find('>', i + 1);
            TRN_ASSERT(value_end != std::string_view::npos);
            i = value_end;
        }
        else if (choices[i] == '/')
        {
            choice_values.push_back(choices.substr(choice_start, i - choice_start));
            choice_start = i + 1;
        }
    }
    choice_values.push_back(choices.substr(choice_start));
    set_buffer(value);

    bool        value_changed;
    int         number_was = -1;
    std::size_t prefix_index = prefixes.size();
    std::size_t choice_index = choice_values.size();
reask_in_choice:
    const std::string_view buffer{g_buf};
    std::string_view       match = buffer;
    if (!prefixes.empty())
    {
        const std::size_t previous_prefix = prefix_index;
        prefix_index = prefixes.size();
        for (std::size_t i = 0; i < prefixes.size(); ++i)
        {
            if (!prefixes[i].empty() && !buffer.empty() && prefixes[i].front() == buffer.front())
            {
                prefix_index = i;
                break;
            }
        }
        if (prefix_index != prefixes.size())
        {
            const std::size_t separator = buffer.find(' ');
            match = separator == std::string_view::npos ? std::string_view{} : buffer.substr(separator + 1);
            while (!match.empty() && match.front() == ' ')
            {
                match.remove_prefix(1);
            }
        }
        value_changed = prefix_index != previous_prefix;
    }
    else
    {
        prefix_index = prefixes.size();
        value_changed = false;
    }
    const std::size_t previous_choice = choice_index;
    while (true)
    {
        if (++choice_index >= choice_values.size())
        {
            choice_index = 0;
        }
        const std::string_view choice = choice_values[choice_index];
        if (!choice.empty() && choice.front() == '<' &&
            ((!buffer.empty() && buffer.front() == '<') || choice.size() < 2 || choice[1] != '#' ||
             (!buffer.empty() && std::isdigit(static_cast<unsigned char>(buffer.front()))) ||
             previous_choice == choice_values.size()))
        {
            prefix_index = prefixes.size();
            break;
        }
        if (previous_choice == choice_index)
        {
            if (!value_changed)
            {
                if (prefix_index != prefixes.size())
                {
                    prefix_index = prefixes.size();
                }
                else
                {
                    dingaling();
                }
            }
            break;
        }
        const std::size_t compare_size = any_value_allowed ? buffer.size() : 1;
        if (match.empty() || choice.substr(0, compare_size) == match.substr(0, compare_size))
        {
            break;
        }
    }

    const std::string_view choice = choice_values[choice_index];
    if (!choice.empty() && choice.front() == '<')
    {
        if ((!buffer.empty() && buffer.front() == '<') || (choice.size() > 1 && choice[1] == '#'))
        {
            if (number_was >= 0)
            {
                set_buffer(fmt::format("{}", number_was));
            }
            else
            {
                const std::size_t digit_count = buffer.find_first_not_of("0123456789");
                g_buf[digit_count == std::string_view::npos ? buffer.size() : digit_count] = '\0';
            }
        }
    }
    else
    {
        if (prefix_index != prefixes.size())
        {
            set_buffer(fmt::format("{} {}", prefixes[prefix_index], choice));
        }
        else
        {
            set_buffer(choice);
        }
    }
    char *input = g_buf + std::string_view{g_buf}.size();
    carriage_return();
    erase_line(false);
    fmt::print("{}{}", prompt, std::string_view{g_buf});
    number_was = -1;

reinp_in_choice:
    if ((input - g_buf) + prompt.size() >= g_tc_COLS)
    {
        s_screen_is_dirty = true;
    }
    std::fflush(stdout);
    get_cmd(input);
    if (errno || *input == '\f') // if return from stop signal
    {
        *input = '\n';
    }
    if (*input != '\n')
    {
        char ch = *input;
        if (!choice.empty() && choice.front() == '<' && ch != '\t' && (ch != ' ' || g_buf != input))
        {
            if (choice.size() > 1 && choice[1] == '#')
            {
                input = edit_buf(input, nullptr);
                if (input != g_buf)
                {
                    if (std::isdigit(static_cast<unsigned char>(input[-1])))
                    {
                        goto reinp_in_choice;
                    }
                    else
                    {
                        std::from_chars(g_buf, input, number_was);
                    }
                }
            }
            else
            {
                input = edit_buf(input, nullptr);
                goto reinp_in_choice;
            }
        }
        *input = '\0';
        const std::string_view edited_buffer{g_buf};
        const std::size_t      separator = edited_buffer.find(' ');
        char *value_start = separator == std::string_view::npos ? g_buf + edited_buffer.size() : g_buf + separator + 1;
        if (separator != std::string_view::npos)
        {
            while (*value_start == ' ')
            {
                ++value_start;
            }
        }
        if (is_hor_space(ch))
        {
            if (prefix_index != prefixes.size())
            {
                *value_start = '\0';
            }
            else
            {
                *g_buf = '\0';
            }
        }
        else
        {
            char ch1 = g_buf[0];
            if (prefix_index != prefixes.size())
            {
                if (ch == ch1)
                {
                    ch = *value_start;
                }
                else
                {
                    ch1 = ch;
                    ch = g_buf[0];
                }
            }
            set_buffer(fmt::format("{} {}", ch == g_erase_char || ch == g_kill_char ? '<' : ch, ch1));
        }
        goto reask_in_choice;
    }
    *input = '\0';

    set_mode(gmode_save, mode_save);
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

void error_msg(std::string_view str)
{
    if (g_general_mode == GM_SELECTOR)
    {
        if (str.data() != g_msg.data())
        {
            g_msg = str;
        }
        g_error_occurred = true;
    }
    else if (!str.empty())
    {
        fmt::print("\n{}\n", str);
        term_down(2);
    }
}

void warn_msg(std::string_view str)
{
    if (g_general_mode != GM_SELECTOR)
    {
        fmt::print("\n{}\n", str);
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

static void reprint()
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
        tputs(tgoto_string(g_tc_CM, 0, 0).c_str(), 1, put_char); // go to home via CM
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
    std::string motion;
    const char *str;
    int  cmcost;

    if (g_term_col == to_col && g_term_line == to_line)
    {
        return;
    }
    if (*g_tc_CM && !g_muck_up_clear)
    {
        motion = tgoto_string(g_tc_CM,to_col,to_line);
        str = motion.c_str();
        cmcost = motion.size();
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
                set_env_var("LINES", std::to_string(g_tc_LINES));
                set_env_var("COLUMNS", std::to_string(g_tc_COLS));
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

void xmouse_init(std::string_view progname)
{
    if (!g_can_home || !g_use_threads)
    {
        return;
    }
    const std::string mouse_setting = get_env_var("XTERMMOUSE");
    if (!mouse_setting.empty())
    {
        const std::string use_mouse = do_interp(mouse_setting);
        set_option(OI_USE_MOUSE, use_mouse.c_str());
    }
    else if (!progname.empty() && progname.back() == 'x')
    {
        // an 'x' at the end means enable Xterm mouse tracking
        set_option(OI_USE_MOUSE, "y");
    }
}

struct MouseButtonDisplay
{
    std::string text;
    std::string highlight;
    int         highlight_col{};
    int         width{};
};

static MouseButtonDisplay mouse_button_display(const MouseButton &button)
{
    MouseButtonDisplay display;
    if (button.has_label)
    {
        display.text = button.label;
        const std::size_t highlight_start = display.text.find_first_not_of(' ');
        if (highlight_start != std::string::npos)
        {
            const std::size_t highlight_end = display.text.find(' ', highlight_start);
            display.highlight_col = static_cast<int>(highlight_start);
            display.highlight = display.text.substr(highlight_start, highlight_end - highlight_start);
        }
    }
    else
    {
        switch (button.command.size())
        {
        case 0:
            display.text = "   ";
            display.highlight_col = 1;
            break;

        case 1:
        case 2:
            display.text = " " + button.command + " ";
            display.highlight_col = 1;
            display.highlight = button.command;
            break;

        case 3:
        case 4:
            display.text = button.command;
            display.highlight = button.command;
            break;

        default:
            display.text = button.command.substr(0, 5);
            display.highlight = display.text;
            break;
        }
    }
    display.width = static_cast<int>(display.text.size()) + 1;
    return display;
}

static int mouse_button_bar_width(const MouseButtonList &buttons)
{
    int width = 0;
    for (const MouseButton &button : buttons)
    {
        width += mouse_button_display(button).width;
    }
    return width;
}

void xmouse_check()
{
    g_mouse_bar_cnt = 0;
    g_mouse_bar_width = 0;
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
            const std::string mouse_modes = do_interp(g_mouse_modes);
            turn_it_on = mouse_modes.find(static_cast<char>(g_mode)) != std::string::npos;
        }
        if (turn_it_on)
        {
            switch (mmode)
            {
            case MM_NEWSRC_SELECTOR:
                s_mouse_bar_btns = &g_newsrc_sel_btns;
                break;

            case MM_ADD_GROUP_SELECTOR:
                s_mouse_bar_btns = &g_add_sel_btns;
                break;

            case MM_OPTION_SELECTOR:
                s_mouse_bar_btns = &g_option_sel_btns;
                break;

            case MM_THREAD_SELECTOR:
                s_mouse_bar_btns = &g_news_sel_btns;
                break;

            case MM_NEWSGROUP_SELECTOR:
                s_mouse_bar_btns = &g_newsgroup_sel_btns;
                break;

            case MM_ARTICLE:
            case MM_PAGER:
                s_mouse_bar_btns = &g_art_pager_btns;
                break;

            case MM_UNIVERSAL:
                s_mouse_bar_btns = &g_univ_sel_btns;
                break;

            default:
                s_mouse_bar_btns = nullptr;
                break;
            }
            if (s_mouse_bar_btns != nullptr)
            {
                g_mouse_bar_cnt = size_cast<int>(*s_mouse_bar_btns);
                g_mouse_bar_width = mouse_button_bar_width(*s_mouse_bar_btns);
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

static void xmouse_on()
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
    if (s_mouse_bar_btns == nullptr || g_mouse_bar_cnt == 0)
    {
        return;
    }

    g_mouse_bar_width = mouse_button_bar_width(*s_mouse_bar_btns);
    s_mouse_bar_start = 0;

    while (g_mouse_bar_width > limit && s_mouse_bar_start < g_mouse_bar_cnt)
    {
        const MouseButtonDisplay display =
            mouse_button_display((*s_mouse_bar_btns)[static_cast<std::size_t>(s_mouse_bar_start)]);
        g_mouse_bar_width -= display.width;
        s_mouse_bar_start++;
    }

    goto_xy(g_tc_COLS - g_mouse_bar_width - 1, g_tc_LINES - 1);
    for (int i = s_mouse_bar_start; i < g_mouse_bar_cnt; i++)
    {
        const MouseButtonDisplay display = mouse_button_display((*s_mouse_bar_btns)[static_cast<std::size_t>(i)]);
        std::putchar(' ');
        color_string(COLOR_MOUSE, display.text);
    }
    g_term_col = g_tc_COLS - 1;
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
    int col = g_tc_COLS - g_mouse_bar_width;

    if (s_mouse_bar_btns != nullptr && g_mouse_bar_width != 0 && btn_clk == 0 && y_clk == g_tc_LINES - 1 //
        && (x_clk -= col - 1) > 0)
    {
        x -= col - 1;
        for (int button_index = s_mouse_bar_start; button_index < g_mouse_bar_cnt; button_index++)
        {
            const MouseButton       &button = (*s_mouse_bar_btns)[static_cast<std::size_t>(button_index)];
            const MouseButtonDisplay display = mouse_button_display(button);
            if (x_clk < display.width)
            {
                int tcol = g_term_col;
                int tline = g_term_line;
                goto_xy(col + display.highlight_col, g_tc_LINES - 1);
                if (btn == 3)
                {
                    color_object(COLOR_MOUSE, true);
                }
                for (const char ch : display.highlight)
                {
                    g_term_col++;
                    std::putchar(ch);
                }
                if (btn == 3)
                {
                    color_pop(); // of COLOR_MOUSE
                }
                goto_xy(tcol, tline);
                std::fflush(stdout);
                if (btn == 3 && x > 0 && x < display.width)
                {
                    push_string(button.command.c_str(), 0);
                }
                break;
            }
            if (!(x_clk -= display.width))
            {
                break;
            }
            x -= display.width;
            col += display.width;
        }
        return true;
    }
    return false;
}

static int s_tc_string_cnt{};

struct ColorCapability
{
    std::string capability; // name of capability, e.g. "foreground red"
    std::string string;     // escape sequence, e.g. "\033[31m"
};
static ColorCapability s_tc_strings[TC_STRINGS];

// Parse a line from the [termcap] section of trnrc.
void add_tc_string(const char *capability, const char *value)
{
    int i;

    for (i = 0; i < s_tc_string_cnt; i++)
    {
        if (s_tc_strings[i].capability == capability)
        {
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
        s_tc_strings[i].capability = capability;
    }

    s_tc_strings[i].string = value;
}

// Return the named termcap color capability's string.
const char *tc_color_capability(const char *capability)
{
    for (int c = 0; c < s_tc_string_cnt; c++)
    {
        if (s_tc_strings[c].capability == capability)
        {
            return s_tc_strings[c].string.c_str();
        }
    }
    return nullptr;
}

#ifdef MSDOS
int tputs(const char *str, int num, int (*func)(int))
{
    std::printf(str,num);
    return 0;
}
#endif

std::string tgoto_string(const char *str, int x, int y)
{
#ifdef MSDOS
    return fmt::sprintf(str, y + 1, x + 1);
#else
    const char *result = tgoto(str, x, y);
    return result == nullptr ? std::string{} : result;
#endif
}
