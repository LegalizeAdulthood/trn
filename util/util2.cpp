/* util2.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <util/util2.h>

#include <config/common.h>
#include <config/string_case_compare.h>
#include <tool/util3.h>
#include <trn/util.h>
#include <util/env.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

namespace fs = std::filesystem;

#ifdef TILDE_NAME
static std::string s_tilde_name;
static std::string s_tilde_dir;
#endif

static bool char_equal_ignore_case(char left, char right)
{
    return string_case_equal(std::string_view{&left, 1}, std::string_view{&right, 1});
}

// safe version of string copy
char *safe_copy(char *to, const char *from, int len)
{
    char* dest = to;

    if (from)
    {
        while (--len && *from)
        {
            *dest++ = *from++;
        }
    }
    *dest = '\0';

    return to;
}

// expand filename via %, ~, and $ interpretation
// Note that there is a 1-deep cache of ~name interpretation

std::string file_exp(std::string_view text)
{
    std::string            filename = do_interp(text);
    const std::string_view expanded{filename};
    if (!expanded.empty() && expanded.front() == '~') // does destination start with ~?
    {
        const std::string_view tilde_text = expanded.substr(1);
        if (tilde_text.empty() || tilde_text.front() == '/')
        {
            filename.replace(0, 1, g_home_dir);
        }
        else if (tilde_text.front() == '~' && (tilde_text.size() == 1 || tilde_text[1] == '/'))
        {
            // Preserve legacy no-op handling for ~~ expansion.
        }
        else
        {
#ifdef TILDE_NAME
            const std::string_view::const_iterator suffix =
                std::find_if(tilde_text.begin(), tilde_text.end(),
                             [](char ch) { return !std::isalnum(static_cast<unsigned char>(ch)); });
            std::string login_name{tilde_text.begin(), suffix};
            std::string suffix_text{suffix, tilde_text.end()};
            if (!s_tilde_dir.empty() && s_tilde_name == login_name)
            {
                filename = s_tilde_dir;
                filename += suffix_text;
            }
            else
            {
                s_tilde_dir.clear();
                s_tilde_name = login_name;
#ifdef HAS_GETPWENT // getpwnam() is not the paragon of efficiency
                {
                    struct passwd *pwd = getpwnam(s_tilde_name.c_str());
                    if (pwd == nullptr)
                    {
                        std::printf("%s is an unknown user. Using default.\n", s_tilde_name.c_str());
                        return {};
                    }
                    s_tilde_dir = pwd->pw_dir;
                    filename = s_tilde_dir;
                    filename += suffix_text;
                    endpwent();
                }
#else // this will run faster, and is less D space
                { // just be sure LOGIN_DIR_FIELD is correct
                    if (std::ifstream pfp{file_exp(PASSWORD_FILE)})
                    {
                        std::string buff;
                        while (std::getline(pfp, buff))
                        {
                            const std::string_view line{buff};
                            if (const std::size_t sep = line.find(':');
                                sep != std::string_view::npos && line.substr(0, sep) == s_tilde_name)
                            {
                                std::size_t begin = sep + 1;
                                std::size_t end = std::string_view::npos;
                                for (int i = LOGIN_DIR_FIELD - 2; i; i--)
                                {
                                    end = line.find(':', begin);
                                    if (end != std::string_view::npos)
                                    {
                                        begin = end + 1;
                                    }
                                }
                                if (end != std::string_view::npos)
                                {
                                    end = line.find(':', begin);
                                    s_tilde_dir = line.substr(begin, end - begin);
                                    filename = s_tilde_dir;
                                    filename += suffix_text;
                                }
                                break;
                            }
                        }
                    }
                    if (s_tilde_dir.empty())
                    {
                        std::printf("%s is an unknown user. Using default.\n", s_tilde_name.c_str());
                        return {};
                    }
                }
#endif
            }
#else // !TILDENAME
            if (g_verbose)
            {
                std::fputs("~loginname not implemented.\n", stdout);
            }
            else
            {
                std::fputs("~login not impl.\n", stdout);
            }
#endif
        }
    }
    else if (!expanded.empty() && expanded.front() == '$')
    { // starts with some env variable?
        std::string env_pattern{"%"};
        if (expanded.size() > 1 && expanded[1] == '{')
        {
            env_pattern.append(expanded.substr(2));
        }
        else
        {
            env_pattern += '{';
            const std::string_view::const_iterator suffix =
                std::find_if(expanded.begin() + 1, expanded.end(),
                             [](char ch) { return !std::isalnum(static_cast<unsigned char>(ch)); });
            env_pattern.append(expanded.begin() + 1, suffix);
            // skip over token
            env_pattern += '}';
            env_pattern.append(suffix, expanded.end());
        }
        // this might do some extra '%'s, but that's how the Mercedes Benz
        filename = do_interp(env_pattern);
    }
    return filename;
}

bool in_string(std::string_view haystack, std::string_view needle, bool case_matters)
{
    if (needle.empty())
    {
        return !haystack.empty();
    }
    if (case_matters)
    {
        return haystack.find(needle) != std::string_view::npos;
    }

    const std::string_view::const_iterator match =
        std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(), char_equal_ignore_case);
    return match != haystack.end();
}

AuthCredentials read_auth_file(const fs::path &file)
{
    if (file.empty())
    {
        return {};
    }

    AuthCredentials result;
    std::ifstream   fp{file};
    if (fp)
    {
        std::getline(fp, result.user);
        std::getline(fp, result.password);
    }
    return result;
}
