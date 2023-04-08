/* common.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_COMMON_H
#define TRN_COMMON_H

#include <stdexcept>
#include <string>

#include <stdio.h>
#include <sys/types.h>
#ifdef I_SYS_STAT
#include <sys/stat.h>
#endif

#include <ctype.h>
#include "config.h"	/* generated by installation script */
#include "config2.h"

#include <errno.h>
#include <signal.h>
#ifdef I_SYS_FILIO
# include <sys/filio.h>
#else
# ifdef I_SYS_IOCTL
#   include <sys/ioctl.h>
# endif
#endif
#ifdef I_VFORK
# include <vfork.h>
#endif
#include <fcntl.h>

#ifdef I_TERMIO
# include <termio.h>
#else
# ifdef I_TERMIOS
#   include <termios.h>
#   if !defined (O_NDELAY)
#     define O_NDELAY O_NONBLOCK	/* Posix-style non-blocking i/o */
#   endif
# else
#   ifdef I_SGTTY
#     include <sgtty.h>
#   endif
# endif
#endif

#ifdef I_PTEM
#include <sys/stream.h>
#include <sys/ptem.h>
#endif

#ifdef I_TIME
#include <time.h>
#endif
#ifdef I_SYS_TIME
#include <sys/time.h>
#endif

#include "typedef.h"

enum
{
    BITSPERBYTE = 8,
    LBUFLEN = 1024, /* line buffer length */
                    /* (don't worry, .newsrc lines can exceed this) */
    CBUFLEN = 512,  /* command buffer length */
    PUSHSIZE = 256,
    MAXFILENAME = 512,
    FINISHCMD = 0177
};

/* Things we can figure out ourselves */

#ifdef SIGTSTP
#   define BERKELEY 	/* include job control signals? */
#endif

#if defined(FIONREAD) || defined(HAS_RDCHK) || defined(O_NDELAY) || defined(MSDOS)
#   define PENDING
#endif

/* Valid substitutions for strings marked with % comment are:
 *	%a	Current article number
 *	%A	Full name of current article (%P/%c/%a)
 *	%b	Destination of a save command, a mailbox or command
 *	%B	The byte offset to the beginning of the article for saves
 *		with or without the header
 *	%c	Current newsgroup, directory form
 *	%C	Current newsgroup, dot form
 *	%d	%P/%c
 *	%D	Old Distribution: line
 *	%e	Extract program
 *	%E	Extract destination directory
 *	%f	Old From: line or Reply-To: line
 *	%F	Newsgroups to followup to from Newsgroups: and Followup-To:
 *      %g      General mode:
 *              I   Init mode.
 *              c   Choice mode (multi-choice input).
 *              i   Input mode (newline terminated).
 *              p   Prompt mode (single-character input).
 *              r   Rn mode.
 *              s   Selector mode.
 *	%h	Name of header file to pass to mail or news poster
 *	%H	Host name (yours)
 *	%i	Old Message-I.D.: line, with <>
 *	%j	The terminal speed (e.g. 9600)
 *	%I	Inclusion indicator
 *	%l	News administrator login name
 *	%L	Login name (yours)
 *      %m      The current minor mode of trn.
 *              A   Add this newsgroup?
 *              B   Abandon confirmation.
 *              C   Catchup confirmation.
 *              D   Delete bogus newsgroups?
 *              F   Is follow-up a new topic?
 *              K   Press any key prompt.
 *              M   Use mailbox format?
 *              R   Resubscribe to this newsgroup?
 *              a   Article level ("What next?").
 *              c   Newsrc selector.
 *              d   Selector mode prompt.
 *              e   End of the article level.
 *              f   End (finis) of newsgroup-list level.
 *              i   Initializing.
 *              j   Addgroup selector.
 *              k   Processing memorized (KILL-file) commands.
 *              l   Option selector.
 *              m   Memorize thread command prompt.
 *              n   Newsgroup-list level.
 *              o   Selector order prompt.
 *              p   Pager level ("MORE" prompt).
 *              r   Memorize subject command prompt.
 *              t   The thread/subject/article selector.
 *              u   Unkill prompt.
 *              w   Newsgroup selector.
 *              z   Option edit prompt.
 *	%M	Number of articles marked with M
 *	%n	Newsgroups from source article
 *	%N	Full name (yours)
 *	%o	Organization (yours)
 *	%O	Original working directory (where you ran trn from)
 *	%p	Your private news directory (-d switch)
 *	%P	Public news spool directory (NEWSSPOOL)
 *	%q	The last quoted input (via %").
 *	%r	Last reference (parent article id)
 *	%R	New references list
 *	%s	Subject, with all Re's and (nf)'s stripped off
 *	%S	Subject, with one Re stripped off
 *	%t	New To: line derived from From: and Reply-To (Internet always)
 *	%T	New To: line derived from Path:
 *	%u	Number of unread articles
 *	%U	Number of unread articles disregarding current article
 *	%v	Number of unselected articles disregarding current article
 *      %V      Version string
 *	%W	The thread directory root
 *	%x	News library directory, usually /usr/lib/news
 *	%X	Trn's private library directory, usually %x/trn
 *	%y	From line with domain shortening (name@*.domain.nam)
 *	%Y	The tmp directory to use
 *	%z	Size of current article in bytes.
 *	%Z	Number of selected threads.
 *	%~	Home directory
 *	%.	Directory containing . files, usually %~
 *	%+	Directory containing a user's init files, usually %./.trn
 *	%#	count of articles saved in current command (from 1 to n)
 *	%^#	ever-increasing number (from 1 to n)
 *	%$	current process number
 *	%{name} Environment variable "name".  %{name-default} form allowed.
 *	%[name]	Header line beginning with "Name: ", without "Name: " 
 *	%"prompt"
 *		Print prompt and insert what is typed.
 *	%`command`
 *		Insert output of command.
 *	%(test_text=pattern?if_text:else_text)
 *		Substitute if_text if test_text matches pattern, otherwise
 *		substitute else_text.  Use != for negated match.
 *		% substitutions are done on test_text, if_text, and else_text.
 *	%digit	Substitute the text matched by the nth bracket in the last
 *		pattern that had brackets.  %0 matches the last bracket
 *		matched, in case you had alternatives.
 *	%?	Insert a space unless the entire result is > 79 chars, in
 *		which case the space becomes a newline.
 *
 *	Put ^ in the middle to capitalize the first letter: %^C = Rec.humor
 *	Put _ in the middle to capitalize last component: %_c = net/Jokes
 *	Put \ in the middle to quote regexp and % characters in the result
 *	Put > in the middle to return the address portion of a name.
 *	Put ) in the middle to return the comment portion of a name.
 *	Put ' in the middle to protect "'"s in arguments you've put in "'"s.
 *	Put :FMT in the middle to format the result: %:-30.30t
 *
 *	~ interpretation in filename expansion happens after % expansion, so
 *	you could put ~%{NEWSLOGNAME-news} and it will expand correctly.
 */

/* *** System Dependent Stuff *** */

/* NOTE: many of these are defined in the config.h file */

/* name of organization */
#ifndef ORGNAME
#   define ORGNAME "ACME Widget Company, Widget Falls, Southern North Dakota"
#endif

#ifndef MBOXCHAR
#   define MBOXCHAR 'F'	/* how to recognize a mailbox by 1st char */
#endif

#ifndef ROOTID
#   define ROOTID 0        /* uid of superuser */
#endif

#ifdef NORMSIG
#   define sigset signal
#   define sigignore(sig) signal(sig,SIG_IGN)
#endif

#ifndef PASSFILE
#   ifdef LIMITED_FILENAMES
#	define PASSFILE "%X/passwd"
#   else
#	define PASSFILE "/etc/passwd"
#   endif
#endif

#ifndef LOGDIRFIELD
#   define LOGDIRFIELD 6		/* Which field (origin 1) is the */
					/* login directory in /etc/passwd? */
					/* (If it is not kept in passwd, */
					/* but getpwnam() returns it, */
					/* define the symbol HAS_GETPWENT) */
#endif
#ifndef GCOSFIELD
#   define GCOSFIELD 5
#endif

#ifndef NEGCHAR
#   define NEGCHAR '!'
#endif

/* Space conservation section */

/* To save D space, cut down size of MAXNGTODO and VARYSIZE. */
enum
{
    MAXNGTODO = 512, /* number of newsgroups allowed on command line */
    VARYSIZE = 256   /* this makes a block 1024 bytes long in DECville */
                     /* (used by virtual array routines) */
};

/* Undefine any of the following features to save both I and D space */
/* In general, earlier ones are easier to get along without */
#define MAILCALL	/* check periodically for mail */
#define NOFIREWORKS	/* keep whole screen from flashing on certain */
			/* terminals such as older Televideos */
#define TILDENAME	/* allow ~logname expansion */
#undef MCHASE		/* unmark xrefed articles on m or M */
#undef	VALIDATE_XREF_SITE /* are xrefs possibly invalid? */

/* if USEFTP is defined, trn will use the ftpgrab script for ftp: URLs
 * USEFTP is not very well tested, and the ftpgrab script is not
 * installed with make install.  May go away later
 */
#define USEFTP  /**/

/* some dependencies among options */

#ifndef SETUIDGID
#   define eaccess access
#endif

#ifdef NDEBUG
#define TRN_ASSERT(ex)
#else
[[noreturn]]
inline void reportAssertion(const char *expr, const char *file, unsigned int line)
{
    fprintf(stderr, "%s(%u): Assertion '%s' failed\n", file, line, expr);
    throw std::runtime_error("assertion failure");
}
#define TRN_ASSERT(expr_)                                \
    do                                                   \
    {                                                    \
        if (!(expr_))                                    \
        {                                                \
            reportAssertion(#expr_, __FILE__, __LINE__); \
        }                                                \
    } while (false)
#endif

#define TCSIZE 512	/* capacity for termcap strings */

#define MIN_DIST 7 /* Maximum error count for acceptable match */

/* Additional ideas:
 *	Make the do_newsgroup() routine a separate process.
 *	Keep .newsrc on disk instead of in memory.
 *	Overlays, if you have them.
 *	Get a bigger machine.
 */

/* End of Space Conservation Section */

/* More System Dependencies */

/* news library */
#ifndef NEWSLIB		/* ~ and %l only ("~%l" is permissable) */
#   define NEWSLIB "/usr/lib/news"
#endif

/* path to private executables */
#ifndef PRIVLIB		/* ~, %x and %l only */
#   define PRIVLIB "%x/trn"
#endif

/* system-wide RNINIT switches */
#ifndef GLOBINIT
#   define GLOBINIT "%X/INIT"
#endif

/* where to find news files */
#ifndef NEWSSPOOL		/* % and ~ */
#   define NEWSSPOOL "/usr/spool/news"
#endif

/* default characters to use in the selection menu */
#ifndef SELECTCHARS
#   define SELECTCHARS "abdefgijlorstuvwxyz1234567890BCFGIKVW"
#endif

/* file containing list of active newsgroups and max article numbers */
#ifndef ACTIVE			/* % and ~ */
#   define ACTIVE "%x/active"
#endif
#ifndef ACTIVE_TIMES
#   define ACTIVE_TIMES "none"
#endif
#ifndef GROUPDESC
#   define GROUPDESC "%x/newsgroups"
#endif
#ifndef DBINIT
#   define DBINIT "%W/db.init"
#endif

/* location of history file */
#ifndef ARTFILE			/* % and ~ */
#    define ARTFILE "%x/history"
#endif

/* preferred shell for use in doshell routine */
/*  ksh or sh would be okay here */
#ifndef PREFSHELL
#   define PREFSHELL "/bin/csh"
#endif

/* path to fastest starting shell */
#ifndef SH
#   define SH "/bin/sh"
#endif

/* path to default editor */
#ifndef DEFEDITOR
#   define DEFEDITOR "/usr/ucb/vi"
#endif

/* the user's init files */
#ifndef TRNDIR
# ifdef LIMITED_FILENAMES
#   define TRNDIR "%./trn"
# else
#   define TRNDIR "%./.trn"
# endif
#endif

/* location of macro file for trn and rn modes */
#ifndef TRNMACRO
#   define TRNMACRO "%+/macros"
#endif
#ifndef RNMACRO
# ifdef LIMITED_FILENAMES
#   define RNMACRO "%./rnmac"
# else
#   define RNMACRO "%./.rnmac"
# endif
#endif

/* location of full name */
#ifndef FULLNAMEFILE
#   ifndef PASSNAMES
#     ifdef LIMITED_FILENAMES
#	define FULLNAMEFILE "%./fullname"
#     else
#	define FULLNAMEFILE "%./.fullname"
#     endif
#   endif
#endif

/* The name to append to the directory name to read an overview file. */
#ifndef OV_FILE_NAME
# ifdef LIMITED_FILENAMES
#   define OV_FILE_NAME "/overview"
# else
#   define OV_FILE_NAME	"/.overview"
# endif
#endif

/* The name to append to the directory name to read a thread file. */
#ifndef MT_FILE_NAME
# ifdef LIMITED_FILENAMES
#   define MT_FILE_NAME "/thread"
# else
#   define MT_FILE_NAME	"/.thread"
# endif
#endif

/* virtual array file name template */
#ifndef VARYNAME		/* % and ~ */
#   define VARYNAME "%Y/rnvary.%$"
#endif

/* file to pass header to followup article poster */
#ifndef HEADNAME		/* % and ~ */
#   ifdef LIMITED_FILENAMES
#	define HEADNAME "%Y/tmpart.%$"
#   else
#	define HEADNAME "%./.rnhead.%$"
#   endif
#endif

/* trn's default access list */
#ifndef DEFACCESS
#   define DEFACCESS "%X/access.def"
#endif

/* trn's access list */
#ifndef TRNACCESS
#   define TRNACCESS "%+/access"
#endif

/* location of newsrc file */
#ifndef RCNAME		/* % and ~ */
# ifdef LIMITED_FILENAMES
#   define RCNAME "%./newsrc"
# else
#   define RCNAME "%./.newsrc"
# endif
#endif

/* temporary newsrc file in case we crash while writing out */
#ifndef RCNAME_NEW
#   define RCNAME_NEW "%s.new"
#endif

/* newsrc file at the beginning of this session */
#ifndef RCNAME_OLD
#   define RCNAME_OLD "%s.old"
#endif

/* lockfile for each newsrc that is not ~/.newsrc (which uses .rnlock) */
#ifndef RCNAME_LOCK
#   define RCNAME_LOCK "%s.LOCK"
#endif

/* news source info for each newsrc that is not ~/.newsrc (.rnlast) */
#ifndef RCNAME_INFO
#   define RCNAME_INFO "%s.info"
#endif

/* if existent, contains process number of current or crashed trn */
#ifndef LOCKNAME		/* % and ~ */
# ifdef LIMITED_FILENAMES
#   define LOCKNAME "%+/lock"
# else
#   define LOCKNAME "%./.rnlock"
# endif
#endif

/* information from last invocation of trn */
#ifndef LASTNAME		/* % and ~ */
# ifdef LIMITED_FILENAMES
#   define LASTNAME "%+/rnlast"
# else
#   define LASTNAME "%./.rnlast"
# endif
#endif

#ifndef SIGNATURE_FILE
# ifdef LIMITED_FILENAMES
#   define SIGNATURE_FILE "%./signatur"
# else
#   define SIGNATURE_FILE "%./.signature"
# endif
#endif

#ifndef NNTP_AUTH_FILE
# define NNTP_AUTH_FILE "%./.nntpauth"
#endif

/* a motd-like file for trn */
#ifndef NEWSNEWSNAME		/* % and ~ */
#   define NEWSNEWSNAME "%X/newsnews"
#endif

/* command to send a reply */
#ifndef MAILPOSTER		/* % and ~ */
#   define MAILPOSTER "Rnmail -h %h"
#endif

#ifndef MAILHEADER		/* % */
#   define MAILHEADER \
        "To: %t\n" \
        "Subject: %(%i=^$?:Re: %S\n" \
        "X-Newsgroups: %n\n" \
        "In-Reply-To: %i)\n" \
        "%(%{FROM}=^$?:From: %{FROM}\n)" \
        "%(%{REPLYTO}=^$?:Reply-To: %{REPLYTO}\n)" \
        "%(%[references]=^$?:References: %[references]\n)" \
        "Organization: %o\n" \
        "Cc: \n" \
        "Bcc: \n" \
        "\n"
#endif

#ifndef YOUSAID			/* % */
#   define YOUSAID "In article %i you write:"
#endif

/* command to forward an article */
#define FORWARDPOSTER MAILPOSTER

#ifndef FORWARDHEADER	/* % */
#define FORWARDHEADER                                                                                                  \
    "To: %\"\n"                                                                                                        \
    "\n"                                                                                                               \
    "To: \"\n"                                                                                                         \
    "Subject: %(%i=^$?:%[subject] (fwd\\)\n"                                                                           \
    "%(%{FROM}=^$?:From: %{FROM}\n)"                                                                                   \
    "%(%{REPLYTO}=^$?:Reply-To: %{REPLYTO}\n)"                                                                         \
    "X-Newsgroups: %n\n"                                                                                               \
    "In-Reply-To: %i)\n"                                                                                               \
    "%(%[references]=^$?:References: %[references]\n)"                                                                 \
    "Organization: %o\n"                                                                                               \
    "Mime-Version: 1.0\n"                                                                                              \
    "Content-Type: multipart/mixed; boundary=\"=%$%^#=--\"\n"                                                          \
    "Cc: \n"                                                                                                           \
    "Bcc: \n"                                                                                                          \
    "\n"
#endif

#ifndef FORWARDMSG		/* % */
#   define FORWARDMSG "------- start of forwarded message -------"
#endif

#ifndef FORWARDMSGEND		/* % */
#   define FORWARDMSGEND "------- end of forwarded message -------"
#endif

/* command to submit a followup article */
#ifndef NEWSPOSTER		/* % and ~ */
#   define NEWSPOSTER "Pnews -h %h"
#endif

#ifndef NEWSHEADER		/* % */
#   define NEWSHEADER "%(%[followup-to]=^$?:%(%[followup-to]=^%n$?:X-ORIGINAL-NEWSGROUPS: %n\n))Newsgroups: %(%F=^$?%C:%F)\nSubject: %(%S=^$?%\"\n\nSubject: \":Re: %S)\nSummary: \nExpires: \n%(%R=^$?:References: %R\n)Sender: \nFollowup-To: \n%(%{FROM}=^$?:From: %{FROM}\n)%(%{REPLYTO}=^$?:Reply-To: %{REPLYTO}\n)Distribution: %(%i=^$?%\"Distribution: \":%D)\nOrganization: %o\nKeywords: %[keywords]\nCc: %(%F=poster?%t:%(%F!=@?:%F))\n\n"
#endif

#ifndef ATTRIBUTION		/* % */
#   define ATTRIBUTION "In article %i,%?%)f <%>f> wrote:"
#endif

#ifndef PIPESAVER		/* % */
#   define PIPESAVER "%(%B=^0$?<%A:tail +%Bc %A |) %b"
#endif

#ifndef SHARSAVER
#   define SHARSAVER "tail +%Bc %A | /bin/sh"
#endif

#ifndef CUSTOMSAVER
#   define CUSTOMSAVER "tail +%Bc %A | %e"
#endif

#ifndef VERIFY_RIPEM
#   define VERIFY_RIPEM "ripem -d -Y fgs -i %A"
#endif

#ifndef VERIFY_PGP
#   define VERIFY_PGP "pgp +batchmode -m %A"
#endif

#ifdef MKDIRS

#   ifndef SAVEDIR			/* % and ~ */
#	define SAVEDIR "%p/%c"
#   endif
#   ifndef SAVENAME		/* % */
#	define SAVENAME "%a"
#   endif

#else

#   ifndef SAVEDIR			/* % and ~ */
#	define SAVEDIR "%p"
#   endif
#   ifndef SAVENAME		/* % */
#	define SAVENAME "%^C"
#   endif

#endif

#ifndef KILLGLOBAL		/* % and ~ */
#   define KILLGLOBAL "%p/KILL"
#endif

#ifndef KILLLOCAL		/* % and ~ */
#   define KILLLOCAL "%p/%c/KILL"
#endif

#ifndef KILLTHREADS		/* % and ~ */
#   define KILLTHREADS "%+/Kill/Threads"
#endif

/* how to cancel an article */
#ifndef CALL_INEWS
#   ifdef BNEWS
#	define CALL_INEWS "%x/inews -h <%h"
#   else
#	define CALL_INEWS "inews -h <%h"
#   endif
#endif

/* how to cancel an article, continued */
#ifndef CANCELHEADER
#   define CANCELHEADER "Newsgroups: %n\nSubject: cancel\n%(%{FROM}=^$?:From: %{FROM}\n)Control: cancel %i\nDistribution: %D\n\n%i was cancelled from within trn.\n"
#endif

/* how to supersede an article */
#ifndef SUPERSEDEHEADER
#   define SUPERSEDEHEADER "Newsgroups: %n\nSubject: %[subject]\n%(%{FROM}=^$?:From: %{FROM}\n)Summary: %[summary]\nExpires: %[expires]\nReferences: %[references]\nFrom: %[from]\nReply-To: %[reply-to]\nSupersedes: %i\nSender: %[sender]\nFollowup-To: %[followup-to]\nDistribution: %D\nOrganization: %o\nKeywords: %[keywords]\n\n"
#endif

#ifndef LOCALTIMEFMT
#   define LOCALTIMEFMT "%a %b %d %X %Z %Y"
#endif

/* where to find the mail file */
#ifndef MAILFILE
#   define MAILFILE "/usr/spool/mail/%L"
#endif

#ifndef HAS_VFORK
#   define vfork fork
#endif

/* *** end of the machine dependent stuff *** */

/* GLOBAL THINGS */

/* various things of type char */

extern char g_msg[CBUFLEN];     /* general purpose message buffer */
extern char g_buf[LBUFLEN + 1]; /* general purpose line buffer */
extern char g_cmd_buf[CBUFLEN]; /* buffer for formatting system commands */

#ifdef DEBUG
extern int debug; /* -D */
#   define DEB_COREDUMPSOK 2
#   define DEB_HEADER 4
#   define DEB_INTRP 8
#   define DEB_NNTP 16
#   define DEB_INNERSRCH 32
#   define DEB_FILEXP 64 
#   define DEB_HASH 128
#   define DEB_XREF_MARKER 256
#   define DEB_CTLAREA_BITMAP 512
#   define DEB_RCFILES 1024
#   define DEB_NEWSRC_LINE 2048
#   define DEB_SEARCH_AHEAD 4096
#   define DEB_CHECKPOINTING 8192
#   define DEB_FEED_XREF 16384
#endif

/* miscellania */

inline const char *plural(int num)
{
    return num == 1 ? "" : "s";
}

template <typename T, typename U>
bool all_bits(T val, U bits)
{
    const T tbits = static_cast<T>(bits);
    return (val & tbits) == tbits;
}

/* Factored strings */

extern const char *g_hforhelp;
#ifdef STRICTCR
extern char g_badcr[];
#endif
extern const char *g_readerr;
extern const char *g_unsubto;
extern const char *g_cantopen;
extern const char *g_cantcreate;
extern const char *g_nocd;

#ifdef NOLINEBUF
#define FLUSH ,fflush(stdout)
#else
#define FLUSH
#endif

#ifdef lint
#undef FLUSH
#define FLUSH
#undef putchar
#define putchar(c)
#endif

inline bool empty(const char *str)
{
    return str == nullptr || str[0] == '\0';
}

#endif
