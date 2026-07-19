/* uudecode.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/uudecode.h>

#include <config/common.h>
#include <trn/artio.h>
#include <trn/mime.h>
#include <trn/string-algos.h>
#include <trn/terminal.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

static void uudecode_line(char *line, std::FILE *ofp);

int uue_prescan(std::string_view text, std::string &filename, int *partp, int *totalp)
{
    const auto is_digit = [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)) != 0; };
    const auto starts_with = [](std::string_view value, std::string_view prefix)
    { return value.substr(0, prefix.size()) == prefix; };
    const auto starts_with_ignore_case = [](std::string_view value, std::string_view prefix)
    {
        return value.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), value.begin(),
                                                           [](char left, char right)
                                                           {
                                                               return std::tolower(static_cast<unsigned char>(left)) ==
                                                                      std::tolower(static_cast<unsigned char>(right));
                                                           });
    };
    const auto parse_number = [is_digit](std::string_view value, std::size_t offset, int &number)
    {
        if (offset >= value.size() || !is_digit(value[offset]))
        {
            return std::string_view::npos;
        }
        const std::from_chars_result result{
            std::from_chars(value.data() + offset, value.data() + value.size(), number)};
        return result.ec == std::errc{} ? static_cast<std::size_t>(result.ptr - value.data()) : std::string_view::npos;
    };
    const auto store_filename = [&filename](std::string_view value) { filename.assign(value); };

    if (starts_with(text, "begin ") && text.size() > 9 && is_digit(text[6]) && is_digit(text[7]) && is_digit(text[8]) &&
        (text[9] == ' ' || (text.size() > 10 && text[6] == '0' && is_digit(text[9]) && text[10] == ' ')))
    {
        if (*partp == -1)
        {
            filename.clear();
            *partp = 1;
            *totalp = 0;
        }
        return 1;
    }
    if (starts_with(text, "section ") && text.size() > 8 && is_digit(text[8]))
    {
        int         tmppart{};
        std::size_t position = parse_number(text, 8, tmppart);
        if (tmppart == 0 || position == std::string_view::npos)
        {
            return 0;
        }
        if (starts_with(text.substr(position), " of "))
        {
            // "section N of ... of file F ..."
            const std::size_t marker = text.find(" of file ", position + 4);
            if (marker == std::string_view::npos)
            {
                return 0;
            }
            const std::size_t filename_start = marker + 9;
            const std::size_t filename_end = text.find(' ', filename_start);
            if (filename_end == std::string_view::npos)
            {
                return 0;
            }
            store_filename(text.substr(filename_start, filename_end - filename_start));
            *partp = tmppart;
            *totalp = 0;
            return 1;
        }
        if (position + 1 < text.size() && text[position] == '/' && is_digit(text[position + 1]))
        {
            int               tmptotal{};
            const std::size_t total_end = parse_number(text, position, tmptotal);
            if (total_end == std::string_view::npos)
            {
                return 0;
            }
            position = text.find_first_not_of(" \f\n\r\t\v", total_end);
            if (tmppart > tmptotal || position == std::string_view::npos ||
                !starts_with(text.substr(position), "file "))
            {
                return 0;
            }
            const std::size_t filename_start = position + 5;
            const std::size_t filename_end = text.find(' ', filename_start);
            if (filename_end == std::string_view::npos)
            {
                return 0;
            }
            store_filename(text.substr(filename_start, filename_end - filename_start));
            *partp = tmppart;
            *totalp = tmptotal;
            return 1;
        }
    }
    if (starts_with(text, "POST V"))
    {
        const std::size_t version_end = text.find(' ', 6);
        if (version_end == std::string_view::npos)
        {
            return 0;
        }
        const std::size_t filename_start = version_end + 1;
        const std::size_t filename_end = text.find(' ', filename_start);
        if (filename_end == std::string_view::npos || !starts_with(text.substr(filename_end), " (Part "))
        {
            return 0;
        }
        int               tmppart{};
        const std::size_t part_end = parse_number(text, filename_end + 7, tmppart);
        if (tmppart == 0 || part_end == std::string_view::npos || part_end >= text.size() || text[part_end] != '/')
        {
            return 0;
        }
        int               tmptotal{};
        const std::size_t total_end = parse_number(text, part_end + 1, tmptotal);
        if (total_end == std::string_view::npos || tmppart > tmptotal || total_end >= text.size() ||
            text[total_end] != ')')
        {
            return 0;
        }
        store_filename(text.substr(filename_start, filename_end - filename_start));
        *partp = tmppart;
        *totalp = tmptotal;
        return 1;
    }
    if (starts_with(text, "File: "))
    {
        const std::size_t filename_start = 6;
        const std::size_t filename_end = text.find(' ', filename_start);
        if (filename_end == std::string_view::npos || !starts_with(text.substr(filename_end), " -- part "))
        {
            return 0;
        }
        int               tmppart{};
        const std::size_t part_end = parse_number(text, filename_end + 9, tmppart);
        if (tmppart == 0 || part_end == std::string_view::npos || !starts_with(text.substr(part_end), " of "))
        {
            return 0;
        }
        int               tmptotal{};
        const std::size_t total_end = parse_number(text, part_end + 4, tmptotal);
        if (total_end == std::string_view::npos || tmppart > tmptotal || !starts_with(text.substr(total_end), " -- "))
        {
            return 0;
        }
        store_filename(text.substr(filename_start, filename_end - filename_start));
        *partp = tmppart;
        *totalp = tmptotal;
        return 1;
    }
    if (starts_with(text, "[Section: "))
    {
        int               tmppart{};
        const std::size_t part_end = parse_number(text, 10, tmppart);
        if (tmppart == 0 || part_end == std::string_view::npos || part_end >= text.size())
        {
            return 0;
        }
        int               tmptotal{};
        const std::size_t total_end = parse_number(text, part_end + 1, tmptotal);
        if (total_end == std::string_view::npos)
        {
            return 0;
        }
        const std::size_t file_label = text.find_first_not_of(" \f\n\r\t\v", total_end);
        if (tmppart > tmptotal || file_label == std::string_view::npos ||
            !starts_with(text.substr(file_label), "File: "))
        {
            return 0;
        }
        const std::size_t filename_start = file_label + 6;
        const std::size_t filename_end = text.find(' ', filename_start);
        if (filename_end == std::string_view::npos)
        {
            return 0;
        }
        store_filename(text.substr(filename_start, filename_end - filename_start));
        *partp = tmppart;
        *totalp = tmptotal;
        return 1;
    }
    if (!filename.empty() && *partp > 0 && *totalp > 0 && *partp <= *totalp &&
        (starts_with(text, "BEGIN") || starts_with(text, "--- BEGIN ---") ||
         (!text.empty() && text[0] == 'M' && text.size() == UU_LENGTH)))
    {
        // Found the start of a section of uuencoded data
        // and have the part N of M information.
        //
        return 1;
    }
    if (starts_with_ignore_case(text, "x-file-name: "))
    {
        const std::size_t filename_end = text.find_first_of(" \f\n\r\t\v", 13);
        store_filename(text.substr(13, filename_end - 13));
        return 0;
    }
    if (starts_with_ignore_case(text, "x-part: "))
    {
        int tmppart{};
        if (parse_number(text, 8, tmppart) != std::string_view::npos && tmppart > 0)
        {
            *partp = tmppart;
        }
        return 0;
    }
    if (starts_with_ignore_case(text, "x-part-total: "))
    {
        int tmptotal{};
        if (parse_number(text, 14, tmptotal) != std::string_view::npos && tmptotal > 0)
        {
            *totalp = tmptotal;
        }
        return 0;
    }
    return 0;
}

DecodeState uudecode(std::FILE *ifp, DecodeState state)
{
    static std::FILE *ofp = nullptr;
    static int   line_length;
    std::string       lastline;
    lastline.reserve(UU_LENGTH + 1);

    if (state == DECODE_DONE)
    {
all_done:
        if (ofp)
        {
            std::fclose(ofp);
            ofp = nullptr;
        }
        return state;
    }

    while (ifp ? std::fgets(g_buf, sizeof g_buf, ifp) : read_art(g_buf, sizeof g_buf))
    {
        if (!ifp && mime_end_of_section(g_buf))
        {
            break;
        }
        char *p = std::strchr(g_buf, '\r');
        if (p)
        {
            p[0] = '\n';
            p[1] = '\0';
        }
        switch (state)
        {
        case DECODE_START:    // Looking for start of uuencoded file
        case DECODE_MAYBE_DONE:
        {
            if (std::strncmp(g_buf, "begin ", 6) != 0)
            {
                break;
            }
            // skip mode
            p = skip_non_space(g_buf + 6);
            p = skip_space(p);
            char *filename = p;
            while (*p && (!std::isspace(*p) || *p == ' '))
            {
                p++;
            }
            *p = '\0';
            if (!*filename)
            {
                return DECODE_ERROR;
            }
            const std::string decode_filename = decode_fix_filename(filename);
            g_decode_filename = decode_filename;

            // Create output file and start decoding
            ofp = std::fopen(decode_filename.c_str(), "wb");
            if (!ofp)
            {
                return DECODE_ERROR;
            }
            std::printf("Decoding %s\n", decode_filename.c_str());
            term_down(1);
            state = DECODE_SET_LEN;
            break;
        }

        case DECODE_INACTIVE: // Looking for uuencoded data to resume
            if (*g_buf != 'M' || std::strlen(g_buf) != line_length)
            {
                if (*g_buf == 'B' && !std::strncmp(g_buf, "BEGIN", 5))
                {
                    state = DECODE_ACTIVE;
                }
                break;
            }
            state = DECODE_ACTIVE;
            // FALL THROUGH

        case DECODE_SET_LEN:
            line_length = std::strlen(g_buf);
            state = DECODE_ACTIVE;
            // FALL THROUGH

        case DECODE_ACTIVE:   // Decoding data
            if (*g_buf == 'M' && std::strlen(g_buf) == line_length)
            {
                uudecode_line(g_buf, ofp);
                break;
            }
            if ((int)std::strlen(g_buf) > line_length)
            {
                state = DECODE_INACTIVE;
                break;
            }
            // May be nearing end of file, so save this line
            lastline = g_buf;
            // some encoders put the end line right after the last M line
            if (!std::strncmp(g_buf, "end", 3))
            {
                goto end;
            }
            else if (*g_buf == ' ' || *g_buf == '`')
            {
                state = DECODE_LAST;
            }
            else
            {
                state = DECODE_NEXT_TO_LAST;
            }
            break;

        case DECODE_NEXT_TO_LAST:// May be nearing end of file
            if (!std::strncmp(g_buf, "end", 3))
            {
                goto end;
            }
            else if (*g_buf == ' ' || *g_buf == '`')
            {
                state = DECODE_LAST;
            }
            else
            {
                state = DECODE_INACTIVE;
            }
            break;

        case DECODE_LAST:     // Should be at end of file
            if (!std::strncmp(g_buf, "end", 3) && std::isspace(g_buf[3]))
            {
                // Handle that last line we saved
                lastline.resize(std::max(lastline.size(), static_cast<std::string::size_type>(UU_LENGTH + 1)), '\0');
                uudecode_line(lastline.data(), ofp);
                lastline.clear();
end:            if (ofp)
                {
                    std::fclose(ofp);
                    ofp = nullptr;
                }
                state = DECODE_MAYBE_DONE;
            }
            else
            {
                state = DECODE_INACTIVE;
            }
            break;

        case DECODE_DONE:
            break;
        }
    }

    if (state == DECODE_DONE || state == DECODE_MAYBE_DONE)
    {
        goto all_done;
    }

    return DECODE_INACTIVE;
}

#define DEC(c)  (((c) - ' ') & 077)

// Decode a uuencoded line to 'ofp'
static void uudecode_line(char *line, std::FILE *ofp)
{
    // Calculate expected length and pad if necessary
    int len = ((DEC(line[0]) + 2) / 3) * 4;
    len = std::min(len, static_cast<int>(UU_LENGTH));
    for (int c = std::strlen(line) - 1; c <= len; c++)
    {
        line[c] = ' ';
    }

    len = DEC(*line++);
    while (len)
    {
        int c = DEC(*line) << 2 | DEC(line[1]) >> 4;
        std::putc(c, ofp);
        if (--len)
        {
            c = DEC(line[1]) << 4 | DEC(line[2]) >> 2;
            std::putc(c, ofp);
            if (--len)
            {
                c = DEC(line[2]) << 6 | DEC(line[3]);
                std::putc(c, ofp);
                len--;
            }
        }
        line += 4;
    }
}
