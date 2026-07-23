/* nntpclient.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_NNTPCLIENT_H
#define TRN_NNTPCLIENT_H

#include <trn/enum-flags.h>
#include <trn/util.h>

#include <boost/system/error_code.hpp>

#include <functional>
#include <memory>
#include <stdlib.h> // size_t
#include <string>
#include <string_view>

enum NNTPFlags
{
    NNTP_NONE = 0x0000,
    NNTP_NEW_CMD_OK = 0x0001,
    NNTP_FORCE_AUTH_NEEDED = 0x0002,
    NNTP_FORCE_AUTH_NOW = 0x0004
};
DECLARE_FLAGS_ENUM(NNTPFlags, int);

using error_code_t = boost::system::error_code;

struct INNTPConnection
{
    virtual ~INNTPConnection() = default;

    virtual std::string read_line(error_code_t &ec) = 0;
    virtual void        write_line(const std::string &line, error_code_t &ec) = 0;
    virtual void        write(std::string_view buffer, error_code_t &ec) = 0;
    virtual size_t      read(char *buf, size_t size, error_code_t &ec) = 0;
};

// use a shared_ptr to allow copying of NNTPLink structure like a value.
using ConnectionPtr = std::shared_ptr<INNTPConnection>;

using ConnectionFactory = std::function<ConnectionPtr(const char *machine, int pot, const char *service)>;

struct NNTPLink
{
    ConnectionPtr connection;
    std::time_t   last_command;
    int           port_number;
    NNTPFlags     flags;
    bool          trailing_cr;
};

// RFC 977 defines these, so don't change them
enum
{
    NNTP_CLASS_INF = '1',
    NNTP_CLASS_OK = '2',
    NNTP_CLASS_CONT = '3',
    NNTP_CLASS_ERR = '4',
    NNTP_CLASS_FATAL = '5'
};

enum
{
    NNTP_POSTOK_VAL = 200,       // Hello -- you can post
    NNTP_NOPOSTOK_VAL = 201,     // Hello -- you can't post
    NNTP_LIST_FOLLOWS_VAL = 215, // There's a list a-comin' next
    NNTP_GOODBYE_VAL = 400,      // Have to hang up for some reason
    NNTP_NOSUCHGROUP_VAL = 411,  // No such newsgroup
    NNTP_NONEXT_VAL = 421,       // No next article
    NNTP_NOPREV_VAL = 422,       // No previous article
    NNTP_POSTFAIL_VAL = 441,     // Posting failed
    NNTP_AUTH_NEEDED_VAL = 480,  // Authorization Failed
    NNTP_AUTH_REJECT_VAL = 482,  // Authorization data rejected
    NNTP_BAD_COMMAND_VAL = 500,  // Command not recognized
    NNTP_SYNTAX_VAL = 501,       // Command syntax error
    NNTP_ACCESS_VAL = 502,       // Access to server denied
    NNTP_TMPERR_VAL = 503,       // Program fault, command not performed
    NNTP_AUTH_BAD_VAL = 580      // Authorization Failed
};

enum
{
    NNTP_STRLEN = 512
};

extern NNTPLink g_nntp_link; // the current server's file handles
extern bool     g_nntp_allow_timeout;
extern char     g_ser_line[NNTP_STRLEN];
extern std::string g_last_command;

inline std::string nntp_get_a_line()
{
    boost::system::error_code ec;

    return g_nntp_link.connection->read_line(ec);
}

void  set_nntp_connection_factory(ConnectionFactory factory);
int   nntp_connect(const char *machine, bool verbose);
std::string nntp_server_name(std::string_view name);
int   nntp_command(std::string_view bp);
int   nntp_xgtitle(std::string_view groupname);
int   nntp_check();
bool        nntp_at_list_end(std::string_view s);
enum NNTPGetsResult
{
    // NGSR: nntp get string result
    NGSR_ERROR = -2,
    NGSR_PARTIAL_LINE = 0,
    NGSR_FULL_LINE = 1,
};
NNTPGetsResult nntp_gets(std::string &line, int len);
NNTPGetsResult nntp_gets(char *bp, int len);
void           nntp_gets_clear_buffer();
void           nntp_close(bool send_quit);

#endif
