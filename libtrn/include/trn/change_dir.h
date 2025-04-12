#pragma once

#include <filesystem>

inline bool change_dir(const std::filesystem::path &path)
{
    std::error_code ec;
    current_path(path, ec);
    return static_cast<bool>(ec);
}
