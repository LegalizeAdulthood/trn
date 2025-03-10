/* rcln.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "common.h"
#include "rcln.h"

#include "datasrc.h"
#include "ngdata.h"
#include "rcstuff.h"
#include "string-algos.h"
#include "terminal.h"
#include "trn.h"
#include "util.h"
#include "util2.h"

enum
{
    MAX_DIGITS = 7
};

bool g_toread_quiet{};

void rcln_init()
{
}

void catch_up(NGDATA *np, int leave_count, int output_level)
{
    char tmpbuf[128];

    if (leave_count) {
        if (output_level) {
            if (g_verbose)
            {
                printf("\nMarking all but %d articles in %s as read.\n",
                      leave_count,np->rcline);
            }
            else
            {
                printf("\nAll but %d marked as read.\n", leave_count);
            }
        }
        checkexpired(np, getngsize(np) - leave_count + 1);
        set_toread(np, ST_STRICT);
    }
    else {
        if (output_level) {
            if (g_verbose)
            {
                printf("\nMarking %s as all read.\n", np->rcline);
            }
            else
            {
                fputs("\nMarked read\n", stdout);
            }
        }
        sprintf(tmpbuf,"%s: 1-%ld", np->rcline,(long)getngsize(np));
        free(np->rcline);
        np->rcline = savestr(tmpbuf);
        *(np->rcline + np->numoffset - 1) = '\0';
        if (g_ng_min_toread > TR_NONE && np->toread > TR_NONE)
        {
            g_newsgroup_toread--;
        }
        np->toread = TR_NONE;
    }
    np->rc->flags |= RF_RCCHANGED;
    if (!write_newsrcs(g_multirc))
    {
        get_anything();
    }
}

/* add an article number to a newsgroup, if it isn't already read */

int addartnum(DATASRC *dp, ART_NUM artnum, const char *ngnam)
{
    char*   s;
    char*   t;
    char*   maxt = nullptr;
    ART_NUM min = 0;
    ART_NUM max = -1;
    ART_NUM lastnum = 0;
    char*   mbuf;
    bool    morenum;

    if (!artnum)
    {
        return 0;
    }
    NGDATA *np = find_ng(ngnam);
    if (np == nullptr)                  /* not found in newsrc? */
    {
        return 0;
    }
    if (dp != np->rc->datasrc) {        /* punt on cross-host xrefs */
#ifdef DEBUG
        if (debug & DEB_XREF_MARKER)
        {
            printf("Cross-host xref to group %s ignored.\n",ngnam);
        }
#endif
        return 0;
    }
    if (!np->numoffset)
    {
        return 0;
    }
#ifndef ANCIENT_NEWS
    if (!np->abs1st) {
        /* Trim down the list due to expires if we haven't done so yet. */
        set_toread(np, ST_LAX);
    }
#endif
#if 0
    if (artnum > np->ngmax + 200) {     /* allow for incoming articles */
        printf("\nCorrupt Xref line!!!  %ld --> %s(1..%ld)\n",
            artnum,ngnam,
            np->ngmax);
        g_paranoid = true;              /* paranoia reigns supreme */
        return -1;                      /* hope this was the first newsgroup */
    }
#endif

    if (np->toread == TR_BOGUS)
    {
        return 0;
    }
    if (artnum > np->ngmax) {
        if (np->toread > TR_NONE)
        {
            np->toread += artnum - np->ngmax;
        }
        np->ngmax = artnum;
    }
#ifdef DEBUG
    if (debug & DEB_XREF_MARKER) {
        printf("%ld->\n%s%c%s\n",(long)artnum,np->rcline, np->subscribechar,
          np->rcline + np->numoffset);
    }
#endif
    s = skip_eq(np->rcline + np->numoffset, ' '); /* skip spaces */
    t = s;
    while (isdigit(*s) && artnum >= (min = atol(s))) {
                                        /* while it might have been read */
        t = skip_digits(s);             /* skip number */
        if (*t == '-') {                /* is it a range? */
            t++;                        /* skip to next number */
            if (artnum <= (max = atol(t)))
            {
                return 0;               /* it is in range => already read */
            }
            lastnum = max;              /* remember it */
            maxt = t;                   /* remember position in case we */
                                        /* want to overwrite the max */
            t = skip_digits(t);         /* skip second number */
        }
        else {
            if (artnum == min)          /* explicitly a read article? */
            {
                return 0;
            }
            lastnum = min;              /* remember what the number was */
            maxt = nullptr;             /* last one was not a range */
        }
        while (*t && !isdigit(*t))
        {
            t++;                        /* skip comma and any spaces */
        }
        s = t;
    }

    /* we have not read it, so insert the article number before s */

    morenum = isdigit(*s);              /* will it need a comma after? */
    *(np->rcline + np->numoffset - 1) = np->subscribechar;
    mbuf = safemalloc((MEM_SIZE)(strlen(s)+(s - np->rcline)+MAX_DIGITS+2+1));
    strcpy(mbuf,np->rcline);            /* make new rc line */
    if (maxt && lastnum && artnum == lastnum+1)
                                        /* can we just extend last range? */
    {
        t = mbuf + (maxt - np->rcline); /* then overwrite previous max */
    }
    else {
        t = mbuf + (t - np->rcline);    /* point t into new line instead */
        if (lastnum) {                  /* have we parsed any line? */
            if (!morenum)               /* are we adding to the tail? */
            {
                *t++ = ',';             /* supply comma before */
            }
            if (!maxt && artnum == lastnum+1 && *(t-1) == ',')
                                        /* adjacent singletons? */
            {
                *(t-1) = '-';           /* turn them into a range */
            }
        }
    }
    if (morenum) {                      /* is there more to life? */
        if (min == artnum+1) {          /* can we consolidate further? */
            bool range_before = (*(t-1) == '-');
            char *nextmax = skip_digits(s);
            bool  range_after = *nextmax++ == '-';

            if (range_before)
            {
                *t = '\0';              /* artnum is redundant */
            }
            else
            {
                sprintf(t,"%ld-",(long)artnum);/* artnum will be new min */
            }

            if (range_after)
            {
                s = nextmax;            /* *s is redundant */
            }
        }
        else
        {
            sprintf(t,"%ld,",(long)artnum);     /* put the number and comma */
        }
    }
    else
    {
        sprintf(t,"%ld",(long)artnum);  /* put the number there (wherever) */
    }
    strcat(t,s);                        /* copy remainder of line */
#ifdef DEBUG
    if (debug & DEB_XREF_MARKER)
    {
        printf("%s\n",mbuf);
    }
#endif
    free(np->rcline);
    np->rcline = mbuf;          /* pull the switcheroo */
    *(np->rcline + np->numoffset - 1) = '\0';
                                        /* wipe out : or ! */
    if (np->toread > TR_NONE)   /* lest we turn unsub into bogus */
    {
        np->toread--;
    }
    return 0;
}

/* delete an article number from a newsgroup, if it is there */

#ifdef MCHASE
void subartnum(DTASRC *dp, ART_NUM artnum, char *ngnam)
{
    NGDATA* np;
    char* s;
    char* t;
    ART_NUM min, max;
    char* mbuf;
    int curlen;

    if (!artnum)
    {
        return;
    }
    np = find_ng(ngnam);
    if (np == nullptr)                  /* not found in newsrc? */
    {
        return;
    }
    if (dp != np->rc->g_datasrc)                /* punt on cross-host xrefs */
    {
        return;
    }
    if (!np->numoffset)
    {
        return;
    }
#ifdef DEBUG
    if (debug & DEB_XREF_MARKER) {
        printf("%ld<-\n%s%c%s\n",(long)artnum,np->rcline,np->subscribechar,
          np->rcline + np->numoffset);
    }
#endif
    s = np->rcline + np->numoffset;
    s = skip_eq(s, ' ');                /* skip spaces */

    /* a little optimization, since it is almost always the last number */

    for (t=s; *t; t++)                  /* find end of string */
    {
    }
    curlen = t - np->rcline;
    for (t--; isdigit(*t); t--)         /* find previous delim */
    {
    }
    if (*t == ',' && atol(t+1) == artnum) {
        *t = '\0';
        if (np->toread >= TR_NONE)
        {
            ++np->toread;
        }
#ifdef DEBUG
        if (debug & DEB_XREF_MARKER)
        {
            printf("%s%c %s\n",np->rcline,np->subscribechar,s);
        }
#endif
        return;
    }

    /* not the last number, oh well, we may need the length anyway */

    while (isdigit(*s) && artnum >= (min = atol(s))) {
                                        /* while it might have been read */
        t = skip_digits(s);             /* skip number */
        if (*t == '-') {                /* is it a range? */
            t++;                        /* skip to next number */
            max = atol(t);
            t = skip_digit(t);          /* skip second number */
            if (artnum <= max) {
                                        /* it is in range => already read */
                if (artnum == min) {
                    min++;
                    artnum = 0;
                }
                else if (artnum == max) {
                    max--;
                    artnum = 0;
                }
                *(np->rcline + np->numoffset - 1) = np->subscribechar;
                mbuf = safemalloc((MEM_SIZE)(curlen+(artnum?(MAX_DIGITS+1)*2+1:1+1)));
                *s = '\0';
                strcpy(mbuf,np->rcline);        /* make new rc line */
                s = mbuf + (s - np->rcline);
                                        /* point s into mbuf now */
                if (artnum) {           /* split into two ranges? */
                    prange(s,min,artnum-1);
                    s += strlen(s);
                    *s++ = ',';
                    prange(s,artnum+1,max);
                }
                else                    /* only one range */
                {
                    prange(s,min,max);
                }
                strcat(s,t);            /* copy remainder over */
#ifdef DEBUG
                if (debug & DEB_XREF_MARKER) {
                    printf("%s\n",mbuf);
                }
#endif
                free(np->rcline);
                np->rcline = mbuf;      /* pull the switcheroo */
                *(np->rcline + np->numoffset - 1) = '\0';
                                        /* wipe out : or ! */
                if (np->toread >= TR_NONE)
                {
                    np->toread++;
                }
                return;
            }
        }
        else {
            if (artnum == min) {        /* explicitly a read article? */
                if (*t == ',')          /* pick a comma, any comma */
                {
                    t++;
                }
                else if (s[-1] == ',')
                {
                    s--;
                }
                else if (s[-2] == ',')  /* (in case of space) */
                {
                    s -= 2;
                }
                strcpy(s,t);            /* no need to realloc */
                if (np->toread >= TR_NONE)
                {
                    np->toread++;
                }
#ifdef DEBUG
                if (debug & DEB_XREF_MARKER) {
                    printf("%s%c%s\n",np->rcline,np->subscribechar,
                      np->rcline + np->numoffset);
                }
#endif
                return;
            }
        }
        while (*t && !isdigit(*t))      /* skip comma and any spaces */
        {
            t++;
        }
        s = t;
    }
}

void prange(char *where, ART_NUM min, ART_NUM max)
{
    if (min == max)
    {
        sprintf(where,"%ld",(long)min);
    }
    else
    {
        sprintf(where,"%ld-%ld",(long)min,(long)max);
    }
}
#endif

/* calculate the number of unread articles for a newsgroup */

void set_toread(NGDATA *np, bool lax_high_check)
{
    char*   c;
    char    tmpbuf[64];
    char*   mybuf = tmpbuf;
    bool    virgin_ng = (!np->abs1st);
    ART_NUM ngsize = getngsize(np);
    ART_NUM unread = ngsize;
    ART_NUM newmax;

    if (ngsize == TR_BOGUS) {
        if (!g_toread_quiet) {
            printf("\nInvalid (bogus) newsgroup found: %s\n",np->rcline);
        }
        g_paranoid = true;
        if (virgin_ng || np->toread >= g_ng_min_toread) {
            g_newsgroup_toread--;
            g_missing_count++;
        }
        np->toread = TR_BOGUS;
        return;
    }
    if (virgin_ng) {
        sprintf(tmpbuf," 1-%ld",(long)ngsize);
        if (strcmp(tmpbuf,np->rcline+np->numoffset) != 0)
        {
            checkexpired(np,np->abs1st);        /* this might realloc rcline */
        }
    }
    char *nums = np->rcline + np->numoffset;
    int   length = strlen(nums);
    if (length+MAX_DIGITS+1 > sizeof tmpbuf)
    {
        mybuf = safemalloc((MEM_SIZE) (length + MAX_DIGITS + 1));
    }
    strcpy(mybuf,nums);
    mybuf[length++] = ',';
    mybuf[length] = '\0';
    char *s = skip_space(mybuf);
    for ( ; (c = strchr(s,',')) != nullptr ; s = ++c) {  /* for each range */
        *c = '\0';                  /* keep index from running off */
        char *h = strchr(s, '-');
        if (h != nullptr) /* find - in range, if any */
        {
            unread -= (newmax = atol(h + 1)) - atol(s) + 1;
        }
        else
        {
            newmax = atol(s);
        }
        if (newmax != 0)
        {
            unread--;           /* recalculate length */
        }
        if (newmax > ngsize) {  /* paranoia check */
            if (!lax_high_check && newmax > ngsize) {
                unread = -1;
                break;
            }
            unread += newmax - ngsize;
            np->ngmax = newmax;
            ngsize = newmax;
        }
    }
    if (unread < 0) {                   /* SOMEONE RESET THE NEWSGROUP!!! */
        unread = (ART_UNREAD)ngsize;    /* assume nothing carried over */
        if (!g_toread_quiet) {
            printf("\nSomebody reset %s -- assuming nothing read.\n",
                   np->rcline);
        }
        *(np->rcline + np->numoffset) = '\0';
        g_paranoid = true;          /* enough to make a guy paranoid */
        np->rc->flags |= RF_RCCHANGED;
    }
    if (np->subscribechar == NEGCHAR)
    {
        unread = TR_UNSUB;
    }

    if (unread >= g_ng_min_toread) {
        if (!virgin_ng && np->toread < g_ng_min_toread)
        {
            g_newsgroup_toread++;
        }
    }
    else if (unread <= 0) {
        if (np->toread > g_ng_min_toread) {
            g_newsgroup_toread--;
            if (virgin_ng)
            {
                g_missing_count++;
            }
        }
    }
    np->toread = (ART_UNREAD)unread;    /* remember how many are left */

    if (mybuf != tmpbuf)
    {
        free(mybuf);
    }
}

/* make sure expired articles are marked as read */

void checkexpired(NGDATA *np, ART_NUM a1st)
{
    char* s;
    ART_NUM num, lastnum = 0;
    char* mbuf;
    char* cp;
    int len;

    if (a1st<=1)
    {
        return;
    }
#ifdef DEBUG
    if (debug & DEB_XREF_MARKER) {
        printf("1-%ld->\n%s%c%s\n",(long)(a1st-1),np->rcline,np->subscribechar,
          np->rcline + np->numoffset);
    }
#endif
    s = skip_space(np->rcline + np->numoffset);
    while (*s && (num = atol(s)) <= a1st) {
        s = skip_digits(s);
        while (*s && !isdigit(*s))
        {
            s++;
        }
        lastnum = num;
    }
    len = strlen(s);
    if (len && s[-1] == '-') {                  /* landed in a range? */
        if (lastnum != 1) {
            if (3+len <= (int)strlen(np->rcline+np->numoffset))
            {
                mbuf = np->rcline;
            }
            else {
                mbuf = safemalloc((MEM_SIZE)(np->numoffset+3+len+1));
                strcpy(mbuf, np->rcline);
            }
            cp = mbuf + np->numoffset;
            *cp++ = ' ';
            *cp++ = '1';
            *cp++ = '-';
            safecpy(cp, s, len+1);
            if (np->rcline != mbuf) {
                free(np->rcline);
                np->rcline = mbuf;
            }
            np->rc->flags |= RF_RCCHANGED;
        }
    }
    else {
        /* s now points to what should follow the first range */
        char numbuf[32];

        sprintf(numbuf," 1-%ld",(long)(a1st - (lastnum != a1st)));
        int nlen = strlen(numbuf) + (len != 0);

        if (s - np->rcline >= np->numoffset + nlen)
        {
            mbuf = np->rcline;
        }
        else {
            mbuf = safemalloc((MEM_SIZE)(np->numoffset+nlen+len+1));
            strcpy(mbuf,np->rcline);
        }

        cp = mbuf + np->numoffset;
        strcpy(cp, numbuf);
        cp += nlen;

        if (len) {
            cp[-1] = ',';
            if (cp != s)
            {
                safecpy(cp, s, len + 1);
            }
        }

        if (!g_checkflag && np->rcline == mbuf)
        {
            np->rcline = saferealloc(np->rcline, (MEM_SIZE) (cp - mbuf + len + 1));
        }
        else {
            if (!g_checkflag)
            {
                free(np->rcline);
            }
            np->rcline = mbuf;
        }
        np->rc->flags |= RF_RCCHANGED;
    }

#ifdef DEBUG
    if (debug & DEB_XREF_MARKER) {
        printf("%s%c%s\n",np->rcline,np->subscribechar,
          np->rcline + np->numoffset);
    }
#endif
}

/* Returns true if article is marked as read or does not exist */
/* could use a better name */
bool was_read_group(DATASRC *dp, ART_NUM artnum, char *ngnam)
{
    char*   s;
    char*   t;
    ART_NUM min = 0;
    ART_NUM max = -1;

    if (!artnum)
    {
        return true;
    }
    NGDATA *np = find_ng(ngnam);
    if (np == nullptr)          /* not found in newsrc? */
    {
        return true;
    }
    if (!np->numoffset)         /* no numbers on line */
    {
        return false;
    }
#if 0
    /* consider this code later */
    if (!np->abs1st) {
        /* Trim down the list due to expires if we haven't done so yet. */
        set_toread(np, ST_LAX);
    }
#endif

    if (np->toread == TR_BOGUS)
    {
        return true;
    }
    if (artnum > np->ngmax) {
        return false;           /* probably doesn't exist, however */
    }
    s = skip_eq(np->rcline + np->numoffset, ' '); /* skip spaces */
    t = s;
    while (isdigit(*s) && artnum >= (min = atol(s))) {
        char*   maxt = nullptr;
        ART_NUM lastnum = 0;
        /* while it might have been read */
        t = skip_digits(s);             /* skip number */
        if (*t == '-') {                /* is it a range? */
            t++;                        /* skip to next number */
            if (artnum <= (max = atol(t)))
            {
                return true;            /* it is in range => already read */
            }
            lastnum = max;              /* remember it */
            maxt = t;                   /* remember position in case we */
                                        /* want to overwrite the max */
            t = skip_digits(t);         /* skip second number */
        }
        else {
            if (artnum == min)          /* explicitly a read article? */
            {
                return true;
            }
            lastnum = min;              /* remember what the number was */
            maxt = nullptr;             /* last one was not a range */
        }
        while (*t && !isdigit(*t))
        {
            t++;                        /* skip comma and any spaces */
        }
        s = t;
    }

    /* we have not read it, so return false */
    return false;
}
