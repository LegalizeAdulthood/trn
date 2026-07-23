/* wildmat.h
 */
#ifndef TRN_WILDMAT_H
#define TRN_WILDMAT_H

#include <string_view>

bool wildcard_match(std::string_view text, std::string_view pattern);

#endif
