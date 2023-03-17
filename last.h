/* last.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

EXT char* lastngname INIT(NULL);	/* last newsgroup read */
EXT long lasttime INIT(0);		/* time last we ran */
EXT long lastactsiz INIT(0);		/* last known size of active file */
EXT long lastnewtime INIT(0);		/* time of last newgroup request */
EXT long lastextranum INIT(0);

void last_init();
void readlast();
void writelast();
