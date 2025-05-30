/* common.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_COMMON_H
#define TRN_COMMON_H

#include "config/config.h" // generated by installation script
#include "config2.h"

#include <cstdio>
#include <stdexcept>
#include <string>

#include <sys/types.h>
#ifdef I_SYS_STAT
#include <sys/stat.h>
#endif

#include <ctype.h>

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

#ifdef I_TERMIOS
#include <termios.h>
#if !defined(O_NDELAY)
#define O_NDELAY O_NONBLOCK // Posix-style non-blocking i/o
#endif
#endif

#include "config/typedef.h"

enum
{
    BITS_PER_BYTE = 8,
    LINE_BUF_LEN = 1024, // line buffer length
                         // (don't worry, .newsrc lines can exceed this)
    CMD_BUF_LEN = 512,   // command buffer length
    PUSH_SIZE = 256,
    MAX_FILENAME = 512,
    FINISH_CMD = 0177
};

// Things we can figure out ourselves

#ifdef SIGTSTP
#   define BERKELEY // include job control signals?
#endif

#if defined(FIONREAD) || defined(HAS_RDCHK) || defined(O_NDELAY) || defined(MSDOS)
#   define PENDING
#endif

/* Valid substitutions for strings marked with % comment are:
 *      %a      Current article number
 *      %A      Full name of current article (%P/%c/%a)
 *      %b      Destination of a save command, a mailbox or command
 *      %B      The byte offset to the beginning of the article for saves
 *              with or without the header
 *      %c      Current newsgroup, directory form
 *      %C      Current newsgroup, dot form
 *      %d      %P/%c
 *      %D      Old Distribution: line
 *      %e      Extract program
 *      %E      Extract destination directory
 *      %f      Old From: line or Reply-To: line
 *      %F      Newsgroups to followup to from Newsgroups: and Followup-To:
 *      %g      General mode:
 *              I   Init mode.
 *              c   Choice mode (multi-choice input).
 *              i   Input mode (newline terminated).
 *              p   Prompt mode (single-character input).
 *              r   Rn mode.
 *              s   Selector mode.
 *      %h      Name of header file to pass to mail or news poster
 *      %H      Host name (yours)
 *      %i      Old Message-I.D.: line, with <>
 *      %j      The terminal speed (e.g. 9600)
 *      %I      Inclusion indicator
 *      %l      News administrator login name
 *      %L      Login name (yours)
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
 *      %M      Number of articles marked with M
 *      %n      Newsgroups from source article
 *      %N      Full name (yours)
 *      %o      Organization (yours)
 *      %O      Original working directory (where you ran trn from)
 *      %p      Your private news directory (-d switch)
 *      %P      Public news spool directory (NEWS_SPOOL)
 *      %q      The last quoted input (via %").
 *      %r      Last reference (parent article id)
 *      %R      New references list
 *      %s      Subject, with all Re's and (nf)'s stripped off
 *      %S      Subject, with one Re stripped off
 *      %t      New To: line derived from From: and Reply-To (Internet always)
 *      %T      New To: line derived from Path:
 *      %u      Number of unread articles
 *      %U      Number of unread articles disregarding current article
 *      %v      Number of unselected articles disregarding current article
 *      %V      Version string
 *      %x      News library directory, usually /usr/lib/news
 *      %X      Trn's private library directory, usually %x/trn
 *      %y      From line with domain shortening (name@*.domain.nam)
 *      %Y      The tmp directory to use
 *      %z      Size of current article in bytes.
 *      %Z      Number of selected threads.
 *      %~      Home directory
 *      %.      Directory containing . files, usually %~
 *      %+      Directory containing a user's init files, usually %./.trn
 *      %#      count of articles saved in current command (from 1 to n)
 *      %^#     ever-increasing number (from 1 to n)
 *      %$      current process number
 *      %{name} Environment variable "name".  %{name-default} form allowed.
 *      %[name] Header line beginning with "Name: ", without "Name: "
 *      %"prompt"
 *              Print prompt and insert what is typed.
 *      %`command`
 *              Insert output of command.
 *      %(test_text=pattern?if_text:else_text)
 *              Substitute if_text if test_text matches pattern, otherwise
 *              substitute else_text.  Use != for negated match.
 *              % substitutions are done on test_text, if_text, and else_text.
 *      %digit  Substitute the text matched by the nth bracket in the last
 *              pattern that had brackets.  %0 matches the last bracket
 *              matched, in case you had alternatives.
 *      %?      Insert a space unless the entire result is > 79 chars, in
 *              which case the space becomes a newline.
 *
 *      Put ^ in the middle to capitalize the first letter: %^C = Rec.humor
 *      Put _ in the middle to capitalize last component: %_c = net/Jokes
 *      Put \ in the middle to quote regexp and % characters in the result
 *      Put > in the middle to return the address portion of a name.
 *      Put ) in the middle to return the comment portion of a name.
 *      Put ' in the middle to protect "'"s in arguments you've put in "'"s.
 *      Put :FMT in the middle to format the result: %:-30.30t
 *
 *      ~ interpretation in filename expansion happens after % expansion, so
 *      you could put ~%{NEWSLOGNAME-news} and it will expand correctly.
 */

// *** System Dependent Stuff ***

// NOTE: many of these are defined in the config.h file

// name of organization
#ifndef ORG_NAME
#   define ORG_NAME "ACME Widget Company, Widget Falls, Southern North Dakota"
#endif

#ifndef MBOX_CHAR
#   define MBOX_CHAR 'F'     // how to recognize a mailbox by 1st char
#endif

#ifndef ROOT_UID
#   define ROOT_UID 0         // uid of superuser
#endif

#ifdef NORM_SIG
#   define sigset signal
#   define sigignore(sig) signal(sig,SIG_IGN)
#endif

#ifndef PASSWORD_FILE
#   ifdef LIMITED_FILENAMES
#       define PASSWORD_FILE "%X/passwd"
#   else
#       define PASSWORD_FILE "/etc/passwd"
#   endif
#endif

#ifndef LOGIN_DIR_FIELD
#   define LOGIN_DIR_FIELD 6            // Which field (origin 1) is the
                                        // login directory in /etc/passwd?
                                        // (If it is not kept in passwd,
                                        // but getpwnam() returns it,
                                        // define the symbol HAS_GETPWENT)
#endif

#ifndef UNSUBSCRIBED_CHAR
#   define UNSUBSCRIBED_CHAR '!'
#endif

// Space conservation section

// To save D space, cut down size of MAXNGTODO and VARYSIZE.
enum
{
    VARY_SIZE = 256 // this makes a block 1024 bytes long in DECville
                    // (used by virtual array routines)
};

// Undefine any of the following features to save both I and D space
// In general, earlier ones are easier to get along without
#define MAIL_CALL         // check periodically for mail
#define NO_FIREWORKS      // keep whole screen from flashing on certain
                          // terminals such as older Televideos
#define TILDE_NAME        // allow ~logname expansion
#undef M_CHASE            // unmark xrefed articles on m or M
#undef VALIDATE_XREF_SITE // are xrefs possibly invalid?

/* if USEFTP is defined, trn will use the ftpgrab script for ftp: URLs
 * USEFTP is not very well tested, and the ftpgrab script is not
 * installed with make install.  May go away later
 */
#define USE_FTP  /**/

// some dependencies among options

#ifndef SETUIDGID
#   define eaccess access
#endif

#ifdef NDEBUG
#define TRN_ASSERT(ex)
#else
[[noreturn]]
inline void report_assertion(const char *expr, const char *file, unsigned int line)
{
    std::fprintf(stderr, "%s(%u): Assertion '%s' failed\n", file, line, expr);
    throw std::runtime_error("assertion failure");
}
#define TRN_ASSERT(expr_)                                 \
    do                                                    \
    {                                                     \
        if (!(expr_))                                     \
        {                                                 \
            report_assertion(#expr_, __FILE__, __LINE__); \
        }                                                 \
    } while (false)
#endif

#define TC_SIZE 512 // capacity for termcap strings

#define MIN_DIST 7 // Maximum error count for acceptable match

/* Additional ideas:
 *      Make the do_newsgroup() routine a separate process.
 *      Keep .newsrc on disk instead of in memory.
 *      Overlays, if you have them.
 *      Get a bigger machine.
 */

// End of Space Conservation Section

// More System Dependencies

// news library
#ifndef NEWS_LIB         // ~ and %l only ("~%l" is permissible)
#   define NEWS_LIB "/usr/lib/news"
#endif

// path to private executables
#ifndef PRIVATE_LIB         // ~, %x and %l only
#   define PRIVATE_LIB "%x/trn"
#endif

// system-wide RNINIT switches
#ifndef GLOBAL_INIT
#   define GLOBAL_INIT "%X/INIT"
#endif

// where to find news files
#ifndef NEWS_SPOOL       // % and ~
#   define NEWS_SPOOL "/usr/spool/news"
#endif

// default characters to use in the selection menu
#ifndef SELECTION_CHARS
#   define SELECTION_CHARS "abdefgijlorstuvwxyz1234567890BCFGIKVW"
#endif

// file containing list of active newsgroups and max article numbers
#ifndef ACTIVE          // % and ~
#   define ACTIVE "%x/active"
#endif
#ifndef ACTIVE_TIMES
#   define ACTIVE_TIMES "none"
#endif
#ifndef GROUP_DESC
#   define GROUP_DESC "%x/newsgroups"
#endif

// preferred shell for use in doshell routine
// ksh or sh would be okay here
#ifndef PREF_SHELL
#   define PREF_SHELL "/bin/csh"
#endif

// path to fastest starting shell
#ifndef SH
#   define SH "/bin/sh"
#endif

// path to default editor
#ifndef DEFAULT_EDITOR
#   define DEFAULT_EDITOR "/usr/ucb/vi"
#endif

// the user's init files
#ifndef TRNDIR
# ifdef LIMITED_FILENAMES
#   define TRNDIR "%./trn"
# else
#   define TRNDIR "%./.trn"
# endif
#endif

// location of macro file for trn and rn modes
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

// location of full name
#ifndef FULLNAMEFILE
#   ifndef PASS_NAMES
#     ifdef LIMITED_FILENAMES
#       define FULLNAMEFILE "%./fullname"
#     else
#       define FULLNAMEFILE "%./.fullname"
#     endif
#   endif
#endif

// The name to append to the directory name to read an overview file.
#ifndef OV_FILE_NAME
# ifdef LIMITED_FILENAMES
#   define OV_FILE_NAME "/overview"
# else
#   define OV_FILE_NAME "/.overview"
# endif
#endif

// virtual array file name template
#ifndef VARYNAME        // % and ~
#   define VARYNAME "%Y/rnvary.%$"
#endif

// file to pass header to followup article poster
#ifndef HEADNAME        // % and ~
#   ifdef LIMITED_FILENAMES
    #   define HEADNAME "%Y/tmpart.%$"
#   else
#       define HEADNAME "%./.rnhead.%$"
#   endif
#endif

// trn's default access list
#ifndef DEFACCESS
#   define DEFACCESS "%X/access.def"
#endif

// trn's access list
#ifndef TRNACCESS
#   define TRNACCESS "%+/access"
#endif

// location of newsrc file
#ifndef RCNAME          // % and ~
# ifdef LIMITED_FILENAMES
#   define RCNAME "%./newsrc"
# else
#   define RCNAME "%./.newsrc"
# endif
#endif

// temporary newsrc file in case we crash while writing out
#ifndef RCNAME_NEW
#   define RCNAME_NEW "%s.new"
#endif

// newsrc file at the beginning of this session
#ifndef RCNAME_OLD
#   define RCNAME_OLD "%s.old"
#endif

// lockfile for each newsrc that is not ~/.newsrc (which uses .rnlock)
#ifndef RCNAME_LOCK
#   define RCNAME_LOCK "%s.LOCK"
#endif

// news source info for each newsrc that is not ~/.newsrc (.rnlast)
#ifndef RCNAME_INFO
#   define RCNAME_INFO "%s.info"
#endif

// if existent, contains process number of current or crashed trn
#ifndef LOCKNAME        // % and ~
# ifdef LIMITED_FILENAMES
#   define LOCKNAME "%+/lock"
# else
#   define LOCKNAME "%./.rnlock"
# endif
#endif

// information from last invocation of trn
#ifndef LASTNAME        // % and ~
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

// a motd-like file for trn
#ifndef NEWSNEWSNAME    // % and ~
#   define NEWSNEWSNAME "%X/newsnews"
#endif

// command to send a reply
#ifndef MAIL_POSTER      // % and ~
#   define MAIL_POSTER "Rnmail -h %h"
#endif

#ifndef MAIL_HEADER      // %
#   define MAIL_HEADER \
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

#ifndef YOU_SAID         // %
#   define YOU_SAID "In article %i you write:"
#endif

// command to forward an article
#define FORWARD_POSTER MAIL_POSTER

#ifndef FORWARD_HEADER   // %
#define FORWARD_HEADER                                                                                                  \
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

#ifndef FORWARD_MSG      // %
#   define FORWARD_MSG "------- start of forwarded message -------"
#endif

#ifndef FORWARD_MSG_END   // %
#   define FORWARD_MSG_END "------- end of forwarded message -------"
#endif

// command to submit a followup article
#ifndef NEWS_POSTER      // % and ~
#   define NEWS_POSTER "Pnews -h %h"
#endif

#ifndef NEWS_HEADER      // %
#   define NEWS_HEADER "%(%[followup-to]=^$?:%(%[followup-to]=^%n$?:X-ORIGINAL-NEWSGROUPS: %n\n))" \
        "Newsgroups: %(%F=^$?%C:%F)\n" \
        "Subject: %(%S=^$?%\"\n\nSubject: \":Re: %S)\n" \
        "Summary: \n" \
        "Expires: \n" \
        "%(%R=^$?:References: %R\n)" \
        "Sender: \n" \
        "Followup-To: \n" \
        "%(%{FROM}=^$?:From: %{FROM}\n)" \
        "%(%{REPLYTO}=^$?:Reply-To: %{REPLYTO}\n)" \
        "Distribution: %(%i=^$?%\"Distribution: \":%D)\n" \
        "Organization: %o\n" \
        "Keywords: %[keywords]\n" \
        "Cc: %(%F=poster?%t:%(%F!=@?:%F))\n" \
        "\n"
#endif

#ifndef ATTRIBUTION     // %
#   define ATTRIBUTION "In article %i,%?%)f <%>f> wrote:"
#endif

#ifndef PIPE_SAVER       // %
#   define PIPE_SAVER "%(%B=^0$?<%A:tail +%Bc %A |) %b"
#endif

#ifndef SHAR_SAVER
#   define SHAR_SAVER "tail +%Bc %A | /bin/sh"
#endif

#ifndef CUSTOM_SAVER
#   define CUSTOM_SAVER "tail +%Bc %A | %e"
#endif

#ifndef VERIFY_RIPEM
#   define VERIFY_RIPEM "ripem -d -Y fgs -i %A"
#endif

#ifndef VERIFY_PGP
#   define VERIFY_PGP "pgp +batchmode -m %A"
#endif

#ifdef MKDIRS

#   ifndef SAVEDIR      // % and ~
#       define SAVEDIR "%p/%c"
#   endif
#   ifndef SAVENAME     // %
#       define SAVENAME "%a"
#   endif

#else

#   ifndef SAVEDIR      // % and ~
#       define SAVEDIR "%p"
#   endif
#   ifndef SAVENAME     // %
#       define SAVENAME "%^C"
#   endif

#endif

#ifndef KILL_GLOBAL      // % and ~
#   define KILL_GLOBAL "%p/KILL"
#endif

#ifndef KILL_LOCAL       // % and ~
#   define KILL_LOCAL "%p/%c/KILL"
#endif

#ifndef KILL_THREADS     // % and ~
#   define KILL_THREADS "%+/Kill/Threads"
#endif

// how to cancel an article
#ifndef CALL_INEWS
#   ifdef BNEWS
#       define CALL_INEWS "%x/inews -h <%h"
#   else
#       define CALL_INEWS "inews -h <%h"
#   endif
#endif

// how to cancel an article, continued
#ifndef CANCEL_HEADER
#   define CANCEL_HEADER "Newsgroups: %n\n" \
        "Subject: cancel\n" \
        "%(%{FROM}=^$?:From: %{FROM}\n)" \
        "Control: cancel %i\n" \
        "Distribution: %D\n" \
        "\n" \
        "%i was cancelled from within trn.\n"
#endif

// how to supersede an article
#ifndef SUPERSEDE_HEADER
#   define SUPERSEDE_HEADER "Newsgroups: %n\n" \
        "Subject: %[subject]\n" \
        "%(%{FROM}=^$?:From: %{FROM}\n)" \
        "Summary: %[summary]\n" \
        "Expires: %[expires]\n" \
        "References: %[references]\n" \
        "From: %[from]\n" \
        "Reply-To: %[reply-to]\n" \
        "Supersedes: %i\n" \
        "Sender: %[sender]\n" \
        "Followup-To: %[followup-to]\n" \
        "Distribution: %D\n" \
        "Organization: %o\n" \
        "Keywords: %[keywords]\n" \
        "\n"
#endif

#ifndef LOCALTIME_FMT
#   define LOCALTIME_FMT "%a %b %d %X %Z %Y"
#endif

// where to find the mail file
#ifndef MAIL_FILE
#   define MAIL_FILE "/usr/spool/mail/%L"
#endif

#ifndef HAS_VFORK
#   define vfork fork
#endif

// *** end of the machine dependent stuff ***

// GLOBAL THINGS

// various things of type char

extern char g_msg[CMD_BUF_LEN];     // general purpose message buffer
extern char g_buf[LINE_BUF_LEN + 1]; // general purpose line buffer
extern char g_cmd_buf[CMD_BUF_LEN]; // buffer for formatting system commands

#ifdef DEBUG
extern int g_debug; // -D
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

// miscellanea

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

// Factored strings

extern const char *g_h_for_help;
#ifdef STRICT_CR
extern char g_bad_cr[];
#endif
extern const char *g_unsub_to;
extern const char *g_cant_open;
extern const char *g_cant_create;
extern const char *g_no_cd;

#endif
