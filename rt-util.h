/* rt-util.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef RT_UTIL_H
#define RT_UTIL_H

extern char g_spin_char;     /* char to put back when we're done spinning */
extern long g_spin_estimate; /* best guess of how much work there is */
extern long g_spin_todo;     /* the max word to do (might decrease) */
extern int g_spin_count;     /* counter for when to spin */
extern int g_spin_marks;     /* how many bargraph marks we want */
extern bool g_performed_article_loop;

enum
{
    SPIN_OFF = 0,
    SPIN_POP = 1,
    SPIN_FOREGROUND = 2,
    SPIN_BACKGROUND = 3,
    SPIN_BARGRAPH = 4
};

char *extract_name(char *name);
char *compress_name(char *name, int max);
char *compress_address(char *name, int max);
char *compress_from(char *from, int size);
char *compress_date(const ARTICLE *ap, int size);
bool subject_has_Re(char *str, char **strp);
const char *compress_subj(const ARTICLE *ap, int max);
void setspin(int mode);
void spin(int count);
bool inbackground();
void perform_status_init(long cnt);
void perform_status(long cnt, int spin);
int perform_status_end(long cnt, const char *obj_type);

#endif
