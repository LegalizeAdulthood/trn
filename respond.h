/* respond.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

EXT char* savedest INIT(nullptr);		/* value of %b */
EXT char* extractdest INIT(nullptr);	/* value of %E */
EXT char* extractprog INIT(nullptr);	/* value of %e */
EXT ART_POS savefrom INIT(0);		/* value of %B */

#define SAVE_ABORT 0
#define SAVE_DONE 1

void respond_init();
int save_article();
int view_article();
int cancel_article();
int supersede_article();
void reply();
void forward();
void followup();
int invoke(char *cmd, char *dir);
