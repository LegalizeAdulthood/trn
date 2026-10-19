/* trn/yenc.h */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_YENC_H
#define TRN_YENC_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

struct YencHeader
{
    std::string   filename;
    std::uint64_t size{};
    std::uint64_t line_length{};
    int           part{1};
    int           total{1};
    bool          multipart{};
};

struct YencPart
{
    YencHeader                   header;
    std::uint64_t                begin{};
    std::uint64_t                end{};
    std::string                  data;
    std::optional<std::uint32_t> crc32;
};

std::optional<YencHeader> yenc_prescan(std::string_view line);
std::optional<YencPart>   yenc_decode(std::string_view text);
std::uint32_t             yenc_crc32(std::string_view bytes, std::uint32_t previous = 0);

#endif
