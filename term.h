/* term.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_TERM_H
#define TRN_TERM_H

/* stuff wanted by terminal mode diddling routines */

#ifdef I_TERMIO
extern termio g_tty;
extern termio g_oldtty;
#else
#ifdef I_TERMIOS
extern termios g_tty;
extern termios g_oldtty;
#else
#ifdef I_SGTTY
extern sgttyb g_tty;
extern int    g_res_flg;
#endif
#endif
#endif
#include "common.h"

extern char          g_erase_char; /* rubout character */
extern char          g_kill_char;  /* line delete character */
extern unsigned char g_lastchar;
extern bool          g_bizarre; /* do we need to restore terminal? */

/* termcap stuff */

/*
 * NOTE: if you don't have termlib you'll either have to define these strings
 *    and the tputs routine, or you'll have to redefine the macros below
 */
#ifdef HAS_TERMLIB
extern int   g_tc_GT; /* hardware tabs */
extern char *g_tc_BC; /* backspace character */
extern char *g_tc_UP; /* move cursor up one line */
extern char *g_tc_CR; /* get to left margin, somehow */
extern char *g_tc_VB; /* visible bell */
extern char *g_tc_CL; /* home and clear screen */
extern char *g_tc_CE; /* clear to end of line */
extern char *g_tc_TI; /* initialize terminal */
extern char *g_tc_CM; /* cursor motion */
extern char *g_tc_HO; /* home cursor */
extern char *g_tc_IL; /* insert line */
extern char *g_tc_CD; /* clear to end of display */
extern char *g_tc_SO; /* begin standout mode */
extern char *g_tc_SE; /* end standout mode */
extern char *g_tc_US; /* start underline mode */
extern char *g_tc_UE; /* end underline mode */
extern char *g_tc_UC; /* underline a character, if that's how it's done */
extern int   g_tc_UG; /* blanks left by US and UE */
extern bool  g_tc_AM; /* does terminal have automatic margins? */
extern bool  g_tc_XN; /* does it eat 1st newline after automatic wrap? */
extern int   g_fire_is_out;
extern int   g_tc_LINES;
extern int   g_tc_COLS;
extern int   g_term_line;
extern int   g_term_col;
extern int   g_term_scrolled; /* how many lines scrolled away */
extern int   g_just_a_sec;    /* 1 sec at current baud rate (number of nulls) */
extern int   g_page_line;     /* line number for paging in print_line (origin 1) */
extern bool  g_error_occurred;
extern int   g_mousebar_cnt;
extern int   g_mousebar_width;
extern bool  g_mouse_is_down;

void  term_init();
void  term_set(char *tcbuf);
void  set_macro(char *seq, char *def);
void  arrow_macros(char *tmpbuf);
void  mac_line(char *line, char *tmpbuf, int tbsize);
void  show_macros();
void  set_mode(general_mode new_gmode, minor_mode new_mode);
int   putchr(char_int ch);
void  hide_pending();
bool  finput_pending(bool check_term);
bool  finish_command(int donewline);
char *edit_buf(char *s, const char *cmd);
bool  finish_dblchar();
void  eat_typeahead();
void  save_typeahead(char *buf, int len);
void  settle_down();
#ifdef SIGALRM
Signal_t alarm_catcher(int signo);
#endif
int read_tty(char *addr, int size);
#if !defined(FIONREAD) && !defined(HAS_RDCHK) && !defined(MSDOS)
int circfill();
#endif
void pushchar(char_int c);
void underprint(const char *s);
#ifdef NOFIREWORKS
void no_sofire();
void no_ulfire();
#endif
void getcmd(char *whatbuf);
void pushstring(char *str, char_int bits);
int  get_anything();
int  pause_getcmd();
void in_char(const char *prompt, minor_mode newmode, const char *dflt);
void in_answer(const char *prompt, minor_mode newmode);
bool in_choice(const char *prompt, char *value, char *choices, minor_mode newmode);
int  print_lines(const char *what_to_print, int hilite);
int  check_page_line();
void page_start();
void errormsg(const char *str);
void warnmsg(const char *str);
void pad(int num);
void printcmd();
void rubout();
void reprint();
void erase_line(bool to_eos);
void clear();
void home_cursor();
void goto_xy(int to_col, int to_line);
#ifdef SIGWINCH
Signal_t winch_catcher(int);
#endif
void  termlib_init();
void  termlib_reset();
void  xmouse_init(const char *progname);
void  xmouse_check();
void  xmouse_on();
void  xmouse_off();
void  draw_mousebar(int limit, bool restore_cursor);
bool  check_mousebar(int btn, int x, int y, int btn_clk, int x_clk, int y_clk);
void  add_tc_string(const char *capability, const char *string);
char *tc_color_capability(const char *capability);
#ifdef MSDOS
int   tputs(char *str, int num, int (*func)(int));
char *tgoto(char *str, int x, int y);
#endif

/* terminal mode diddling routines */

#ifdef I_TERMIO
extern int g_tty_ch;

inline void crmode()
{
    g_bizarre = true;
    g_tty.c_lflag &= ~ICANON;
    g_tty.c_cc[VMIN] = 1;
    ioctl(g_tty_ch, TCSETAF, &g_tty);
}
inline void nocrmode()
{
    g_bizarre = true;
    g_tty.c_lflag |= ICANON;
    g_tty.c_cc[VEOF] = CEOF;
    stty(g_tty_ch, &g_tty);
}
inline void echo()
{
    g_bizarre = true;
    g_tty.c_lflag |= ECHO;
    ioctl(g_tty_ch, TCSETA, &g_tty);
}
inline void noecho()
{
    g_bizarre = true;
    g_tty.c_lflag &= ~ECHO;
    ioctl(g_tty_ch, TCSETA, &g_tty);
}
inline void nl()
{
    g_bizarre = true;
    g_tty.c_iflag |= ICRNL;
    g_tty.c_oflag |= ONLCR;
    ioctl(g_tty_ch, TCSETAW, &g_tty);
}
inline void nonl()
{
    g_bizarre = true;
    g_tty.c_iflag &= ~ICRNL;
    g_tty.c_oflag &= ~ONLCR;
    ioctl(g_tty_ch, TCSETAW, &g_tty);
}
inline void savetty()
{
    ioctl(g_tty_ch, TCGETA, &g_oldtty);
    ioctl(g_tty_ch, TCGETA, &g_tty);
}
inline void resetty()
{
    g_bizarre = false;
    ioctl(g_tty_ch, TCSETAF, &g_oldtty);
}
inline void unflush_output()
{
}

#else /* !I_TERMIO */
#ifdef I_TERMIOS
extern int  g_tty_ch;

inline void crmode()
{
    g_bizarre = true;
    g_tty.c_lflag &= ~ICANON;
    g_tty.c_cc[VMIN] = 1;
    tcsetattr(g_tty_ch, TCSAFLUSH, &g_tty);
}
inline void nocrmode()
{
    g_bizarre = true;
    g_tty.c_lflag |= ICANON;
    g_tty.c_cc[VEOF] = CEOF;
    tcsetattr(g_tty_ch, TCSAFLUSH, &g_tty);
}
inline void echo()
{
    g_bizarre = true;
    g_tty.c_lflag |= ECHO;
    tcsetattr(g_tty_ch, TCSAFLUSH, &g_tty);
}
inline void noecho()
{
    g_bizarre = true;
    g_tty.c_lflag &= ~ECHO;
    tcsetattr(g_tty_ch, TCSAFLUSH, &g_tty);
}
inline void nl()
{
    g_bizarre = true;
    g_tty.c_iflag |= ICRNL;
    g_tty.c_oflag |= ONLCR;
    tcsetattr(g_tty_ch, TCSAFLUSH, &g_tty);
}
inline void nonl()
{
    g_bizarre = true;
    g_tty.c_iflag &= ~ICRNL;
    g_tty.c_oflag &= ~ONLCR;
    tcsetattr(g_tty_ch, TCSAFLUSH, &g_tty);
}
inline void savetty()
{
    tcgetattr(g_tty_ch, &g_oldtty);
    tcgetattr(g_tty_ch, &g_tty);
}
inline void resetty()
{
    g_bizarre = false;
    tcsetattr(g_tty_ch, TCSAFLUSH, &g_oldtty);
}
inline void unflush_output()
{
}

#else /* !I_TERMIOS */
#ifdef I_SGTTY
extern int g_tty_ch;

inline void raw()
{
    g_bizarre = true;
    g_tty.sg_flags |= RAW;
    stty(g_tty_ch, &g_tty);
}
inline void noraw()
{
    g_bizarre = true;
    g_tty.sg_flags &= ~RAW;
    stty(g_tty_ch, &g_tty);
}
inline void crmode()
{
    g_bizarre = true;
    g_tty.sg_flags |= CBREAK;
    stty(g_tty_ch, &g_tty);
}
inline void nocrmode()
{
    g_bizarre = true;
    g_tty.sg_flags &= ~CBREAK;
    stty(g_tty_ch, &g_tty);
}
inline void echo()
{
    g_bizarre = true;
    g_tty.sg_flags |= ECHO;
    stty(g_tty_ch, &g_tty);
}
inline void noecho()
{
    g_bizarre = true;
    g_tty.sg_flags &= ~ECHO;
    stty(g_tty_ch, &g_tty);
}
inline void nl()
{
    g_bizarre = true;
    g_tty.sg_flags |= CRMOD;
    stty(g_tty_ch, &g_tty);
}
inline void nonl()
{
    g_bizarre = true;
    g_tty.sg_flags &= ~CRMOD;
    stty(g_tty_ch, &g_tty);
}
inline void savetty()
{
    gtty(g_tty_ch, &g_tty);
    g_res_flg = g_tty.sg_flags;
}
inline void resetty()
{
    g_bizarre = false;
    g_tty.sg_flags = g_res_flg;
    stty(g_tty_ch, &g_tty);
}
inline void unflush_output()
{
#ifdef LFLUSHO
    extern int g_lflusho;
    ioctl(g_tty_ch, TIOCLBIC, &g_lflusho);
#endif
}
#else
#ifdef MSDOS

inline void crmode()
{
    g_bizarre = true;
}
inline void nocrmode()
{
    g_bizarre = true;
}
inline void echo()
{
    g_bizarre = true;
}
inline void noecho()
{
    g_bizarre = true;
}
inline void nl()
{
    g_bizarre = true;
}
inline void nonl()
{
    g_bizarre = true;
}
inline void savetty()
{
}
inline void resetty()
{
    g_bizarre = false;
}
inline void unflush_output()
{
}
#else  /* !MSDOS */
// ..."Don't know how to define the term macros!"
#endif /* !MSDOS */
#endif /* !I_SGTTY */
#endif /* !I_TERMIOS */

#endif /* !I_TERMIO */

inline void forceme(const char *c)
{
#ifdef TIOCSTI
    ioctl(g_tty_ch, TIOCSTI, c) /* pass character in " " */
#endif
}

/* define a few handy macros */
inline void termdown(int x)
{
    g_term_line += x;
    g_term_col = 0;
}
inline void newline()
{
    g_term_line++;
    g_term_col = 0;
    putchar('\n');
    FLUSH;
}
inline void backspace()
{
    tputs(g_tc_BC, 0, putchr) FLUSH;
}
inline void erase_eol()
{
    tputs(g_tc_CE, 1, putchr) FLUSH;
}
inline void clear_rest()
{
    tputs(g_tc_CD, g_tc_LINES, putchr) FLUSH;
}
inline void maybe_eol()
{
    if (g_erase_screen && g_erase_each_line)
        tputs(g_tc_CE, 1, putchr) FLUSH;
}
inline void underline()
{
    tputs(g_tc_US, 1, putchr) FLUSH;
}
inline void un_underline()
{
    g_fire_is_out |= UNDERLINE;
    tputs(g_tc_UE, 1, putchr) FLUSH;
}
inline void underchar()
{
    tputs(g_tc_UC, 0, putchr) FLUSH;
}
inline void standout()
{
    tputs(g_tc_SO, 1, putchr) FLUSH;
}
inline void un_standout()
{
    g_fire_is_out |= STANDOUT;
    tputs(g_tc_SE, 1, putchr) FLUSH;
}
inline void up_line()
{
    g_term_line--;
    tputs(g_tc_UP, 1, putchr) FLUSH;
}
inline void insert_line()
{
    tputs(g_tc_IL, 1, putchr) FLUSH;
}
inline void carriage_return()
{
    g_term_col = 0, tputs(g_tc_CR, 1, putchr) FLUSH;
}
inline void dingaling()
{
    tputs(g_tc_VB, 1, putchr) FLUSH;
}
#else  /* !HAS_TERMLIB */
//..."Don't know how to define the term macros!"
#endif /* !HAS_TERMLIB */

inline bool input_pending()
{
    return finput_pending(true);
}
inline bool macro_pending()
{
    return finput_pending(false);
}

#endif
