// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_MODERATORS_H
#define TRN_MODERATORS_H

#include <string>
#include <string_view>

// INN moderators files and RFC 6048 LIST MODERATORS share this format.
std::string moderator_address(std::string_view group, std::string_view rules);

#endif
