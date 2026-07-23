/* post_response.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <inews/post_response.h>

#include <charconv>
#include <cstddef>
#include <system_error>

int inews_post_response_code(std::string_view response)
{
    int               status{};
    const char *const first = response.data();
    const char *const last = first + response.size();

    const std::from_chars_result result = std::from_chars(first, last, status);
    if (result.ptr == first || result.ec == std::errc::result_out_of_range)
    {
        return 0;
    }
    return status;
}

std::string inews_post_failure_message(std::string_view response)
{
    std::string_view server_message = response.size() > 4 ? response.substr(4) : std::string_view{};
    if (const std::size_t carriage_return = server_message.find('\r'); carriage_return != std::string_view::npos)
    {
        server_message = server_message.substr(0, carriage_return);
    }

    std::string post_failure;
    post_failure.reserve(server_message.size());
    for (const char ch : server_message)
    {
        post_failure += ch == '\\' ? '\n' : ch;
    }
    return post_failure;
}
