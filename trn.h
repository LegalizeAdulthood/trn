/* trn.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

EXT char* ngname INIT(nullptr);	/* name of current newsgroup */
EXT int ngnlen INIT(0);		/* current malloced size of ngname */
EXT int ngname_len;		/* length of current ngname */
EXT char* ngdir INIT(nullptr);	/* same thing in directory name form */
EXT int ngdlen INIT(0);		/* current malloced size of ngdir */

#define ING_NORM	0
#define ING_ASK		1
#define ING_INPUT	2
#define ING_ERASE	3
#define ING_QUIT	4
#define ING_ERROR	5
#define ING_SPECIAL	6
#define ING_BREAK	7
#define ING_RESTART	8
#define ING_NOSERVER	9
#define ING_DISPLAY	10
#define ING_MESSAGE	11

EXT int ing_state;

#define INGS_CLEAN	0
#define INGS_DIRTY	1

EXT bool  write_less INIT(false);	/* write .newsrc less often */

EXT char* auto_start_cmd INIT(nullptr);	/* command to auto-start with */
EXT bool  auto_started INIT(false);	/* have we auto-started? */

EXT bool  is_strn INIT(false);		/* Is this "strn", or trn/rn? */

EXT char patchlevel[] INIT(PATCHLEVEL);

void trn_init();
void do_multirc();
int input_newsgroup();
void check_active_refetch(bool force);
void trn_version();
void set_ngname(const char *what);
const char *getngdir(const char *ngnam);
