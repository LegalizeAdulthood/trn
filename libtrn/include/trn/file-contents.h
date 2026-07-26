/* file_contents.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef FILE_CONTENTS_H
#define FILE_CONTENTS_H

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

inline std::string file_contents(const std::filesystem::path &path)
{
    std::ifstream input{path};
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

#endif
