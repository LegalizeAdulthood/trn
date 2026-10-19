// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_TESTS_SUPPORT_MOCKNNTPSERVER_H
#define TRN_TESTS_SUPPORT_MOCKNNTPSERVER_H

#include <memory>
#include <string>
#include <vector>

class MockNntpServer
{
public:
    MockNntpServer();
    ~MockNntpServer();

    MockNntpServer(const MockNntpServer &) = delete;
    MockNntpServer &operator=(const MockNntpServer &) = delete;

    void expect_body(long article_number, std::vector<std::string> lines);
    void expect_capabilities(std::vector<std::string> lines);
    void expect_group(std::string group_name, long count, long first, long last);
    void expect_head(long article_number, std::vector<std::string> lines);
    void expect_list(std::vector<std::string> lines);
    void expect_listgroup(std::vector<long> article_numbers);
    void expect_mode_reader();
    void expect_quit();
    void expect_starttls(bool handshake_failure = false);
    void set_welcome_banner(std::string line);
    void start();
    int  port() const;
    void stop();
    void verify();

private:
    struct ExpectedCommand;
    struct Impl;

    bool build_welcome_banner(std::string &line);
    void consume_expected_command();
    void expect_command(std::string command, std::vector<std::string> response_lines);
    bool next_expected_command(ExpectedCommand &command) const;
    void record_failure(std::string message);
    void run();
    bool stopping() const;

    std::unique_ptr<Impl> m_impl;
};

#endif
