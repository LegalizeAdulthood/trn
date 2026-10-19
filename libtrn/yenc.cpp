/* yenc.cpp */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/yenc.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <utility>
#include <vector>

namespace
{
using Fields = std::vector<std::pair<std::string_view, std::string_view>>;

std::string_view strip_line_end(std::string_view line)
{
    if (!line.empty() && line.back() == '\n')
    {
        line.remove_suffix(1);
    }
    if (!line.empty() && line.back() == '\r')
    {
        line.remove_suffix(1);
    }
    return line;
}

std::string_view next_line(std::string_view &text)
{
    const std::size_t      end = text.find('\n');
    const std::string_view line = text.substr(0, end);
    text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
    return strip_line_end(line);
}

std::optional<std::string_view> field(const Fields &fields, std::string_view name)
{
    const auto found =
        std::find_if(fields.begin(), fields.end(), [name](const auto &entry) { return entry.first == name; });
    return found == fields.end() ? std::nullopt : std::optional<std::string_view>{found->second};
}

std::optional<Fields> parse_fields(std::string_view text, bool has_filename = false)
{
    Fields fields;
    while (!text.empty())
    {
        const std::size_t start = text.find_first_not_of(" \t");
        if (start == std::string_view::npos)
        {
            break;
        }
        text.remove_prefix(start);
        const std::size_t      end = text.find_first_of(" \t");
        const std::string_view token = text.substr(0, end);
        const std::size_t      equal = token.find('=');
        if (equal == std::string_view::npos || equal == 0 || field(fields, token.substr(0, equal)))
        {
            return std::nullopt;
        }
        const std::string_view name = token.substr(0, equal);
        if (has_filename && name == "name")
        {
            std::string_view  value = text.substr(equal + 1);
            const std::size_t first = value.find_first_not_of(" \t");
            if (first == std::string_view::npos)
            {
                return std::nullopt;
            }
            value.remove_prefix(first);
            value = value.substr(0, value.find_last_not_of(" \t") + 1);
            fields.emplace_back(name, value);
            break;
        }
        fields.emplace_back(name, token.substr(equal + 1));
        text.remove_prefix(end == std::string_view::npos ? text.size() : end);
    }
    return fields;
}

template <typename T>
std::optional<T> number(const Fields &fields, std::string_view name, int base = 10)
{
    const auto value = field(fields, name);
    if (!value || value->empty())
    {
        return std::nullopt;
    }
    T          result{};
    const auto parsed = std::from_chars(value->data(), value->data() + value->size(), result, base);
    if (parsed.ec != std::errc{} || parsed.ptr != value->data() + value->size())
    {
        return std::nullopt;
    }
    return result;
}

constexpr std::array<std::uint32_t, 256> crc_table()
{
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < table.size(); ++i)
    {
        std::uint32_t crc = i;
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320U : 0);
        }
        table[i] = crc;
    }
    return table;
}
constexpr auto CRC_TABLE = crc_table();
} // namespace

std::uint32_t yenc_crc32(std::string_view bytes, std::uint32_t previous)
{
    std::uint32_t crc = ~previous;
    for (const unsigned char byte : bytes)
    {
        crc = (crc >> 8) ^ CRC_TABLE[(crc ^ byte) & 0xFF];
    }
    return ~crc;
}

std::optional<YencHeader> yenc_prescan(std::string_view line)
{
    line = strip_line_end(line);
    if (line.substr(0, 8) != "=ybegin ")
    {
        return std::nullopt;
    }
    const auto fields = parse_fields(line.substr(8), true);
    if (!fields)
    {
        return std::nullopt;
    }
    const auto length = number<std::uint64_t>(*fields, "line");
    const auto size = number<std::uint64_t>(*fields, "size");
    const auto name = field(*fields, "name");
    if (!length || *length == 0 || !size || !name || name->empty())
    {
        return std::nullopt;
    }
    YencHeader result{std::string{*name}, *size, *length};
    if (field(*fields, "part"))
    {
        const auto part = number<int>(*fields, "part");
        if (!part || *part < 1 || *size == 0)
        {
            return std::nullopt;
        }
        result.multipart = true;
        result.part = *part;
        result.total = 0;
    }
    if (field(*fields, "total"))
    {
        const auto total = number<int>(*fields, "total");
        if (!result.multipart || !total || *total < result.part)
        {
            return std::nullopt;
        }
        result.total = *total;
    }
    return result;
}

std::optional<YencPart> yenc_decode(std::string_view text)
{
    std::optional<YencHeader> header;
    while (!text.empty() && !header)
    {
        header = yenc_prescan(next_line(text));
    }
    if (!header)
    {
        return std::nullopt;
    }
    YencPart result{*header, 1, header->size, {}, {}};
    if (header->multipart)
    {
        const std::string_view line = next_line(text);
        const auto             fields = line.substr(0, 7) == "=ypart " ? parse_fields(line.substr(7)) : std::nullopt;
        if (!fields)
        {
            return std::nullopt;
        }
        const auto begin = number<std::uint64_t>(*fields, "begin");
        const auto end = number<std::uint64_t>(*fields, "end");
        if (!begin || !end || *begin == 0 || *end < *begin || *end > header->size)
        {
            return std::nullopt;
        }
        result.begin = *begin;
        result.end = *end;
    }
    const std::uint64_t expected = header->multipart ? result.end - result.begin + 1 : header->size;
    while (!text.empty())
    {
        const std::string_view line = next_line(text);
        if (line.substr(0, 6) == "=yend ")
        {
            const auto fields = parse_fields(line.substr(6));
            if (!fields || number<std::uint64_t>(*fields, "size") != expected || result.data.size() != expected)
            {
                return std::nullopt;
            }
            for (const std::string_view key : {"crc32", "pcrc32"})
            {
                const auto raw = field(*fields, key);
                if (!raw)
                {
                    continue;
                }
                const auto crc = number<std::uint32_t>(*fields, key, 16);
                if (!crc || raw->size() != 8 ||
                    ((key == "pcrc32" || !header->multipart) && *crc != yenc_crc32(result.data)))
                {
                    return std::nullopt;
                }
                if (key == "crc32")
                {
                    result.crc32 = crc;
                }
            }
            if (header->multipart && (number<int>(*fields, "part") != header->part || !field(*fields, "pcrc32")))
            {
                return std::nullopt;
            }
            return result;
        }
        for (std::size_t i = 0; i < line.size(); ++i)
        {
            unsigned char byte = static_cast<unsigned char>(line[i]);
            if (byte == 0 || byte == '\r' || byte == '\n')
            {
                return std::nullopt;
            }
            if (byte == '=')
            {
                if (++i == line.size())
                {
                    return std::nullopt;
                }
                if (line[i] == '\0' || line[i] == '\r' || line[i] == '\n')
                {
                    return std::nullopt;
                }
                byte = static_cast<unsigned char>(static_cast<unsigned char>(line[i]) - 64);
            }
            if (result.data.size() >= expected)
            {
                return std::nullopt;
            }
            result.data += static_cast<char>(static_cast<unsigned char>(byte - 42));
        }
    }
    return std::nullopt;
}
