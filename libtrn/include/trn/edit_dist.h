/* trn/edit_dist.h
*/
/* The authors make no claims as to the fitness or correctness of this software
 * for any use whatsoever, and it is provided as is. Any use of this software
 * is at the user's own risk.
 */
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_EDIT_DIST_H
#define TRN_EDIT_DIST_H

#include <string_view>

int edit_distn(std::string_view from, std::string_view to);

#endif
