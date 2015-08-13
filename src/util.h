#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdbool.h>
#include <stdint.h>

int column(char const *, uint16_t, char *, uint16_t);
char const *next_line(char const *);

bool startswith(char const *, char const *);
bool equal(char const *, char const *);

#endif /* _UTIL_H_ */
