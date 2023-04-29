/* This file Copyright 1993 by Clifford A. Adams */
/* scoresave.c
 *
 * Saving/restoring scores from a file.
 */

#include "common.h"
#include "scoresave.h"

#include "cache.h"
#include "env.h" /* get_val */
#include "list.h"
#include "ngdata.h"
#include "scan.h"
#include "scanart.h"
#include "score.h"
#include "string-algos.h"
#include "util.h" /* several */
#include "util2.h"

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

    for (i = 0; i < s_num_lines; i++) {
	s = s_lines[i];
	if (s && *s == '!' && !strcmp(gname,s+1))
	    break;
    }
    if (i == s_num_lines)
	return;		/* group not found */
    int start = i;
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
    s_num_lines = 0;
    s_lines_alloc = 0;
    s_lines = nullptr;

    char *s = get_val("SAVESCOREFILE", "%+/savedscores");
    FILE *fp = fopen(filexp(s), "r");
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
    if (s_num_lines == 0)
	return;

    g_waiting = true;	/* don't interrupt */
    char *savename = savestr(filexp(get_val("SAVESCOREFILE", "%+/savedscores")));
    strcpy(s_lbuf,savename);
    strcat(s_lbuf,".tmp");
    FILE *tmpfp = fopen(s_lbuf, "w");
    if (!tmpfp) {
#if 0
	printf("Could not open score save temp file %s for writing.\n",
	       s_lbuf) FLUSH;
#endif
	free(savename);
	g_waiting = false;
	return;
    }
    for (int i = 0; i < s_num_lines; i++) {
	if (s_lines[i])
	    fprintf(tmpfp,"%s\n",s_lines[i]);
	if (ferror(tmpfp)) {
	    fclose(tmpfp);
	    free(savename);
	    printf("\nWrite error in temporary save file %s\n",s_lbuf) FLUSH;
	    printf("(keeping old saved scores)\n");
	    remove(s_lbuf);
	    g_waiting = false;
	    return;
	}
    }
    fclose(tmpfp);
    remove(savename);
    rename(s_lbuf,savename);
    g_waiting = false;
}

/* returns the next article number (after the last one used) */
//ART_NUM a;	/* art number to start with */
ART_NUM sc_sv_use_line(char *line, ART_NUM a)
{
    char*p;
    char c1, c2;
    int  x;

    int   score = 0; /* get rid of warning */
    char *s = line;
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
	    s = skip_digits(s);
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
	    s = skip_digits(s);
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
		s = skip_digits(s);
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
		s = skip_digits(s);
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
    bool lastscore_valid = false;
    int  num_output = 0;
    int  i;
    bool neg_flag;

    char *s = s_lbuf;
    *s++ = '.';
    int lastscore = 0;

    for (ART_NUM art = article_first(a); art <= g_lastart && num_output < 50; art = article_next(art)) {
	if (article_unread(art) && article_scored(art)) {
	    if (s_last != art-1) {
		if (s_last == art-2) {
		    *s++ = 's';
		    num_output++;
		} else {
		    sprintf(s,"s%ld",(art-s_last)-1);
		    s = s_lbuf + strlen(s_lbuf);
		    num_output++;
		}
	    }
	    /* print article's score */
	    int score = article_ptr(art)->score;
	    /* check for repeating scores */
	    if (score == lastscore && lastscore_valid) {
		art = article_next(art);
		for (i = 1; art <= g_lastart && article_unread(art) && article_scored(art)
			 && article_ptr(art)->score == score; i++)
		    art = article_next(art);
		art = article_prev(art);	/* prepare for the for loop increment */
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
	    s_last = art;
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
    char*   s;

    s_sc_save_new = -1;		/* just in case we exit early */
    s_loaded = 0;
    s_used = 0;
    g_sc_loaded_count = 0;

    /* verbosity is only really useful for debugging... */
    bool verbose = false;

    if (s_num_lines == 0)
	sc_sv_getfile();

    char *gname = savestr(filexp("%C"));

    int i;
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
    int total = 0;
    int scored = 0;
    for (ART_NUM art = article_first(a); art <= g_lastart; art = article_next(art)) {
	if (!article_exists(art))
	    continue;
        if (!article_unread(art) && !g_sa_mode_read_elig)
	    continue;
	total++;
	if (article_scored(art))
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
    s_saved = 0;
    s_last = 0;

    g_waiting = true;	/* DON'T interrupt */
    char *gname = savestr(filexp("%C"));
    /* not being able to open is OK */
    if (s_num_lines > 0) {
	sc_sv_delgroup(gname);	/* delete old group */
    } else {		/* there was no old file */
	sc_sv_add("#STRN saved score file.");
	sc_sv_add("v1.0");
    }
    sprintf(s_lbuf2,"!%s",gname);	/* add the header */
    sc_sv_add(s_lbuf2);

    ART_NUM a = g_firstart;
    sprintf(s_lbuf2,":%ld",a);
    sc_sv_add(s_lbuf2);
    s_last = a-1;
    while (a <= g_lastart)
	a = sc_sv_make_line(a);
    g_waiting = false;
}
