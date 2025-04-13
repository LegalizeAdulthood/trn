/* inews.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <config/common.h>
#include <config/pipe_io.h>
#include <config/string_case_compare.h>
#include <nntp/nntpclient.h>
#include <nntp/nntpinit.h>
#include <tool/util3.h>
#include <trn/string-algos.h>
#include <util/env.h>
#include <util/util2.h>

#include <string>

enum
{
    MAX_SIGNATURE = 4
};

int debug{};
int new_connection{};
char *g_server_name{};
std::string g_nntp_auth_file;
char g_buf[LBUFLEN + 1]{}; /* general purpose line buffer */

int valid_header(char *h);
void append_signature();

static FILE *inews_wr_fp{};

static void inews_fputs(const char *buff)
{
    if (inews_wr_fp)
    {
        fputs(buff, inews_wr_fp);
    }
    else
    {
        boost::system::error_code ec;
        g_nntplink.connection->write(buff, 0, ec);
    }
}

static void inews_fputc(char c)
{
    if (inews_wr_fp)
    {
        fputc(c, inews_wr_fp);
    }
    else
    {
        boost::system::error_code ec;
        g_nntplink.connection->write(&c, 1, ec);
    }
}

static void inews_fflush()
{
    if (inews_wr_fp)
    {
        (void) fflush(inews_wr_fp);
    }
}

int main(int argc, char *argv[])
{
    bool has_fromline;
    bool in_header;
    bool has_pathline;
    bool found_nl;
    bool  had_nl;
    int   artpos;
    int   len;
    char *cp;
    int  i;

    int   headbuf_size = LBUFLEN * 8;
    char *headbuf = safemalloc(headbuf_size);

#ifdef LAX_INEWS
    env_init(headbuf, true);
#else
    if (!env_init(headbuf, false)) {
        fprintf(stderr,"Can't get %s information. Please contact your system adminstrator.\n",
                (!g_login_name.empty() || g_real_name.empty())? "user" : "host");
        exit(1);
    }
#endif

    argv++;
    while (argc > 1) {
        if (*argv[0] != '-')
        {
            break;
        }
        argv++;
        argc--;
    }
    if (argc > 1) {
        if (freopen(*argv, "r", stdin) == nullptr) {
            perror(*argv);
            exit(1);
        }
    }

    cp = get_val("NNTPSERVER");
    if (!cp) {
        cp = filexp(SERVER_NAME);
        if (FILE_REF(cp))
        {
            cp = nntp_servername(cp);
        }
    }
    const char *line_end;
    if (cp && *cp && strcmp(cp,"local") != 0)
    {
        g_server_name = savestr(cp);
        cp = strchr(g_server_name, ';');
        if (cp) {
            *cp = '\0';
            g_nntplink.port_number = atoi(cp+1);
        }
        line_end = "\r\n";
        g_nntp_auth_file = filexp(NNTP_AUTH_FILE);
        if ((cp = getenv("NNTP_FORCE_AUTH")) != nullptr
         && (*cp == 'y' || *cp == 'Y'))
        {
            g_nntplink.flags |= NNTP_FORCE_AUTH_NEEDED;
        }
    } else {
        g_server_name = nullptr;
        line_end = "\n";
    }

    in_header = false;
    has_fromline = false;
    has_pathline = false;
    artpos = 0;
    cp = headbuf;
    had_nl = true;

    for (;;) {
        if (headbuf_size < artpos + LBUFLEN + 1) {
            len = cp - headbuf;
            headbuf_size += LBUFLEN * 4;
            headbuf = saferealloc(headbuf,headbuf_size);
            cp = headbuf + len;
        }
        i = getc(stdin);
        if (g_server_name && had_nl && i == '.')
        {
            *cp++ = '.';
        }

        if (i == '\n') {
            if (!in_header)
            {
                continue;
            }
            break;
        }
        if (i == EOF || !fgets(cp+1, LBUFLEN-1, stdin)) {
            /* Still in header after EOF?  Hmm... */
            fprintf(stderr,"Article was all header -- no body.\n");
            exit(1);
        }
        *cp = (char)i;
        len = strlen(cp);
        found_nl = (len && cp[len-1] == '\n');
        if (had_nl) {
            i = valid_header(cp);
            if (i == 0)
            {
                fprintf(stderr,"Invalid header:\n%s",cp);
                exit(1);
            }
            if (i == 2) {
                if (!in_header)
                {
                    continue;
                }
                break;
            }
            in_header = true;
            if (string_case_equal(cp, "From:", 5))
            {
                has_fromline = true;
            }
            else if (string_case_equal(cp, "Path:", 5))
            {
                has_pathline = true;
            }
        }
        artpos += len;
        cp += len;
        had_nl = found_nl;
        if (had_nl != 0 && g_server_name)
        {
            cp[-1] = '\r';
            *cp++ = '\n';
        }
    }
    *cp = '\0';

    /* Well, the header looks ok, so let's get on with it. */

    if (g_server_name) {
        if (!g_nntplink.connection) {
            if (init_nntp() < 0 || !nntp_connect(g_server_name,false))
            {
                exit(1);
            }
            new_connection = true;
        }
        if (nntp_command("POST") <= 0 || nntp_check() <= 0) {
            if (new_connection)
            {
                nntp_close(true);
            }
            fprintf(stderr,"Sorry, you can't post from this machine.\n");
            exit(1);
        }
    }
    else {
        sprintf(g_buf, "%s -h", EXTRAINEWS);
        inews_wr_fp = popen(g_buf,"w");
        if (!inews_wr_fp) {
            fprintf(stderr,"Unable to execute inews for local posting.\n");
            exit(1);
        }
    }

    inews_fputs(headbuf);
    if (!has_pathline)
    {
        sprintf(g_buf,"Path: not-for-mail%s",line_end);
        inews_fputs(g_buf);
    }
    if (!has_fromline) {
        char buff[80];
        strcpy(buff, g_real_name.c_str());
        sprintf(g_buf, "From: %s@%s (%s)%s", g_login_name.c_str(), g_p_host_name.c_str(), get_val_const("NAME", buff),
                line_end);
        inews_fputs(g_buf);
    }
    if (!getenv("NO_ORIGINATOR")) {
        char buff[80];
        strcpy(buff, g_real_name.c_str());
        sprintf(g_buf, "Originator: %s@%s (%s)%s", g_login_name.c_str(), g_p_host_name.c_str(), get_val_const("NAME", buff),
                line_end);
        inews_fputs(g_buf);
    }
    sprintf(g_buf, "%s", line_end);
    inews_fputs(g_buf);

    had_nl = true;
    while (fgets(headbuf, headbuf_size, stdin)) {
        /* Single . is eof, so put in extra one */
        if (g_server_name && had_nl && *headbuf == '.')
        {
            inews_fputc('.');
        }
        /* check on newline */
        cp = headbuf + strlen(headbuf);
        if (cp > headbuf && *--cp == '\n') {
            *cp = '\0';
            sprintf(g_buf, "%s%s", headbuf, line_end);
            inews_fputs(g_buf);
            had_nl = true;
        }
        else {
            inews_fputs(headbuf);
            had_nl = false;
        }
    }

    if (!inews_wr_fp)
    {
        return pclose(inews_wr_fp);
    }

    if (!had_nl)
    {
        inews_fputs(line_end);
    }

    append_signature();

    inews_fputs(".\r\n");
    inews_fflush();

    if (nntp_gets(g_ser_line, sizeof g_ser_line) < 0
     || *g_ser_line != NNTP_CLASS_OK) {
        if (atoi(g_ser_line) == NNTP_POSTFAIL_VAL) {
            fprintf(stderr,"Article not accepted by server; not posted:\n");
            for (cp = g_ser_line + 4; *cp && *cp != '\r'; cp++) {
                if (*cp == '\\')
                {
                    fputc('\n',stderr);
                }
                else
                {
                    fputc(*cp,stderr);
                }
            }
            fputc('\n', stderr);
        }
        else
        {
            fprintf(stderr, "Remote error: %s\n", g_ser_line);
        }
        if (new_connection)
        {
            nntp_close(true);
        }
        exit(1);
    }

    if (new_connection)
    {
        nntp_close(true);
    }
    cleanup_nntp();

    return 0;
}

/* valid_header -- determine if a line is a valid header line */
int valid_header(char *h)
{
    /* Blank or tab in first position implies this is a continuation header */
    if (is_hor_space(h[0])) {
        h = skip_hor_space(h);
        return *h && *h != '\n'? 1 : 2;
    }

    /* Just check for initial letter, colon, and space to make
     * sure we discard only invalid headers. */
    char *colon = strchr(h, ':');
    char *space = strchr(h, ' ');
    if (isalpha(h[0]) && colon && space == colon + 1)
    {
        return 1;
    }

    /* Anything else is a bad header */
    return 0;
}

/* append_signature -- append the person's .signature file if
 * they have one.  Limit .signature to MAX_SIGNATURE lines.
 * The rn-style DOTDIR environmental variable is used if present.
 */
void append_signature()
{
    char* cp;
    FILE* fp;
    int count = 0;

#ifdef NO_INEWS_DOTDIR
    g_dot_dir = g_home_dir;
#endif
    if (g_dot_dir.empty())
    {
        return;
    }

    fp = fopen(filexp(SIGNATURE_FILE), "r");
    if (fp == nullptr)
    {
        return;
    }

    sprintf(g_buf, "-- \r\n");
    inews_fputs(g_buf);
    while (fgets(g_ser_line, sizeof g_ser_line, fp)) {
        count++;
        if (count > MAX_SIGNATURE) {
            fprintf(stderr,"Warning: .signature files should be no longer than %d lines.\n",
                    MAX_SIGNATURE);
            fprintf(stderr,"(Only %d lines of your .signature were posted.)\n",
                    MAX_SIGNATURE);
            break;
        }
        /* Strip trailing newline */
        cp = g_ser_line + strlen(g_ser_line) - 1;
        if (cp >= g_ser_line && *cp == '\n')
        {
            *cp = '\0';
        }
        sprintf(g_buf, "%s\r\n", g_ser_line);
        inews_fputs(g_buf);
    }
    (void) fclose(fp);
}

int nntp_handle_timeout()
{
    if (!new_connection) {
        static bool handling_timeout = false;
        char last_command_save[NNTP_STRLEN];

        if (string_case_equal(g_last_command, "quit"))
        {
            return 0;
        }
        if (handling_timeout)
        {
            return -1;
        }
        handling_timeout = true;
        strcpy(last_command_save, g_last_command);
        nntp_close(false);
        if (init_nntp() < 0 || nntp_connect(g_server_name,false) <= 0)
        {
            exit(1);
        }
        if (nntp_command(last_command_save) <= 0)
        {
            return -1;
        }
        handling_timeout = false;
        new_connection = true;
        return 1;
    }
    fputs("\n503 Server timed out.\n",stderr);
    return -2;
}
