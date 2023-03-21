/* term.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TERM_H
#define TERM_H

/* stuff wanted by terminal mode diddling routines */

#ifdef I_TERMIO
EXT struct termio _tty, _oldtty;
#else
# ifdef I_TERMIOS
EXT struct termios _tty, _oldtty;
# else
#  ifdef I_SGTTY
EXT struct sgttyb _tty;
EXT int _res_flg INIT(0);
#  endif
# endif
#endif

extern char ERASECH; /* rubout character */
extern char KILLCH;  /* line delete character */
extern char circlebuf[PUSHSIZE];
extern int nextin;
extern int nextout;
extern unsigned char lastchar;
extern int _tty_ch;
extern bool bizarre; /* do we need to restore terminal? */

/* termcap stuff */

/*
 * NOTE: if you don't have termlib you'll either have to define these strings
 *    and the tputs routine, or you'll have to redefine the macros below
 */
#ifdef HAS_TERMLIB
extern int tc_GT;   /* hardware tabs */
extern char *tc_BC; /* backspace character */
extern char *tc_UP; /* move cursor up one line */
extern char *tc_CR; /* get to left margin, somehow */
extern char *tc_VB; /* visible bell */
extern char *tc_CL; /* home and clear screen */
extern char *tc_CE; /* clear to end of line */
extern char *tc_TI; /* initialize terminal */
extern char *tc_TE; /* reset terminal */
extern char *tc_KS; /* enter `keypad transmit' mode */
extern char *tc_KE; /* exit `keypad transmit' mode */
extern char *tc_CM; /* cursor motion */
extern char *tc_HO; /* home cursor */
extern char *tc_IL; /* insert line */
extern char *tc_CD; /* clear to end of display */
extern char *tc_SO; /* begin standout mode */
extern char *tc_SE; /* end standout mode */
extern int tc_SG;   /* blanks left by SO and SE */
extern char *tc_US; /* start underline mode */
extern char *tc_UE; /* end underline mode */
extern char *tc_UC; /* underline a character, if that's how it's done */
extern int tc_UG;   /* blanks left by US and UE */
extern bool tc_AM;  /* does terminal have automatic margins? */
extern bool tc_XN;  /* does it eat 1st newline after automatic wrap? */
extern char tc_PC;  /* pad character for use by tputs() */
#ifdef _POSIX_SOURCE
extern speed_t outspeed; /* terminal output speed, */
#else
extern long outspeed; /* 	for use by tputs() */
#endif
extern int fire_is_out;
extern int tc_LINES;
extern int tc_COLS;
extern int g_term_line;
extern int g_term_col;
extern int term_scrolled; /* how many lines scrolled away */
extern int just_a_sec;    /* 1 sec at current baud rate (number of nulls) */
extern int page_line;     /* line number for paging in print_line (origin 1) */
extern bool error_occurred;
extern char *mousebar_btns;
extern int mousebar_cnt;
extern int mousebar_start;
extern int mousebar_width;
extern bool xmouse_is_on;
extern bool mouse_is_down;

/* terminal mode diddling routines */

#ifdef I_TERMIO

#define crmode() ((bizarre=1),_tty.c_lflag &=~ICANON,_tty.c_cc[VMIN] = 1,ioctl(_tty_ch,TCSETAF,&_tty))
#define nocrmode() ((bizarre=1),_tty.c_lflag |= ICANON,_tty.c_cc[VEOF] = CEOF,stty(_tty_ch,&_tty))
#define echo()	 ((bizarre=1),_tty.c_lflag |= ECHO, ioctl(_tty_ch, TCSETA, &_tty))
#define noecho() ((bizarre=1),_tty.c_lflag &=~ECHO, ioctl(_tty_ch, TCSETA, &_tty))
#define nl()	 ((bizarre=1),_tty.c_iflag |= ICRNL,_tty.c_oflag |= ONLCR,ioctl(_tty_ch, TCSETAW, &_tty))
#define nonl()	 ((bizarre=1),_tty.c_iflag &=~ICRNL,_tty.c_oflag &=~ONLCR,ioctl(_tty_ch, TCSETAW, &_tty))
#define	savetty() (ioctl(_tty_ch, TCGETA, &_oldtty),ioctl(_tty_ch, TCGETA, &_tty))
#define	resetty() ((bizarre=0),ioctl(_tty_ch, TCSETAF, &_oldtty))
#define unflush_output()

#else /* !I_TERMIO */
# ifdef I_TERMIOS

#define crmode() ((bizarre=1), _tty.c_lflag &= ~ICANON,_tty.c_cc[VMIN]=1,tcsetattr(_tty_ch, TCSAFLUSH, &_tty))
#define nocrmode() ((bizarre=1),_tty.c_lflag |= ICANON,_tty.c_cc[VEOF] = CEOF,tcsetattr(_tty_ch, TCSAFLUSH,&_tty))
#define echo()	 ((bizarre=1),_tty.c_lflag |= ECHO, tcsetattr(_tty_ch, TCSAFLUSH, &_tty))
#define noecho() ((bizarre=1),_tty.c_lflag &=~ECHO, tcsetattr(_tty_ch, TCSAFLUSH, &_tty))
#define nl()	 ((bizarre=1),_tty.c_iflag |= ICRNL,_tty.c_oflag |= ONLCR,tcsetattr(_tty_ch, TCSAFLUSH, &_tty))
#define nonl()	 ((bizarre=1),_tty.c_iflag &=~ICRNL,_tty.c_oflag &=~ONLCR,tcsetattr(_tty_ch, TCSAFLUSH, &_tty))
#define	savetty() (tcgetattr(_tty_ch, &_oldtty),tcgetattr(_tty_ch, &_tty))
#define	resetty() ((bizarre=0),tcsetattr(_tty_ch, TCSAFLUSH, &_oldtty))
#define unflush_output()

# else /* !I_TERMIOS */
#  ifdef I_SGTTY

#define raw()	 ((bizarre=1),_tty.sg_flags|=RAW, stty(_tty_ch,&_tty))
#define noraw()	 ((bizarre=1),_tty.sg_flags&=~RAW,stty(_tty_ch,&_tty))
#define crmode() ((bizarre=1),_tty.sg_flags |= CBREAK, stty(_tty_ch,&_tty))
#define nocrmode() ((bizarre=1),_tty.sg_flags &= ~CBREAK,stty(_tty_ch,&_tty))
#define echo()	 ((bizarre=1),_tty.sg_flags |= ECHO, stty(_tty_ch, &_tty))
#define noecho() ((bizarre=1),_tty.sg_flags &= ~ECHO, stty(_tty_ch, &_tty))
#define nl()	 ((bizarre=1),_tty.sg_flags |= CRMOD,stty(_tty_ch, &_tty))
#define nonl()	 ((bizarre=1),_tty.sg_flags &= ~CRMOD, stty(_tty_ch, &_tty))
#define	savetty() (gtty(_tty_ch, &_tty), _res_flg = _tty.sg_flags)
#define	resetty() ((bizarre=0),_tty.sg_flags = _res_flg, stty(_tty_ch, &_tty))
#   ifdef LFLUSHO
EXT int lflusho INIT(LFLUSHO);
#define unflush_output() (ioctl(_tty_ch,TIOCLBIC,&lflusho))
#   else /*! LFLUSHO */
#define unflush_output()
#   endif
#  else
#   ifdef MSDOS

#define crmode() (bizarre=1)
#define nocrmode() (bizarre=1)
#define echo()	 (bizarre=1)
#define noecho() (bizarre=1)
#define nl()	 (bizarre=1)
#define nonl()	 (bizarre=1)
#define	savetty()
#define	resetty() (bizarre=0)
#define unflush_output()
#   else /* !MSDOS */
// ..."Don't know how to define the term macros!"
#   endif /* !MSDOS */
#  endif /* !I_SGTTY */
# endif /* !I_TERMIOS */

#endif /* !I_TERMIO */

#ifdef TIOCSTI
#define forceme(c) ioctl(_tty_ch,TIOCSTI,c) /* pass character in " " */
#else
#define forceme(c)
#endif

/* define a few handy macros */
inline void termdown(int x)
{
    g_term_line += x;
    g_term_col = 0;
}
inline void newline()
{
    g_term_line++;
    g_term_col=0;
    putchar('\n');
    FLUSH;
}
#define backspace() tputs(tc_BC,0,putchr) FLUSH
#define erase_eol() tputs(tc_CE,1,putchr) FLUSH
#define clear_rest() tputs(tc_CD,tc_LINES,putchr) FLUSH
#define maybe_eol() if(erase_screen&&erase_each_line)tputs(tc_CE,1,putchr) FLUSH
#define underline() tputs(tc_US,1,putchr) FLUSH
#define un_underline() fire_is_out|=UNDERLINE, tputs(tc_UE,1,putchr) FLUSH
#define underchar() tputs(tc_UC,0,putchr) FLUSH
#define standout() tputs(tc_SO,1,putchr) FLUSH
#define un_standout() fire_is_out|=STANDOUT, tputs(tc_SE,1,putchr) FLUSH
#define up_line() g_term_line--, tputs(tc_UP,1,putchr) FLUSH
#define insert_line() tputs(tc_IL,1,putchr) FLUSH
#define carriage_return() g_term_col=0, tputs(tc_CR,1,putchr) FLUSH
#define dingaling() tputs(tc_VB,1,putchr) FLUSH
#else /* !HAS_TERMLIB */
//..."Don't know how to define the term macros!"
#endif /* !HAS_TERMLIB */

#define input_pending() finput_pending(true)
#define macro_pending() finput_pending(false)

void term_init();
void term_set(char *tcbuf);
void set_macro(char *seq, char *def);
void arrow_macros(char *tmpbuf);
void mac_line(char *line, char *tmpbuf, int tbsize);
void show_macros();
void set_mode(char_int new_gmode, char_int new_mode);
int putchr(char_int ch);
void hide_pending();
bool finput_pending(bool check_term);
bool finish_command(int donewline);
char *edit_buf(char *s, char *cmd);
bool finish_dblchar();
void eat_typeahead();
void save_typeahead(char *buf, int len);
void settle_down();
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
int get_anything();
int pause_getcmd();
void in_char(const char *prompt, char_int newmode, const char *dflt);
void in_answer(const char *prompt, char_int newmode);
bool in_choice(const char *prompt, char *value, char *choices, char_int newmode);
int print_lines(const char *what_to_print, int hilite);
int check_page_line();
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
void termlib_init();
void termlib_reset();
void xmouse_init(const char *progname);
void xmouse_check();
void xmouse_on();
void xmouse_off();
void draw_mousebar(int limit, bool restore_cursor);
bool check_mousebar(int btn, int x, int y, int btn_clk, int x_clk, int y_clk);
void add_tc_string(const char *capability, const char *string);
char *tc_color_capability(const char *capability);
#ifdef MSDOS
int tputs(char *str, int num, int (*func)(int));
char *tgoto(char *str, int x, int y);
#endif

#endif
