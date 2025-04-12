/* respond.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <config/string_case_compare.h>

#include "config/common.h"
#include "respond.h"

#include "art.h"
#include "artio.h"
#include "artstate.h"
#include "change_dir.h"
#include "charsubst.h"
#include "datasrc.h"
#include "decode.h"
#include "util/env.h"
#include "final.h"
#include "head.h"
#include "intrp.h"
#include "mime.h"
#include "ng.h"
#include "ngdata.h"
#include "nntp.h"
#include "nntp/nntpclient.h"
#include "string-algos.h"
#include "terminal.h"
#include "trn.h"
#include "config/user_id.h"
#include "util.h"
#include "util/util2.h"
#include "uudecode.h"

std::string g_savedest;      /* value of %b */
std::string g_extractdest;   /* value of %E */
std::string g_extractprog;   /* value of %e */
ART_POS     g_savefrom{};    /* value of %B */
bool        g_mbox_always{}; /* -M */
bool        g_norm_always{}; /* -N */
std::string g_privdir;       /* private news directory */
std::string g_indstr{">"};   /* indent for old article embedded in followup */

static char  s_nullart[] = "\nEmpty article.\n";
static FILE *s_tmpfp{};

static void follow_it_up();
#if 0
static bool cut_line(char *str);
#endif

void respond_init()
{
    g_savedest.clear();
    g_extractdest.clear();
}

save_result save_article()
{
    char* s;
    char* c;
    char altbuf[CBUFLEN];
    bool interactive = (g_buf[1] == FINISHCMD);
    char cmd = *g_buf;

    if (!finish_command(interactive))   /* get rest of command */
    {
        return SAVE_ABORT;
    }
    bool use_pref = isupper(cmd);
    if (use_pref != 0)
    {
        cmd = tolower(cmd);
    }
    parseheader(g_art);
    mime_SetArticle();
    clear_artbuf();
    g_savefrom = (cmd == 'w' || cmd == 'e')? g_htype[PAST_HEADER].minpos : 0;
    if (artopen(g_art,g_savefrom) == nullptr) {
        if (g_verbose)
        {
            fputs("\nCan't save an empty article.\n", stdout);
        }
        else
        {
            fputs(s_nullart, stdout);
        }
        termdown(2);
        return SAVE_DONE;
    }
    if (change_dir(g_privdir)) {
        printf(g_nocd,g_privdir.c_str());
        sig_catcher(0);
    }
    if (cmd == 'e') {           /* is this an extract command? */
        static bool custom_extract = false;
        char*       cmdstr;
        int         partOpt = 0;
        int         totalOpt = 0;

        s = g_buf+1;            /* skip e */
        s = skip_eq(s, ' ');    /* skip leading spaces */
        if (*s == '-' && isdigit(s[1])) {
            partOpt = atoi(s+1);
            do
            {
                s++;
            } while (isdigit(*s));
            if (*s == '/') {
                ++s;
                totalOpt = atoi(s);
                s = skip_digits(s);
                s = skip_eq(s, ' ');
            }
            else
            {
                totalOpt = partOpt;
            }
        }
        safecpy(altbuf,filexp(s),sizeof altbuf);
        s = altbuf;
        if (*s) {
            cmdstr = cpytill(g_buf,s,'|');      /* check for | */
            s = g_buf + strlen(g_buf)-1;
            while (*s == ' ')
            {
                s--;                            /* trim trailing spaces */
            }
            *++s = '\0';
            if (*cmdstr) {
                s = cmdstr+1;                   /* skip | */
                s = skip_eq(s, ' ');
                if (*s) {                       /* if new command, use it */
                    g_extractprog = s;          /* put extracter in %e */
                }
                else
                {
                    static char cmdbuff[1024];
                    strcpy(cmdbuff, g_extractprog.c_str());
                    cmdstr = cmdbuff;
                }
            }
            else
            {
                cmdstr = nullptr;
            }
            s = g_buf;
        }
        else {
            if (!g_extractdest.empty())
            {
                strcpy(s, g_extractdest.c_str());
            }
            if (custom_extract)
            {
                static char cmdbuff[1024];
                strcpy(cmdbuff, g_extractprog.c_str());
                cmdstr = cmdbuff;
            }
            else
            {
                cmdstr = nullptr;
            }
        }
        custom_extract = (cmdstr != nullptr);

        if (!FILE_REF(s)) {     /* relative path? */
            c = (s==g_buf ? altbuf : g_buf);
            interp(c, (sizeof g_buf), get_val("SAVEDIR",SAVEDIR));
            if (makedir(c, MD_DIR))      /* ensure directory exists */
            {
                strcpy(c,g_privdir.c_str());
            }
            if (*s) {
                while (*c)
                {
                    c++;
                }
                *c++ = '/';
                strcpy(c,s);            /* add filename */
            }
            s = (s==g_buf ? altbuf : g_buf);
        }
        if (!FILE_REF(s)) {     /* path still relative? */
            c = (s==g_buf ? altbuf : g_buf);
            sprintf(c, "%s/%s", g_privdir.c_str(), s);
            s = c;                      /* absolutize it */
        }
        g_extractdest = s; /* make it handy for %E */
        {
            static char buff[512];
            strcpy(buff, g_extractdest.c_str());
            s = buff;
        }
        if (makedir(s, MD_DIR)) {       /* ensure directory exists */
            g_int_count++;
            return SAVE_DONE;
        }
        if (change_dir(s)) {
            printf(g_nocd,s);
            sig_catcher(0);
        }
        c = trn_getwd(g_buf, sizeof(g_buf));    /* simplify path for output */
        if (custom_extract) {
            printf("Extracting article into %s using %s\n",c,g_extractprog.c_str());
            termdown(1);
            interp(g_cmd_buf, sizeof g_cmd_buf, get_val("CUSTOMSAVER",CUSTOMSAVER));
            invoke(g_cmd_buf, nullptr);
        }
        else if (g_is_mime) {
            printf("Extracting MIME article into %s:\n", c);
            termdown(1);
            mime_DecodeArticle(false);
        }
        else {
            char *filename;
            int   part;
            int   total;
            int decode_type = 0;
            int cnt = 0;

            /* Scan subject for filename and part number information */
            filename = decode_subject(g_art, &part, &total);
            if (partOpt)
            {
                part = partOpt;
            }
            if (totalOpt)
            {
                total = totalOpt;
            }
            for (g_artpos = g_savefrom;
                 readart(g_art_line,sizeof g_art_line) != nullptr;
                 g_artpos = tellart())
            {
                if (*g_art_line <= ' ')
                {
                    continue;   /* Ignore empty or initially-whitespace lines */
                }
                if (((*g_art_line == '#' || *g_art_line == ':')
                  && (!strncmp(g_art_line+1, "! /bin/sh", 9)
                   || !strncmp(g_art_line+1, "!/bin/sh", 8)
                   || !strncmp(g_art_line+2, "This is ", 8)))
#if 0
                 || !strncmp(g_art_line, "sed ", 4)
                 || !strncmp(g_art_line, "cat ", 4)
                 || !strncmp(g_art_line, "echo ", 5)
#endif
                ) {
                    g_savefrom = g_artpos;
                    decode_type = 1;
                    break;
                }
                if (uue_prescan(g_art_line,&filename,&part,&total)) {
                    g_savefrom = g_artpos;
                    seekart(g_savefrom);
                    decode_type = 2;
                    break;
                }
                if (++cnt == 300)
                {
                    break;
                }
            }/* for */
            switch (decode_type) {
              case 1:
                printf("Extracting shar into %s:\n", c);
                termdown(1);
                interp(g_cmd_buf,(sizeof g_cmd_buf),get_val("SHARSAVER",SHARSAVER));
                invoke(g_cmd_buf, nullptr);
                break;
              case 2:
                printf("Extracting uuencoded file into %s:\n", c);
                termdown(1);
                g_mime_section->type = IMAGE_MIME;
                safefree(g_mime_section->filename);
                g_mime_section->filename = filename? savestr(filename) : nullptr;
                g_mime_section->encoding = MENCODE_UUE;
                g_mime_section->part = part;
                g_mime_section->total = total;
                if (!decode_piece(nullptr,nullptr) && *g_msg) {
                    newline();
                    fputs(g_msg,stdout);
                }
                newline();
                break;
              default:
                printf("Unable to determine type of file.\n");
                termdown(1);
                break;
            }
        }/* if */
    }
    else if ((s = strchr(g_buf,'|')) != nullptr) { /* is it a pipe command? */
        s++;                    /* skip the | */
        s = skip_eq(s, ' ');
        safecpy(altbuf,filexp(s),sizeof altbuf);
        g_savedest = altbuf;
        if (g_datasrc->flags & DF_REMOTE)
        {
            nntp_finishbody(FB_SILENT);
        }
        interp(g_cmd_buf, (sizeof g_cmd_buf), get_val("PIPESAVER",PIPESAVER));
                                /* then set up for command */
        termlib_reset();
        resetty();              /* restore tty state */
        if (use_pref)           /* use preferred shell? */
        {
            doshell(nullptr,g_cmd_buf);
                                /* do command with it */
        }
        else
        {
            doshell(SH,g_cmd_buf);  /* do command with sh */
        }
        noecho();               /* and stop echoing */
        crmode();               /* and start cbreaking */
        termlib_init();
    }
    else {                      /* normal save */
        bool  there;
        bool  mailbox;
        char * savename = get_val("SAVENAME",SAVENAME);

        s = g_buf+1;            /* skip s or S */
        if (*s == '-') {        /* if they are confused, skip - also */
            if (g_verbose)
            {
                fputs("Warning: '-' ignored.  This isn't readnews.\n", stdout);
            }
            else
            {
                fputs("'-' ignored.\n", stdout);
            }
            termdown(1);
            s++;
        }
        for (; *s == ' '; s++)
        {
            /* skip spaces */
        }
        safecpy(altbuf,filexp(s),sizeof altbuf);
        s = altbuf;
        if (!FILE_REF(s)) {
            interp(g_buf, (sizeof g_buf), get_val("SAVEDIR",SAVEDIR));
            if (makedir(g_buf, MD_DIR))  /* ensure directory exists */
            {
                strcpy(g_buf, g_privdir.c_str());
            }
            if (*s) {
                for (c = g_buf; *c; c++)
                {
                }
                *c++ = '/';
                strcpy(c,s);            /* add filename */
            }
            s = g_buf;
        }
        stat_t save_dir_stat{};
        for (int i = 0;
            (there = stat(s,&save_dir_stat) >= 0) && S_ISDIR(save_dir_stat.st_mode);
            i++) {                      /* is it a directory? */

            c = (s+strlen(s));
            *c++ = '/';                 /* put a slash before filename */
            static char s_news[] = "News";
            interp(c, s == g_buf ? (sizeof g_buf) : (sizeof altbuf), i ? s_news : savename);
                                /* generate a default name somehow or other */
        }
        makedir(s, MD_FILE);
        if (!FILE_REF(s)) {     /* relative path? */
            c = (s==g_buf ? altbuf : g_buf);
            sprintf(c, "%s/%s", g_privdir.c_str(), s);
            s = c;                      /* absolutize it */
        }
        g_savedest = s; /* make it handy for %b */
        s_tmpfp = nullptr;
        if (!there) {
            if (g_mbox_always)
            {
                mailbox = true;
            }
            else if (g_norm_always)
            {
                mailbox = false;
            }
            else {
                const char* dflt = (in_string(savename,"%a", true) ? "nyq" : "ynq");

                sprintf(g_cmd_buf,
                "\nFile %s doesn't exist--\n        use mailbox format?",s);
              reask_save:
                in_char(g_cmd_buf, MM_USE_MAILBOX_FORMAT_PROMPT, dflt);
                newline();
                printcmd();
                if (*g_buf == 'h') {
                    if (g_verbose)
                    {
                        printf("\n"
                               "Type y to create %s as a mailbox.\n"
                               "Type n to create it as a normal file.\n"
                               "Type q to abort the save.\n",
                               s);
                    }
                    else
                    {
                        fputs("\n"
                              "y to create mailbox.\n"
                              "n to create normal file.\n"
                              "q to abort.\n",
                              stdout);
                    }
                    termdown(4);
                    goto reask_save;
                }
                else if (*g_buf == 'n') {
                    mailbox = false;
                }
                else if (*g_buf == 'y') {
                    mailbox = true;
                }
                else if (*g_buf == 'q') {
                    goto s_bomb;
                }
                else {
                    fputs(g_hforhelp,stdout);
                    termdown(1);
                    settle_down();
                    goto reask_save;
                }
            }
        }
        else if (S_ISCHR(save_dir_stat.st_mode))
        {
            mailbox = false;
        }
        else {
            s_tmpfp = fopen(s,"r+");
            if (!s_tmpfp)
            {
                mailbox = false;
            }
            else {
                if (fread(g_buf,1,LBUFLEN,s_tmpfp)) {
                    c = g_buf;
                    if (!isspace(MBOXCHAR))   /* if non-zero, */
                    {
                        c = skip_space(c);   /* check the first character */
                    }
                    mailbox = (*c == MBOXCHAR);
                } else
                {
                    mailbox = g_mbox_always;    /* if zero length, recheck -M */
                }
            }
        }

        s = get_val(mailbox ? "MBOXSAVER" : "NORMSAVER");
        int i;
        if (s) {
            if (s_tmpfp)
            {
                fclose(s_tmpfp);
            }
            safecpy(g_cmd_buf, filexp(s), sizeof g_cmd_buf);
            if (g_datasrc->flags & DF_REMOTE)
            {
                nntp_finishbody(FB_SILENT);
            }
            termlib_reset();
            resetty();          /* make terminal behave */
            i = doshell(use_pref?nullptr:SH,g_cmd_buf);
            termlib_init();
            noecho();           /* make terminal do what we want */
            crmode();
        }
        else if (s_tmpfp != nullptr || (s_tmpfp = fopen(g_savedest.c_str(), "a")) != nullptr) {
            bool quote_From = false;
            fseek(s_tmpfp,0,2);
            if (mailbox) {
#if MBOXCHAR == '\001'
                fprintf(s_tmpfp,"\001\001\001\001\n");
#else
                interp(g_cmd_buf, sizeof g_cmd_buf, "From %t %`LANG= date`\n");
                fputs(g_cmd_buf, s_tmpfp);
                quote_From = true;
#endif
            }
            if (g_savefrom == 0 && g_art != 0)
            {
                fprintf(s_tmpfp, "Article: %ld of %s\n", g_art, g_ngname.c_str());
            }
            seekart(g_savefrom);
            while (readart(g_buf,LBUFLEN) != nullptr) {
                if (quote_From && string_case_equal(g_buf, "from ",5))
                {
                    putc('>', s_tmpfp);
                }
                fputs(g_buf, s_tmpfp);
            }
            fputs("\n\n", s_tmpfp);
#if MBOXCHAR == '\001'
            if (mailbox)
            {
                fprintf(s_tmpfp,"\001\001\001\001\n");
            }
#endif
            fclose(s_tmpfp);
            i = 0; /*$$ set non-zero on write error */
        }
        else
        {
            i = 1;
        }
        if (i)
        {
            fputs("Not saved", stdout);
        }
        else {
            printf("%s to %s %s", there? "Appended" : "Saved",
                   mailbox? "mailbox" : "file", g_savedest.c_str());
        }
        if (interactive)
        {
            newline();
        }
    }
s_bomb:
    chdir_newsdir();
    return SAVE_DONE;
}

save_result view_article()
{
    parseheader(g_art);
    mime_SetArticle();
    clear_artbuf();
    g_savefrom = g_htype[PAST_HEADER].minpos;
    if (artopen(g_art,g_savefrom) == nullptr) {
        if (g_verbose)
        {
            fputs("\nNo attatchments on an empty article.\n", stdout);
        }
        else
        {
            fputs(s_nullart, stdout);
        }
        termdown(2);
        return SAVE_DONE;
    }
    printf("Processing attachments...\n");
    termdown(1);
    if (g_is_mime)
    {
        mime_DecodeArticle(true);
    }
    else {
        char *filename;
        int   part;
        int   total;
        int cnt = 0;

        /* Scan subject for filename and part number information */
        filename = decode_subject(g_art, &part, &total);
        for (g_artpos = g_savefrom;
             readart(g_art_line,sizeof g_art_line) != nullptr;
             g_artpos = tellart())
        {
            if (*g_art_line <= ' ')
            {
                continue;       /* Ignore empty or initially-whitespace lines */
            }
            if (uue_prescan(g_art_line, &filename, &part, &total)) {
                MIMECAP_ENTRY*mc = mime_FindMimecapEntry("image/jpeg", MCF_NONE); /*$$ refine this */
                g_savefrom = g_artpos;
                seekart(g_savefrom);
                g_mime_section->type = UNHANDLED_MIME;
                safefree(g_mime_section->filename);
                g_mime_section->filename = filename? savestr(filename) : nullptr;
                g_mime_section->encoding = MENCODE_UUE;
                g_mime_section->part = part;
                g_mime_section->total = total;
                if (mc && !decode_piece(mc,nullptr) && *g_msg) {
                    newline();
                    fputs(g_msg,stdout);
                }
                newline();
                cnt = 0;
            }
            else if (++cnt == 300)
            {
                break;
            }
        }/* for */
        if (cnt) {
            printf("Unable to determine type of file.\n");
            termdown(1);
        }
    }
    chdir_newsdir();
    return SAVE_DONE;
}

int cancel_article()
{
    char hbuf[5*LBUFLEN];
    int  myuid = current_user_id();
    int  r = -1;

    if (artopen(g_art,(ART_POS)0) == nullptr) {
        if (g_verbose)
        {
            fputs("\nCan't cancel an empty article.\n", stdout);
        }
        else
        {
            fputs(s_nullart, stdout);
        }
        termdown(2);
        return r;
    }
    char *reply_buf = fetchlines(g_art, REPLY_LINE);
    char *from_buf = fetchlines(g_art, FROM_LINE);
    char *ngs_buf = fetchlines(g_art, NEWSGROUPS_LINE);
    if (!string_case_equal(get_val_const("FROM",""),from_buf)
     && (!in_string(from_buf,g_hostname, false)
      || (!in_string(from_buf,g_login_name.c_str(), true)
       && !in_string(reply_buf,g_login_name.c_str(), true)
#ifdef HAS_NEWS_ADMIN
       && myuid != g_newsuid
#endif
       && myuid != ROOTID))) {
#ifdef DEBUG
        if (debug) {
            printf("\n%s@%s != %s\n",g_login_name.c_str(),g_hostname,from_buf);
            printf("%s != %s\n",get_val("FROM",""),from_buf);
            termdown(3);
        }
#endif
        if (g_verbose)
        {
            fputs("\nYou can't cancel someone else's article\n", stdout);
        }
        else
        {
            fputs("\nNot your article\n", stdout);
        }
        termdown(2);
    }
    else {
        FILE *header = fopen(g_headname.c_str(),"w");   /* open header file */
        if (header == nullptr) {
            printf(g_cantcreate,g_headname.c_str());
            termdown(1);
            goto done;
        }
        interp(hbuf, sizeof hbuf, get_val("CANCELHEADER",CANCELHEADER));
        fputs(hbuf,header);
        fclose(header);
        fputs("\nCanceling...\n",stdout);
        termdown(2);
        r = doshell(SH,filexp(get_val_const("CANCEL",CALL_INEWS)));
    }
done:
    free(ngs_buf);
    free(from_buf);
    free(reply_buf);
    return r;
}

int supersede_article()         /* Supersedes: */
{
    char hbuf[5*LBUFLEN];
    int  myuid = current_user_id();
    int  r = -1;
    bool incl_body = (*g_buf == 'Z');

    if (artopen(g_art,(ART_POS)0) == nullptr) {
        if (g_verbose)
        {
            fputs("\nCan't supersede an empty article.\n", stdout);
        }
        else
        {
            fputs(s_nullart, stdout);
        }
        termdown(2);
        return r;
    }
    char *reply_buf = fetchlines(g_art, REPLY_LINE);
    char *from_buf = fetchlines(g_art, FROM_LINE);
    char *ngs_buf = fetchlines(g_art, NEWSGROUPS_LINE);
    if (!string_case_equal(get_val_const("FROM",""),from_buf)
     && (!in_string(from_buf,g_hostname, false)
      || (!in_string(from_buf,g_login_name.c_str(), true)
       && !in_string(reply_buf,g_login_name.c_str(), true)
#ifdef HAS_NEWS_ADMIN
       && myuid != g_newsuid
#endif
       && myuid != ROOTID))) {
#ifdef DEBUG
        if (debug) {
            printf("\n%s@%s != %s\n",g_login_name.c_str(),g_hostname,from_buf);
            printf("%s != %s\n",get_val("FROM",""),from_buf);
            termdown(3);
        }
#endif
        if (g_verbose)
        {
            fputs("\nYou can't supersede someone else's article\n", stdout);
        }
        else
        {
            fputs("\nNot your article\n", stdout);
        }
        termdown(2);
    }
    else {
        FILE *header = fopen(g_headname.c_str(),"w");   /* open header file */
        if (header == nullptr) {
            printf(g_cantcreate,g_headname.c_str());
            termdown(1);
            goto done;
        }
        interp(hbuf, sizeof hbuf, get_val("SUPERSEDEHEADER",SUPERSEDEHEADER));
        fputs(hbuf,header);
        if (incl_body && g_artfp != nullptr) {
            parseheader(g_art);
            seekart(g_htype[PAST_HEADER].minpos);
            while (readart(g_buf,LBUFLEN) != nullptr)
            {
                fputs(g_buf, header);
            }
        }
        fclose(header);
        follow_it_up();
        r = 0;
    }
done:
    free(ngs_buf);
    free(from_buf);
    free(reply_buf);
    return r;
}

static int nntp_date()
{
    return nntp_command("DATE");
}

static void follow_it_up()
{
    safecpy(g_cmd_buf,filexp(get_val_const("NEWSPOSTER",NEWSPOSTER)), sizeof g_cmd_buf);
    if (invoke(g_cmd_buf,g_origdir.c_str()) == 42) {
        int ret;
        if ((g_datasrc->flags & DF_REMOTE) &&
            (nntp_date() <= 0 || (nntp_check() < 0 && atoi(g_ser_line) != NNTP_BAD_COMMAND_VAL)))
        {
            ret = 1;
        }
        else
        {
            ret = invoke(filexp(CALL_INEWS),g_origdir.c_str());
        }
        if (ret) {
            int   appended = 0;
            char* deadart = filexp("%./dead.article");
            FILE *fp_out = fopen(deadart, "a");
            if (fp_out != nullptr)
            {
                FILE *fp_in = fopen(g_headname.c_str(), "r");
                if (fp_in != nullptr)
                {
                    while (fgets(g_cmd_buf, sizeof g_cmd_buf, fp_in))
                    {
                        fputs(g_cmd_buf, fp_out);
                    }
                    fclose(fp_in);
                    appended = 1;
                }
                fclose(fp_out);
            }
            if (appended)
            {
                printf("Article appended to %s\n", deadart);
            }
            else
            {
                printf("Unable to append article to %s\n", deadart);
            }
        }
    }
}

void reply()
{
    char hbuf[5*LBUFLEN];
    bool incl_body = (*g_buf == 'R' && g_art);
    char* maildoer = savestr(get_val_const("MAILPOSTER",MAILPOSTER));

    artopen(g_art,(ART_POS)0);
    FILE *header = fopen(g_headname.c_str(),"w");       /* open header file */
    if (header == nullptr) {
        printf(g_cantcreate,g_headname.c_str());
        termdown(1);
        goto done;
    }
    interp(hbuf, sizeof hbuf, get_val("MAILHEADER",MAILHEADER));
    fputs(hbuf,header);
    if (!in_string(maildoer,"%h", true)) {
        if (g_verbose)
        {
            printf("\n%s\n(Above lines saved in file %s)\n", g_buf, g_headname.c_str());
        }
        else
        {
            printf("\n%s\n(Header in %s)\n", g_buf, g_headname.c_str());
        }
        termdown(3);
    }
    if (incl_body && g_artfp != nullptr) {
        char* s;
        interp(g_buf, (sizeof g_buf), get_val("YOUSAID",YOUSAID));
        fprintf(header,"%s\n",g_buf);
        parseheader(g_art);
        mime_SetArticle();
        clear_artbuf();
        seekart(g_htype[PAST_HEADER].minpos);
        g_wrapped_nl = '\n';
        while ((s = readartbuf(false)) != nullptr) {
            char *t = strchr(s, '\n');
            if (t != nullptr)
            {
                *t = '\0';
            }
            strcharsubst(hbuf,s,sizeof hbuf,*g_charsubst);
            fprintf(header,"%s%s\n",g_indstr.c_str(),hbuf);
            if (t)
            {
                *t = '\0';
            }
        }
        fprintf(header,"\n");
        g_wrapped_nl = WRAPPED_NL;
    }
    fclose(header);
    safecpy(g_cmd_buf,filexp(maildoer),sizeof g_cmd_buf);
    invoke(g_cmd_buf,g_origdir.c_str());
done:
    free(maildoer);
}

void forward()
{
    char hbuf[5*LBUFLEN];
    char* maildoer = savestr(get_val_const("FORWARDPOSTER",FORWARDPOSTER));
#ifdef REGEX_WORKS_RIGHT
    COMPEX mime_compex;
#else
    char* eol;
#endif
    char* mime_boundary;

#ifdef REGEX_WORKS_RIGHT
    init_compex(&mime_compex);
#endif
    artopen(g_art,(ART_POS)0);
    FILE *header = fopen(g_headname.c_str(),"w");       /* open header file */
    if (header == nullptr) {
        printf(g_cantcreate,g_headname.c_str());
        termdown(1);
        goto done;
    }
    interp(hbuf, sizeof hbuf, get_val("FORWARDHEADER",FORWARDHEADER));
    fputs(hbuf,header);
#ifdef REGEX_WORKS_RIGHT
    if (!compile(&mime_compex,"Content-Type: multipart/.*; *boundary=\"\\([^\"]*\\)\"",true,true)
     && execute(&mime_compex,hbuf) != nullptr)
    {
        mime_boundary = getbracket(&mime_compex,1);
    }
    else
    {
        mime_boundary = nullptr;
    }
#else
    mime_boundary = nullptr;
    for (char *s = hbuf; s; s = eol) {
        eol = strchr(s, '\n');
        if (eol)
        {
            eol++;
        }
        if (*s == 'C' && string_case_equal(s, "Content-Type: multipart/", 24)) {
            s += 24;
            for (;;) {
                for ( ; *s && *s != ';'; s++) {
                    if (*s == '\n' && !isspace(s[1]))
                    {
                        break;
                    }
                }
                if (*s != ';')
                {
                    break;
                }
                s = skip_eq(++s, ' ');
                if (*s == 'b' && string_case_equal(s, "boundary=\"", 10)) {
                    mime_boundary = s+10;
                    s = strchr(mime_boundary, '"');
                    if (s != nullptr)
                    {
                        *s = '\0';
                    }
                    mime_boundary = savestr(mime_boundary);
                    if (s)
                    {
                        *s = '"';
                    }
                    break;
                }
            }
        }
    }
#endif
    if (!in_string(maildoer,"%h", true)) {
        if (g_verbose)
        {
            printf("\n%s\n(Above lines saved in file %s)\n", hbuf, g_headname.c_str());
        }
        else
        {
            printf("\n%s\n(Header in %s)\n", hbuf, g_headname.c_str());
        }
        termdown(3);
    }
    if (g_artfp != nullptr) {
        interp(g_buf, sizeof g_buf, get_val("FORWARDMSG",FORWARDMSG));
        if (mime_boundary) {
            if (*g_buf && string_case_compare(g_buf, "Content-", 8))
            {
                strcpy(g_buf, "Content-Type: text/plain\n");
            }
            fprintf(header,"--%s\n%s\n[Replace this with your comments.]\n\n--%s\nContent-Type: message/rfc822\n\n",
                    mime_boundary,g_buf,mime_boundary);
        }
        else if (*g_buf)
        {
            fprintf(header, "%s\n", g_buf);
        }
        parseheader(g_art);
        seekart((ART_POS)0);
        while (readart(g_buf,sizeof g_buf) != nullptr) {
            if (!mime_boundary && *g_buf == '-') {
                putchar('-');
                putchar(' ');
            }
            fprintf(header,"%s",g_buf);
        }
        if (mime_boundary)
        {
            fprintf(header, "\n--%s--\n", mime_boundary);
        }
        else {
            interp(g_buf, (sizeof g_buf), get_val("FORWARDMSGEND",FORWARDMSGEND));
            if (*g_buf)
            {
                fprintf(header, "%s\n", g_buf);
            }
        }
    }
    fclose(header);
    safecpy(g_cmd_buf,filexp(maildoer),sizeof g_cmd_buf);
    invoke(g_cmd_buf,g_origdir.c_str());
  done:
    free(maildoer);
#ifdef REGEX_WORKS_RIGHT
    free_compex(&mime_compex);
#else
    safefree(mime_boundary);
#endif
}

void followup()
{
    char hbuf[5*LBUFLEN];
    bool incl_body = (*g_buf == 'F' && g_art);
    ART_NUM oldart = g_art;

    if (!incl_body && g_art <= g_lastart) {
        termdown(2);
        in_answer("\n\nAre you starting an unrelated topic? [ynq] ", MM_FOLLOWUP_NEW_TOPIC_PROMPT);
        setdef(g_buf,"y");
        if (*g_buf == 'q')  /*TODO: need to add 'h' also */
        {
            return;
        }
        if (*g_buf != 'n')
        {
            g_art = g_lastart + 1;
        }
    }
    artopen(g_art,(ART_POS)0);
    FILE *header = fopen(g_headname.c_str(),"w");
    if (header == nullptr) {
        printf(g_cantcreate,g_headname.c_str());
        termdown(1);
        g_art = oldart;
        return;
    }
    interp(hbuf, sizeof hbuf, get_val("NEWSHEADER",NEWSHEADER));
    fputs(hbuf,header);
    if (incl_body && g_artfp != nullptr) {
        char* s;
        if (g_verbose)
        {
            fputs("\n"
                  "(Be sure to double-check the attribution against the signature, and\n"
                  "trim the quoted article down as much as possible.)\n",
                  stdout);
        }
        interp(g_buf, (sizeof g_buf), get_val("ATTRIBUTION",ATTRIBUTION));
        fprintf(header,"%s\n",g_buf);
        parseheader(g_art);
        mime_SetArticle();
        clear_artbuf();
        seekart(g_htype[PAST_HEADER].minpos);
        g_wrapped_nl = '\n';
        while ((s = readartbuf(false)) != nullptr) {
            char *t = strchr(s, '\n');
            if (t != nullptr)
            {
                *t = '\0';
            }
            strcharsubst(hbuf,s,sizeof hbuf,*g_charsubst);
            fprintf(header,"%s%s\n",g_indstr.c_str(),hbuf);
            if (t)
            {
                *t = '\0';
            }
        }
        fprintf(header,"\n");
        g_wrapped_nl = WRAPPED_NL;
    }
    fclose(header);
    follow_it_up();
    g_art = oldart;
}

int invoke(const char *cmd, const char *dir)
{
    minor_mode oldmode = g_mode;
    int ret = -1;

    if (g_datasrc->flags & DF_REMOTE)
    {
        nntp_finishbody(FB_SILENT);
    }
#ifdef DEBUG
    if (debug)
    {
        printf("\nInvoking command: %s\n",cmd);
    }
#endif
    if (dir) {
        if (change_dir(dir)) {
            printf(g_nocd,dir);
            return ret;
        }
    }
    set_mode(g_general_mode,MM_EXECUTE);
    termlib_reset();
    resetty();                  /* make terminal well-behaved */
    ret = doshell(SH,cmd);      /* do the command */
    noecho();                   /* set no echo */
    crmode();                   /* and cbreak mode */
    termlib_init();
    set_mode(g_general_mode,oldmode);
    if (dir)
    {
        chdir_newsdir();
    }
    return ret;
}

/*
** cut_line() determines if a line is meant as a "cut here" marker.
** Some examples that we understand:
**
**  BEGIN--cut here--cut here
**
**  ------------------ tear at this line ------------------
**
**  #----cut here-----cut here-----cut here-----cut here----#
*/
#if 0
static bool cut_line(char *str)
{
    char* cp;
    char got_flag;
    char word[80];
    int  dash_cnt, equal_cnt, other_cnt;

    /* Disallow any single-/double-quoted, parenthetical or c-commented
    ** string lines.  Make sure it has the cut-phrase and at least six
    ** '-'s or '='s.  If only four '-'s are present, check for a duplicate
    ** of the cut phrase.  If over 20 unknown characters are encountered,
    ** assume it isn't a cut line.  If we succeed, return true.
    */
    for (cp = str, dash_cnt = equal_cnt = other_cnt = 0; *cp; cp++) {
        switch (*cp) {
        case '-':
            dash_cnt++;
            break;
        case '=':
            equal_cnt++;
            break;
        case '/':
            if (*(cp+1) != '*')
            {
                break;
            }
        case '"':
        case '\'':
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
            return false;
        default:
            other_cnt++;
            break;
        }
    }
    if (dash_cnt < 4 && equal_cnt < 6)
    {
        return false;
    }

    got_flag = 0;

    for (*(cp = word) = '\0'; *str; str++) {
        if (islower(*str))
        {
            *cp++ = *str;
        }
        else if (isupper(*str))
        {
            *cp++ = tolower(*str);
        }
        else {
            if (*word) {
                *cp = '\0';
                switch (got_flag) {
                case 2:
                    if (!strcmp(word, "line")
                     || !strcmp(word, "here"))
                        if ((other_cnt -= 4) <= 20)
                        {
                            return true;
                        }
                    break;
                case 1:
                    if (!strcmp(word, "this")) {
                        got_flag = 2;
                        other_cnt -= 4;
                    }
                    else if (!strcmp(word, "here")) {
                        other_cnt -= 4;
                        if ((dash_cnt >= 6 || equal_cnt >= 6)
                         && other_cnt <= 20)
                        {
                            return true;
                        }
                        dash_cnt = 6;
                        got_flag = 0;
                    }
                    break;
                case 0:
                    if (!strcmp(word, "cut")
                     || !strcmp(word, "snip")
                     || !strcmp(word, "tear")) {
                        got_flag = 1;
                        other_cnt -= strlen(word);
                    }
                    break;
                }
                *(cp = word) = '\0';
            }
        }
    } /* for *str */

    return false;
}
#endif
