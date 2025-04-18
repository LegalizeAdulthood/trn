/* trn/rt-util.h
*/
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_RT_UTIL_H
#define TRN_RT_UTIL_H

struct Article;

extern char g_spin_char;              // char to put back when we're done spinning
extern long g_spin_estimate;          // best guess of how much work there is
extern long g_spin_todo;              // the max word to do (might decrease)
extern int  g_spin_count;             // counter for when to spin
extern bool g_performed_article_loop; //
extern bool g_bkgnd_spinner;          // -B
extern bool g_unbroken_subjects;      // -u

enum SpinMode
{
    SPIN_OFF = 0,
    SPIN_POP,
    SPIN_FOREGROUND,
    SPIN_BACKGROUND,
    SPIN_BAR_GRAPH
};

char *extract_name(char *name);
char *compress_name(char *name, int max);
char *compress_address(char *name, int max);
char *compress_from(const char *from, int size);
char *compress_date(const Article *ap, int size);
bool strip_one_re(char *str, char **strp);
bool subject_has_re(char *str, char **strp);
const char *compress_subj(const Article *ap, int max);
void set_spin(SpinMode mode);
void spin(int count);
bool in_background();
void perform_status_init(long cnt);
void perform_status(long cnt, int spin);
int perform_status_end(long cnt, const char *obj_type);

#endif
