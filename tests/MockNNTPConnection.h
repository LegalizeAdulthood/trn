// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_TESTS_MOCKNNTPCONNECTION_H
#define TRN_TESTS_MOCKNNTPCONNECTION_H

#include <nntp/nntpclient.h>

#include <gmock/gmock.h>

#include <cstddef>
#include <string>
#include <string_view>

class MockNNTPConnection : public INNTPConnection
{
public:
    ~MockNNTPConnection() override = default;

    MOCK_METHOD(std::string, read_line, (error_code_t &), (override));
    MOCK_METHOD(void, write_line, (const std::string &, error_code_t &), (override));
    MOCK_METHOD(void, write, (std::string_view, error_code_t &), (override));
    MOCK_METHOD(std::size_t, read, (char *, std::size_t, error_code_t &), (override));
};

#endif
