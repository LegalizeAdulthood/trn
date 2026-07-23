/* post_response.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_INEWS_POST_RESPONSE_H
#define TRN_INEWS_POST_RESPONSE_H

#include <string>
#include <string_view>

int         inews_post_response_code(std::string_view response);
std::string inews_post_failure_message(std::string_view response);

#endif
