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
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

#ifdef MSDOS
static constexpr std::string_view s_bad_chars{"<>:\"/\\|?* \t"};
#else
static constexpr std::string_view s_bad_chars{"!$&*()|'\";<>[]{}?/`\\ \t"};
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
        if (std::isprint(static_cast<unsigned char>(ch)) && s_bad_chars.find(ch) == std::string_view::npos)
        {
            filename.push_back(ch);
        }
    }
#ifdef MSDOS
    while (!filename.empty() && (filename.back() == ' ' || filename.back() == '.'))
    {
        filename.pop_back();
    }
#endif
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
    if (filename == "." || filename == "..")
    {
        return true;
    }

    const std::size_t      dot = filename.find('.');
    const std::string_view device_name = dot == std::string_view::npos ? filename : filename.substr(0, dot);
    if (device_name.size() == 3)
    {
        if (string_case_equal(device_name, "aux") || string_case_equal(device_name, "con") //
            || string_case_equal(device_name, "nul") || string_case_equal(device_name, "prn"))
        {
            return true;
        }
    }
    else if (device_name.size() == 4)
    {
        if (string_case_equal(device_name, "com1") || string_case_equal(device_name, "com2")    //
            || string_case_equal(device_name, "com3") || string_case_equal(device_name, "com4") //
            || string_case_equal(device_name, "lpt1") || string_case_equal(device_name, "lpt2") //
            || string_case_equal(device_name, "lpt3"))
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
    int  part = -1;
    int  total = 0;
    bool hasdot = false;

    *partp = part;
    *totalp = total;
    std::string subject_storage = fetch_subj_copy(art_num);
    if (subject_storage.empty())
    {
        return {};
    }

    std::string_view  subject{subject_storage};
    const std::size_t nul = subject.find('\0');
    if (nul != std::string_view::npos)
    {
        subject = subject.substr(0, nul);
    }
    const std::size_t newline = subject.find('\n');
    if (newline != std::string_view::npos)
    {
        subject = subject.substr(0, newline);
    }

    const auto char_at = [&subject](std::size_t pos) { return pos < subject.size() ? subject[pos] : '\0'; };
    const auto is_digit = [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)) != 0; };
    const auto is_alnum = [](char ch) { return std::isalnum(static_cast<unsigned char>(ch)) != 0; };
    const auto is_filename_char = [&is_alnum](char ch)
    { return is_alnum(ch) || ch == '-' || ch == '+' || ch == '&' || ch == '_' || ch == '.'; };
    const auto skip_space_pos = [&subject](std::size_t pos)
    {
        while (pos < subject.size() && std::isspace(static_cast<unsigned char>(subject[pos])))
        {
            ++pos;
        }
        return pos;
    };
    const auto skip_digits_pos = [&subject, &is_digit](std::size_t pos)
    {
        while (pos < subject.size() && is_digit(subject[pos]))
        {
            ++pos;
        }
        return pos;
    };
    const auto starts_case = [&subject](std::size_t pos, std::string_view text)
    { return pos + text.size() <= subject.size() && string_case_equal(subject.substr(pos, text.size()), text); };
    const auto parse_number = [&subject, &is_digit](std::size_t pos)
    {
        int value = 0;
        while (pos < subject.size() && is_digit(subject[pos]))
        {
            value = value * 10 + subject[pos] - '0';
            ++pos;
        }
        return value;
    };

    // Skip leading whitespace and other garbage
    std::size_t pos = 0;
    while (is_hor_space(char_at(pos)) || char_at(pos) == '-')
    {
        ++pos;
    }
    if (starts_case(pos, "repost"))
    {
        pos += 6;
        while (is_hor_space(char_at(pos)) || char_at(pos) == ':' || char_at(pos) == '-')
        {
            ++pos;
        }
    }

    while (starts_case(pos, "re:"))
    {
        pos = skip_space_pos(pos + 3);
    }

    // Get filename

    // Grab the first filename-like string.  Explicitly ignore strings with
    // prefix "v<digit>" ending in ":", since that is a popular volume/issue
    // representation syntax
    //
    std::size_t filename_begin = pos;
    std::size_t filename_end = pos;
    do
    {
        while (pos < subject.size() && !is_alnum(subject[pos]) && subject[pos] != '_')
        {
            ++pos;
        }
        filename_begin = pos;
        filename_end = pos;
        while (is_filename_char(char_at(pos)))
        {
            if (subject[pos++] == '.')
            {
                hasdot = true;
            }
        }
        filename_end = pos;
        if (pos >= subject.size())
        {
            return {};
        }
    } while (filename_begin == filename_end ||
             (char_at(filename_begin) == 'v' && is_digit(char_at(filename_begin + 1)) && char_at(pos) == ':'));
    ++pos;

    // Try looking for a filename with a "." in it later in the subject line.
    // Exclude <digit>.<digit>, since that is usually a version number.
    //
    if (!hasdot)
    {
        std::size_t scan = pos;
        while (scan < subject.size())
        {
            std::size_t token_begin = skip_space_pos(scan);
            std::size_t token_end = token_begin;
            bool        token_hasdot = false;
            while (is_filename_char(char_at(token_end)))
            {
                const bool prior_is_digit = token_end != 0 && is_digit(char_at(token_end - 1));
                if (subject[token_end] == '.' && (!prior_is_digit || !is_digit(char_at(token_end + 1))))
                {
                    token_hasdot = true;
                }
                ++token_end;
            }
            if (token_hasdot && token_end > token_begin)
            {
                filename_begin = token_begin;
                filename_end = token_end;
                break;
            }
            scan = token_end;
            while (scan < subject.size() && !is_alnum(subject[scan]))
            {
                ++scan;
            }
        }
        pos = filename_end + 1;
    }

    if (pos >= subject.size())
    {
        return {};
    }

    // Get part number
    while (pos < subject.size())
    {
        // skip over versioning
        if (char_at(pos) == 'v' && is_digit(char_at(pos + 1)))
        {
            ++pos;
            pos = skip_digits_pos(pos);
        }
        // look for "1/6" or "1 / 6" or "1 of 6" or "1-of-6" or "1o6"
        if (is_digit(char_at(pos))                                       //
            && (char_at(pos + 1) == '/'                                  //
                || (char_at(pos + 1) == ' ' && char_at(pos + 2) == '/')  //
                || (char_at(pos + 1) == ' ' && char_at(pos + 2) == 'o' && char_at(pos + 3) == 'f') //
                || (char_at(pos + 1) == '-' && char_at(pos + 2) == 'o' && char_at(pos + 3) == 'f') //
                || (char_at(pos + 1) == 'o' && is_digit(char_at(pos + 2)))))
        {
            std::size_t part_begin = pos;
            while (part_begin != 0 && is_digit(char_at(part_begin - 1)))
            {
                --part_begin;
            }
            part = parse_number(part_begin);
            ++pos;
            while (pos < subject.size() && !is_digit(char_at(pos)))
            {
                ++pos;
            }
            total = is_digit(char_at(pos)) ? parse_number(pos) : 0;
            pos = skip_digits_pos(pos);
            // We don't break here because we want the last item on the line
        }

        // look for "6 parts" or "part 1"
        if (starts_case(pos, "part"))
        {
            if (char_at(pos + 4) == 's')
            {
                std::size_t digit_pos = pos;
                while (digit_pos != 0 && !is_digit(char_at(digit_pos)))
                {
                    --digit_pos;
                }
                if (digit_pos != 0 && is_digit(char_at(digit_pos)))
                {
                    std::size_t total_begin = digit_pos;
                    while (total_begin != 0 && is_digit(char_at(total_begin - 1)))
                    {
                        --total_begin;
                    }
                    total = parse_number(total_begin);
                }
            }
            else
            {
                std::size_t part_pos = pos;
                while (part_pos < subject.size() && !is_digit(char_at(part_pos)))
                {
                    ++part_pos;
                }
                if (is_digit(char_at(part_pos)))
                {
                    part = parse_number(part_pos);
                    pos = part_pos == 0 ? 0 : part_pos - 1;
                }
                else
                {
                    pos = subject.size();
                }
            }
        }
        if (pos < subject.size())
        {
            ++pos;
        }
    }

    if (total == 0 || part == -1 || part > total)
    {
        return {};
    }
    *partp = part;
    *totalp = total;
    return std::string{subject.substr(filename_begin, filename_end - filename_begin)};
}

//
// Handle a piece of a split file.
//
bool decode_piece(MimeCapEntry *mcp, char *first_line)
{
    g_msg.clear();
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
            g_msg = "Failed.";
            return false;
        }
    }
    if (mcp)
    {
        if (change_dir(dir))
        {
            fmt::print("Can't chdir to directory {}\n", dir.string());
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
            g_msg = "Failed.";
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
        mime_parse_sub_header(fp, first_line != nullptr ? first_line : "");
        first_line = nullptr;
    }
    g_mime_getc_line = first_line;
    DecodeFunc decoder = decode_function(g_mime_section->m_encoding);
    if (!decoder)
    {
        g_msg = "Unhandled encoding type -- aborting.";
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
            g_msg = "Failed.";
            return false;
        }
    }

    if (state != DECODE_DONE)
    {
        (void) decoder(nullptr, DECODE_DONE);
        if (state != DECODE_MAYBE_DONE)
        {
            g_msg = "Premature EOF.";
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
        mime_exec(mcp->command);
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
