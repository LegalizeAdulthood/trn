/* parsedate.h
 */
#ifndef PARSE_DATE_H
#define PARSE_DATE_H

#include <ctime>

extern "C" std::time_t parsedate(const char *text);

#endif
