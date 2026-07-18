/* decode.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/decode.h"

#include "trn/art.h"
#include "trn/artio.h"
#include "trn/artstate.h"
#include "trn/change_dir.h"
#include "trn/final.h"
#include "trn/head.h"
#include "trn/intrp.h"
#include "trn/mime.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/util.h"
#include "util/util2.h"
#include "trn/uudecode.h"

#include <fmt/format.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

#ifdef MSDOS
#define GOODCHARS                \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
    "abcdefghijklmnopqrstuvwxyz" \
    "0123456789-_^#%"
#else
#define BADCHARS "!$&*()|\'\";<>[]{}?/`\\ \t"
#endif

std::string g_decode_filename;

static bool bad_filename(std::string_view filename);
static DecodeFunc decode_function(MimeEncoding encoding);
static fs::path decode_mkdir(std::string_view filename);
static void     decode_rmdir(const fs::path &dir);

void decode_init()
{
}

std::string decode_fix_filename(std::string_view text)
{
    std::string path{text};
    std::replace(path.begin(), path.end(), '\\', '/');

    std::string filename;
    filename.reserve(path.size());
    for (const char ch : fs::path{path}.filename().string())
    {
        if (std::isprint(static_cast<unsigned char>(ch))
#ifdef GOODCHARS
            && std::strchr(GOODCHARS, ch) != nullptr
#else
            && std::strchr(BADCHARS, ch) == nullptr
#endif
        )
        {
            filename.push_back(ch);
        }
    }
    if (filename.empty() || bad_filename(filename))
    {
        filename = "x";
    }
    return filename;
}

// Returns true if "filename" is a bad choice
static bool bad_filename(std::string_view filename)
{
#ifdef MSDOS
    if (filename.size() == 3)
    {
        if (string_case_equal(filename.data(), "aux", 3) || string_case_equal(filename.data(), "con", 3) //
            || string_case_equal(filename.data(), "nul", 3) || string_case_equal(filename.data(), "prn", 3))
        {
            return true;
        }
    }
    else if (filename.size() == 4)
    {
        if (string_case_equal(filename.data(), "com1", 4) || string_case_equal(filename.data(), "com2", 4)    //
            || string_case_equal(filename.data(), "com3", 4) || string_case_equal(filename.data(), "com4", 4) //
            || string_case_equal(filename.data(), "lpt1", 4) || string_case_equal(filename.data(), "lpt2", 4) //
            || string_case_equal(filename.data(), "lpt3", 4))
        {
            return true;
        }
    }
#else
    if (filename.size() <= 2 && !filename.empty())
    {
        if (filename.front() == '.')
        {
            return true;
        }
    }
#endif
    return false;
}

// Parse the subject looking for filename and part number information.
std::string decode_subject(ArticleNum art_num, int *partp, int *totalp)
{
    char *filename;
    char *t;
    int   part = -1;
    int   total = 0;
    int   hasdot = 0;

    *partp = part;
    *totalp = total;
    std::string subject = fetch_subj_copy(art_num);
    if (subject.empty())
    {
        return {};
    }

    // Skip leading whitespace and other garbage
    char *subject_text = subject.data();
    char *s = subject_text;
    while (is_hor_space(*s) || *s == '-')
    {
        s++;
    }
    if (string_case_equal(s, "repost", 6))
    {
        for (s += 6; is_hor_space(*s) || *s == ':' || *s == '-'; s++)
        {
        }
    }

    while (string_case_equal(s, "re:", 3))
    {
        s = skip_space(s + 3);
    }

    // Get filename

    // Grab the first filename-like string.  Explicitly ignore strings with
    // prefix "v<digit>" ending in ":", since that is a popular volume/issue
    // representation syntax
    //
    char *end = s + std::strlen(s);
    do
    {
        while (*s && !std::isalnum(*s) && *s != '_')
        {
            s++;
        }
        filename = s;
        t = s;
        while (std::isalnum(*s) || *s == '-' || *s == '+' || *s == '&' //
               || *s == '_' || *s == '.')
        {
            if (*s++ == '.')
            {
                hasdot = 1;
            }
        }
        if (!*s || *s == '\n')
        {
            return {};
        }
    } while (t == s || (t[0] == 'v' && std::isdigit(t[1]) && *s == ':'));
    *s++ = '\0';

    // Try looking for a filename with a "." in it later in the subject line.
    // Exclude <digit>.<digit>, since that is usually a version number.
    //
    if (!hasdot)
    {
        while (*(t = s) != '\0' && *s != '\n')
        {
            t = skip_space(t);
            for (s = t; std::isalnum(*s) || *s == '-' || *s == '+' || *s == '&' || *s == '_' || *s == '.'; s++)
            {
                if (*s == '.' && (!std::isdigit(s[-1]) || !std::isdigit(s[1])))
                {
                    hasdot = 1;
                }
            }
            if (hasdot && s > t)
            {
                filename = t;
                *s++ = '\0';
                break;
            }
            while (*s && *s != '\n' && !std::isalnum(*s))
            {
                s++;
            }
        }
        s = filename + std::strlen(filename) + 1;
    }

    if (s >= end)
    {
        return {};
    }

    // Get part number
    while (*s && *s != '\n')
    {
        // skip over versioning
        if (*s == 'v' && std::isdigit(s[1]))
        {
            s++;
            s = skip_digits(s);
        }
        // look for "1/6" or "1 / 6" or "1 of 6" or "1-of-6" or "1o6"
        if (std::isdigit(*s)                                   //
            && (s[1] == '/'                                    //
                || (s[1] == ' ' && s[2] == '/')                //
                || (s[1] == ' ' && s[2] == 'o' && s[3] == 'f') //
                || (s[1] == '-' && s[2] == 'o' && s[3] == 'f') //
                || (s[1] == 'o' && std::isdigit(s[2]))))
        {
            for (t = s; std::isdigit(t[-1]); t--)
            {
            }
            part = std::atoi(t);
            while (*++s != '\0' && *s != '\n' && !std::isdigit(*s))
            {
            }
            total = std::isdigit(*s) ? std::atoi(s) : 0;
            s = skip_digits(s);
            // We don't break here because we want the last item on the line
        }

        // look for "6 parts" or "part 1"
        if (string_case_equal("part", s, 4))
        {
            if (s[4] == 's')
            {
                for (t = s; t >= subject_text && !std::isdigit(*t); t--)
                {
                }
                if (t > subject_text)
                {
                    while (t > subject_text && std::isdigit(t[-1]))
                    {
                        t--;
                    }
                    total = std::atoi(t);
                }
            }
            else
            {
                while (*s && *s != '\n' && !std::isdigit(*s))
                {
                    s++;
                }
                if (std::isdigit(*s))
                {
                    part = std::atoi(s);
                }
                s--;
            }
        }
        if (*s)
        {
            s++;
        }
    }

    if (total == 0 || part == -1 || part > total)
    {
        return {};
    }
    *partp = part;
    *totalp = total;
    return filename;
}

//
// Handle a piece of a split file.
//
bool decode_piece(MimeCapEntry *mcp, char *first_line)
{
    *g_msg = '\0';
    const auto open_path = [](const fs::path &path, const char *mode)
    { return std::fopen(path.string().c_str(), mode); };
    const auto remove_path = [](const fs::path &path)
    {
        std::error_code error;
        fs::remove(path, error);
    };

    int part = g_mime_section->m_part;
    int total = g_mime_section->m_total;
    if (!total && g_is_mime)
    {
        total = 1;
        part = 1;
    }

    fs::path dir;
    g_decode_filename = decode_fix_filename(g_mime_section->m_filename ? *g_mime_section->m_filename : "unknown");
    const std::string filename = g_decode_filename;
    if (mcp || total != 1 || part != 1)
    {
        // Create directory to store parts and copy this part there.
        dir = decode_mkdir(filename);
        if (dir.empty())
        {
            std::strcpy(g_msg, "Failed.");
            return false;
        }
    }
    if (mcp)
    {
        if (change_dir(dir))
        {
            std::printf(g_no_cd, dir.string().c_str());
            sig_catcher(0);
        }
    }

    std::FILE* fp;
    if (total != 1 || part != 1)
    {
        if (total)
        {
            fmt::print("Saving part {} of {} {}", part, total, filename);
        }
        else
        {
            fmt::print("Saving part {} {}", part, filename);
        }
        if (g_no_wait_fork)
        {
            std::fflush(stdout);
        }
        else
        {
            newline();
        }

        fp = open_path(dir / std::to_string(part), "w");
        if (!fp)
        {
            std::strcpy(g_msg,"Failed.");
            return false;
        }
        while (read_art(g_art_line, sizeof g_art_line))
        {
            if (mime_end_of_section(g_art_line))
            {
                break;
            }
            std::fputs(g_art_line,fp);
            if (total == 0 && *g_art_line == 'e' && g_art_line[1] == 'n' //
                && g_art_line[2] == 'd' && std::isspace(g_art_line[3]))
            {
                // This is the last part. Remember the fact
                total = part;
                if (std::FILE *total_fp = open_path(dir / "CT", "w"))
                {
                    std::fprintf(total_fp, "%d\n", total);
                    std::fclose(total_fp);
                }
            }
        }
        std::fclose(fp);

        // Retrieve any previously saved number of the last part
        if (total == 0)
        {
            fp = open_path(dir / "CT", "r");
            if (fp != nullptr)
            {
                const std::string total_line = get_a_line(fp);
                if (!total_line.empty())
                {
                    total = std::atoi(total_line.c_str());
                    total = std::max(total, 0);
                }
                std::fclose(fp);
            }
            if (total == 0)
            {
                return true;
            }
        }

        // Check to see if we have all parts.  Start from the highest numbers
        // as we are more likely not to have them.
        //
        for (part = total; part; part--)
        {
            fp = open_path(dir / std::to_string(part), "r");
            if (!fp)
            {
                return true;
            }
            if (part != 1)
            {
                std::fclose(fp);
            }
        }
    }
    else
    {
        fp = nullptr;
        total = 1;
    }

    if (g_mime_section->m_type == MESSAGE_MIME)
    {
        mime_push_section();
        mime_parse_sub_header(fp, first_line);
        first_line = nullptr;
    }
    g_mime_getc_line = first_line;
    DecodeFunc decoder = decode_function(g_mime_section->m_encoding);
    if (!decoder)
    {
        std::strcpy(g_msg,"Unhandled encoding type -- aborting.");
        if (fp)
        {
            std::fclose(fp);
        }
        if (!dir.empty())
        {
            decode_rmdir(dir);
        }
        return false;
    }

    // Handle each part in order
    DecodeState state;
    for (state = DECODE_START, part = 1; part <= total; part++)
    {
        if (part != 1)
        {
            fp = open_path(dir / std::to_string(part), "r");
            if (!fp)
            {
                return true;
            }
        }

        state = decoder(fp, state);
        if (fp)
        {
            std::fclose(fp);
        }
        if (state == DECODE_ERROR)
        {
            std::strcpy(g_msg,"Failed.");
            return false;
        }
    }

    if (state != DECODE_DONE)
    {
        (void) decoder(nullptr, DECODE_DONE);
        if (state != DECODE_MAYBE_DONE)
        {
            std::strcpy(g_msg,"Premature EOF.");
            return false;
        }
    }

    if (fp)
    {
        // Cleanup all the pieces
        for (part = 0; part <= total; part++)
        {
            remove_path(dir / std::to_string(part));
        }
        remove_path(dir / "CT");
    }

    if (mcp)
    {
        mime_exec(mcp->command.c_str());
        remove_path(g_decode_filename);
        change_dir("..");
    }

    if (!dir.empty())
    {
        decode_rmdir(dir);
    }

    return true;
}

static DecodeFunc decode_function(MimeEncoding encoding)
{
    switch (encoding)
    {
    case MENCODE_QPRINT:
        return qp_decode;

    case MENCODE_BASE64:
        return b64_decode;

    case MENCODE_UUE:
        return uudecode;

    case MENCODE_NONE:
        return cat_decode;

    default:
        return nullptr;
    }
}

// return a directory to use for unpacking the pieces of a given filename
static fs::path decode_mkdir(std::string_view filename)
{
    fs::path dir;

#ifdef MSDOS
    dir = file_exp("%Y/parts/");
#else
    dir = file_exp("%Y/m-prts-%L/");
#endif
    if (filename.empty())
    {
        return {};
    }
    dir /= std::string{filename};
    if (dir.empty())
    {
        return {};
    }
    std::error_code error;
    fs::create_directories(dir, error);
    if (error)
    {
        return {};
    }
    return dir;
}

static void decode_rmdir(const fs::path &dir)
{
    // TODO: conditional-ize this
    std::error_code error;
    fs::remove(dir, error);
}
