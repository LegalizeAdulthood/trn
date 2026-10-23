// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

namespace
{
std::string environment(const char *name)
{
    const char *value = std::getenv(name);
    return value ? value : "";
}

std::string read_file(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

void write_file(const std::filesystem::path &path, const std::string &text)
{
    std::ofstream output(path, std::ios::binary);
    output << text;
}
} // namespace

int main(int argc, char **argv)
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    if (argc < 2)
    {
        return 2;
    }
    const std::string           role = std::string{argv[1]} == "--disable" && argc > 2 ? argv[2] : argv[1];
    const std::string           mode = environment("PNEWS_TEST_MODE");
    const std::filesystem::path directory = environment("PNEWS_TEST_DIRECTORY");
    std::ofstream log(directory / (role == "article-saver" ? "article-saver.log" : "tools.log"), std::ios::app);
    log << role << '\n';
    for (int i = 2; i < argc; ++i)
    {
        log << argv[i] << '\n';
    }
    log.close();
    if (mode == role + "_failure" || mode == role + "_cancel")
    {
        return mode == role + "_cancel" ? 130 : 3;
    }
    if (role == "/c" || role == "-c")
    {
#ifdef _WIN32
        const std::string command_line = GetCommandLineA();
        const std::size_t start = command_line.find(" /c ");
        if (start == std::string::npos)
        {
            return 12;
        }
        const char *command = command_line.c_str() + start + 4;
#else
        if (argc != 3)
        {
            return 12;
        }
        const char *command = argv[2];
#endif
        write_file(directory / "shell-used", role);
        return std::system(command) == 0 ? 0 : 3;
    }
    if (role == "article-saver")
    {
        if (argc != 7)
        {
            return 11;
        }
        std::ifstream input{argv[2], std::ios::binary};
        input.seekg(std::stoll(argv[3]));
        std::ofstream output{argv[4], std::ios::binary | std::ios::app};
        if (!input || !output)
        {
            return 11;
        }
        output << input.rdbuf();
        output.close();
        return input.bad() || output.fail() ? 11 : 0;
    }
    if (role == "editor")
    {
        for (int i = 2; i < argc; ++i)
        {
            const std::filesystem::path path = argv[i];
            if (path.filename().string().find(".article.") == 0 || path.filename().string().find(".letter.") == 0)
            {
                std::string text = read_file(path);
                text += "mispelled editor body\n";
                write_file(path, text);
                return 0;
            }
        }
        return 4;
    }
    if (role == "poster")
    {
        if (argc != 3 || std::string{argv[2]} != "-h")
        {
            return 5;
        }
        const std::string text{std::istreambuf_iterator<char>{std::cin}, std::istreambuf_iterator<char>{}};
        write_file(directory / "posted", text);
        return 0;
    }
    if (role == "mailer")
    {
        const std::string input{std::istreambuf_iterator<char>{std::cin}, std::istreambuf_iterator<char>{}};
        if (input.find("silent\n") != 0)
        {
            write_file(directory / "mailed", input);
            return 0;
        }
        std::istringstream lines{input};
        std::string        line;
        std::string        payload;
        std::string        recipients;
        while (std::getline(lines, line))
        {
            const std::size_t equal = line.find(" = ");
            if (equal == std::string::npos)
            {
                continue;
            }
            std::istringstream value{line.substr(equal + 3)};
            std::string        text;
            value >> std::quoted(text);
            if (line.substr(0, equal) == "upload-file")
            {
                payload = text;
            }
            else if (line.substr(0, equal) == "mail-rcpt")
            {
                recipients += text + '\n';
            }
        }
        if (payload.empty() || recipients.empty())
        {
            return 10;
        }
        write_file(directory / "mailed", read_file(payload));
        write_file(directory / "recipients", recipients);
        if (mode != "mailer_unconfirmed")
        {
            std::cout << (mode == "mailer_handoff" ? "202" : "250");
        }
        return 0;
    }
    if (role == "artcheck")
    {
        return argc == 6 && std::string{argv[3]} == "79" && std::filesystem::exists(argv[2]) ? 0 : 6;
    }
    if (role == "nntplist")
    {
        if (argc != 5 || std::string{argv[2]} != "-o")
        {
            return 7;
        }
        write_file(argv[3], std::string{argv[4]} == "active" ? "comp.test 1 1 y\n" : "comp.test Test group\n");
        return 0;
    }
    if (role == "spell")
    {
        const std::filesystem::path path = argv[argc - 1];
        std::string                 text = read_file(path);
        for (std::size_t at = text.find("mispelled"); at != std::string::npos; at = text.find("mispelled", at + 9))
        {
            text.replace(at, 9, "corrected");
        }
        write_file(path, text);
        return 0;
    }
    if (role == "saver")
    {
        if (argc != 10 || std::string{argv[5]} != "0" || std::string{argv[6]} != "0" ||
            std::string{argv[7]} != "Pnews" || std::string{argv[9]}.find("From ") != 0)
        {
            return 8;
        }
        write_file(argv[8], read_file(argv[2]));
        return 0;
    }
    if (role == "pgp")
    {
        for (int i = 2; i < argc; ++i)
        {
            if (std::string{argv[i]} == "-o")
            {
                return 0;
            }
        }
        std::string body{std::istreambuf_iterator<char>{std::cin}, std::istreambuf_iterator<char>{}};
        if (std::find_if(argv + 2, argv + argc, [](const char *arg) { return std::string{arg} == "-fast"; }) !=
            argv + argc)
        {
            std::cout << "-----BEGIN PGP SIGNED MESSAGE-----\n" << body << "-----BEGIN PGP SIGNATURE-----\n";
        }
        else
        {
            for (const std::string marker : {"-----BEGIN PGP SIGNED MESSAGE-----\n", "-----BEGIN PGP SIGNATURE-----\n"})
            {
                const std::size_t at = body.find(marker);
                if (at != std::string::npos)
                {
                    body.erase(at, marker.size());
                }
            }
            std::cout << body;
        }
        return 0;
    }
    return role == "pager" ? 0 : 9;
}
