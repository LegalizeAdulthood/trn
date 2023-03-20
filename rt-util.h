/* rt-util.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */

EXT char spin_char INIT(' ');	/* char to put back when we're done spinning */
EXT long spin_estimate;		/* best guess of how much work there is */
EXT long spin_todo;		/* the max word to do (might decrease) */
EXT int  spin_count;		/* counter for when to spin */
EXT int  spin_marks INIT(25);	/* how many bargraph marks we want */

#define SPIN_OFF	0
#define SPIN_POP	1
#define SPIN_FOREGROUND	2
#define SPIN_BACKGROUND 3
#define SPIN_BARGRAPH	4

EXT bool performed_article_loop;

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
