/* This file Copyright 1993 by Clifford A. Adams */
/* scoresave.c
 *
 * Saving/restoring scores from a file.
 */

#include "EXTERN.h"
#include "common.h"
#include "list.h"
#include "cache.h"
#include "ngdata.h"
#include "util.h"		/* several */
#include "util2.h"
#include "env.h"		/* get_val */
#include "scan.h"
#include "scanart.h"
#include "score.h"
#include "scoresave.h"

int g_sc_loaded_count{}; /* how many articles were loaded? */

static long s_sc_save_new{}; /* new articles (unloaded) */
static int s_num_lines{};
static int s_lines_alloc{};
static char **s_lines{};
static char s_lbuf[LBUFLEN]{};
static char s_lbuf2[LBUFLEN]{}; /* what's another buffer between... */
static int s_loaded{};
static int s_used{};
static int s_saved{};
static ART_NUM s_last{};

void sc_sv_add(const char *str)
{
    if (s_num_lines == s_lines_alloc) {
	s_lines_alloc += 100;
	s_lines = (char**)saferealloc((char*)s_lines,s_lines_alloc * sizeof (char*));
    }
    s_lines[s_num_lines] = savestr(str);
    s_num_lines++;
}

void sc_sv_delgroup(const char *gname)
{
    char* s;
    int i;
    int start;

    for (i = 0; i < s_num_lines; i++) {
	s = s_lines[i];
	if (s && *s == '!' && !strcmp(gname,s+1))
	    break;
    }
    if (i == s_num_lines)
	return;		/* group not found */
    start = i;
    free(s_lines[i]);
    s_lines[i] = nullptr;
    for (i++; i < s_num_lines; i++) {
	s = s_lines[i];
	if (s && *s == '!')
	    break;
	if (s) {
	    free(s);
	    s_lines[i] = nullptr;
	}
    }
    /* copy into the hole (if any) */
    for ( ; i < s_num_lines; i++)
	s_lines[start++] = s_lines[i];
    s_num_lines -= (i-start);
}

/* get the file containing scores into memory */
void sc_sv_getfile()
{
    char* s;
    FILE* fp;

    s_num_lines = s_lines_alloc = 0;
    s_lines = nullptr;

    s = get_val("SAVESCOREFILE","%+/savedscores");
    fp = fopen(filexp(s),"r");
    if (!fp) {
#if 0
	printf("Could not open score save file for reading.\n") FLUSH;
#endif
	return;
    }
    while (fgets(s_lbuf,LBUFLEN-2,fp)) {
	s_lbuf[strlen(s_lbuf)-1] = '\0';	/* strip \n */
	sc_sv_add(s_lbuf);
    }
    fclose(fp);
}

/* save the memory into the score file */
void sc_sv_savefile()
{
    char* s;
    FILE* tmpfp;
    char* savename;
    int i;

    if (s_num_lines == 0)
	return;
    waiting = true;	/* don't interrupt */
    s = get_val("SAVESCOREFILE","%+/savedscores");
    savename = savestr(filexp(s));
    strcpy(s_lbuf,savename);
    strcat(s_lbuf,".tmp");
    tmpfp = fopen(s_lbuf,"w");
    if (!tmpfp) {
#if 0
	printf("Could not open score save temp file %s for writing.\n",
	       s_lbuf) FLUSH;
#endif
	free(savename);
	waiting = false;
	return;
    }
    for (i = 0; i < s_num_lines; i++) {
	if (s_lines[i])
	    fprintf(tmpfp,"%s\n",s_lines[i]);
	if (ferror(tmpfp)) {
	    fclose(tmpfp);
	    free(savename);
	    printf("\nWrite error in temporary save file %s\n",s_lbuf) FLUSH;
	    printf("(keeping old saved scores)\n");
	    remove(s_lbuf);
	    waiting = false;
	    return;
	}
    }
    fclose(tmpfp);
    remove(savename);
    rename(s_lbuf,savename);
    waiting = false;
}

/* returns the next article number (after the last one used) */
//ART_NUM a;	/* art number to start with */
ART_NUM sc_sv_use_line(char *line, ART_NUM a)
{
    char* s;
    char* p;
    char c1,c2;
    int score;
    int x;

    score = 0;	/* get rid of warning */
    s = line;
    if (!s)
	return a;
    while (*s) {
	switch(*s) {
	  case 'A': case 'B': case 'C': case 'D': case 'E':
	  case 'F': case 'G': case 'H': case 'I':
	    /* negative starting digit */
	    p = s;
	    c1 = *s;
	    *s = '0' + ('J' - *s);	/* convert to first digit */
	    s++;
	    while (isdigit(*s)) s++;
	    c2 = *s;
	    *s = '\0';
	    score = 0 - atoi(p);
	    *p = c1;
	    *s = c2;
	    s_loaded++;
	    if (is_available(a) && article_unread(a)) {
		sc_set_score(a,score);
		s_used++;
	    }
	    a++;
	    break;
	  case 'J': case 'K': case 'L': case 'M': case 'N':
	  case 'O': case 'P': case 'Q': case 'R': case 'S':
	    /* positive starting digit */
	    p = s;
	    c1 = *s;
	    *s = '0' + (*s - 'J');	/* convert to first digit */
	    s++;
	    while (isdigit(*s)) s++;
	    c2 = *s;
	    *s = '\0';
	    score = atoi(p);
	    *p = c1;
	    *s = c2;
	    s_loaded++;
	    if (is_available(a) && article_unread(a)) {
		sc_set_score(a,score);
		s_used++;
	    }
	    a++;
	    break;
	  case 'r':	/* repeat */
	    s++;
	    p = s;
	    if (!isdigit(*s)) {
		/* simple case, just "r" */
		x = 1;
	    } else {
		s++;
		while (isdigit(*s)) s++;
		c1 = *s;
		*s = '\0';
		x = atoi(p);
		*s = c1;
	    }
	    for ( ; x; x--) {
		s_loaded++;
		if (is_available(a) && article_unread(a)) {
		    sc_set_score(a,score);
		    s_used++;
		}
		a++;
	    }
	    break;
	  case 's':	/* skip */
	    s++;
	    p = s;
	    if (!isdigit(*s)) {
		/* simple case, just "s" */
		a += 1;
	    } else {
		s++;
		while (isdigit(*s)) s++;
		c1 = *s;
		*s = '\0';
		x = atoi(p);
		*s = c1;
		a += x;
	    }
	    break;
	} /* switch */
    } /* while */
    return a;
}

ART_NUM sc_sv_make_line(ART_NUM a)
{
    char* s;
    bool lastscore_valid = false;
    int num_output = 0;
    int score,lastscore;
    int i;
    bool neg_flag;

    s = s_lbuf;
    *s++ = '.';
    lastscore = 0;

    for (a = article_first(a); a <= g_lastart && num_output < 50; a = article_next(a)) {
	if (article_unread(a) && SCORED(a)) {
	    if (s_last != a-1) {
		if (s_last == a-2) {
		    *s++ = 's';
		    num_output++;
		} else {
		    sprintf(s,"s%ld",(a-s_last)-1);
		    s = s_lbuf + strlen(s_lbuf);
		    num_output++;
		}
	    }
	    /* print article's score */
	    score = article_ptr(a)->score;
	    /* check for repeating scores */
	    if (score == lastscore && lastscore_valid) {
		a = article_next(a);
		for (i = 1; a <= g_lastart && article_unread(a) && SCORED(a)
			 && article_ptr(a)->score == score; i++)
		    a = article_next(a);
		a = article_prev(a);	/* prepare for the for loop increment */
		if (i == 1) {
		    *s++ = 'r';		/* repeat one */
		    num_output++;
		} else {
		    sprintf(s,"r%d",i);	/* repeat >one */
		    s = s_lbuf + strlen(s_lbuf);
		    num_output++;
		}
		s_saved += i-1;
	    } else {	/* not a repeat */
		i = score;
		if (i < 0) {
		    neg_flag = true;
		    i = 0 - i;
		} else
		    neg_flag = false;
		sprintf(s,"%d",i);
		i = (*s - '0');
		if (neg_flag)
		    *s++ = 'J' - i;
		else
		    *s++ = 'J' + i;
		s = s_lbuf + strlen(s_lbuf);
		num_output++;
		lastscore_valid = true;
	    }
	    lastscore = score;
	    s_last = a;
	    s_saved++;
	} /* if */
    } /* for */
    *s = '\0';
    sc_sv_add(s_lbuf);
    return a;
}

void sc_load_scores()
{
/* lots of cleanup needed here */
    ART_NUM a = 0;
    char* s;
    char* gname;
    int i;
    int total,scored;
    bool verbose;

    s_sc_save_new = -1;		/* just in case we exit early */
    s_loaded = s_used = 0;
    g_sc_loaded_count = 0;

    /* verbosity is only really useful for debugging... */
    verbose = false;

    if (s_num_lines == 0)
	sc_sv_getfile();

    gname = savestr(filexp("%C"));

    for (i = 0; i < s_num_lines; i++) {
	s = s_lines[i];
	if (s && *s == '!' && !strcmp(s+1,gname))
	    break;
    }
    if (i == s_num_lines)
	return;		/* no scores loaded */
    i++;

    if (verbose) {
	printf("\nLoading scores...");
	fflush(stdout);
    }
    while (i < s_num_lines) {
	s = s_lines[i++];
	if (!s)
	    continue;
	switch (*s) {
	  case ':':
	    a = atoi(s+1);	/* set the article # */
	    break;
	  case '.':			/* longer score line */
	    a = sc_sv_use_line(s+1,a);
	    break;
	  case '!':			/* group of shared file */
	    i = s_num_lines;
	    break;
	  case 'v':			/* version number */
	    break;			/* not used now */
	  case '\0':			/* empty string */
	  case '#':			/* comment */
	    break;
	  default:
	    /* don't even try to deal with it */
	    return;
	} /* switch */
    } /* while */

    g_sc_loaded_count = s_loaded;
    a = g_firstart;
    if (g_sa_mode_read_elig)
	a = g_absfirst;
    total = scored = 0;
    for (a = article_first(a); a <= g_lastart; a = article_next(a)) {
	if (!article_exists(a))
	    continue;
        if (!article_unread(a) && !g_sa_mode_read_elig)
	    continue;
	total++;
	if (SCORED(a))
	    scored++;
    } /* for */

    /* sloppy plurals (:-) */
    if (verbose)
	printf("(%d/%d/%d scores loaded/used/unscored)\n",
	       s_loaded,s_used,total-scored) FLUSH;

    s_sc_save_new = total-scored;
    if (g_sa_initialized)
	g_s_top_ent = -1;	/* reset top of page */
}

void sc_save_scores()
{
    ART_NUM a;
    char* gname;

    s_saved = 0;
    s_last = 0;

    waiting = true;	/* DON'T interrupt */
    gname = savestr(filexp("%C"));
    /* not being able to open is OK */
    if (s_num_lines > 0) {
	sc_sv_delgroup(gname);	/* delete old group */
    } else {		/* there was no old file */
	sc_sv_add("#STRN saved score file.");
	sc_sv_add("v1.0");
    }
    sprintf(s_lbuf2,"!%s",gname);	/* add the header */
    sc_sv_add(s_lbuf2);

    a = g_firstart;
    sprintf(s_lbuf2,":%ld",a);
    sc_sv_add(s_lbuf2);
    s_last = a-1;
    while (a <= g_lastart)
	a = sc_sv_make_line(a);
    waiting = false;
}
