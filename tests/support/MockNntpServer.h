// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_TESTS_SUPPORT_MOCKNNTPSERVER_H
#define TRN_TESTS_SUPPORT_MOCKNNTPSERVER_H

#include <memory>
#include <string>

class MockNntpServer
{
public:
    MockNntpServer();
    ~MockNntpServer();

    MockNntpServer(const MockNntpServer &) = delete;
    MockNntpServer &operator=(const MockNntpServer &) = delete;

    void set_welcome_banner(std::string line);
    void start();
    int  port() const;
    void stop();
    void verify();

private:
    struct Impl;

    bool build_welcome_banner(std::string &line);
    void record_failure(std::string message);
    void run();
    bool stopping() const;

    std::unique_ptr<Impl> m_impl;
};

#endif
