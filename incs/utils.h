#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdbool.h>

// Converts a long integer to a string.
void ltoa(long n, char *s);

// Converts a floating-point number to a string.
void ftoa(float n, char *s, int precision);

#endif // UTILS_H
