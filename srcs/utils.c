#include "utils.h"
#include <string.h>

// Converts a long integer to a string.
void ltoa(long n, char *s)
{
    int i = 0;
    bool is_negative = false;
    if (n < 0)
    {
        is_negative = true;
        n = -n;
    }

    do
    {
        s[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);

    if (is_negative)
    {
        s[i++] = '-';
    }
    s[i] = '\0';

    // Reverse the string
    for (int j = 0, k = i - 1; j < k; j++, k--)
    {
        char temp = s[j];
        s[j] = s[k];
        s[k] = temp;
    }
}

// Converts a floating-point number to a string.
void ftoa(float n, char *s, int precision)
{
    // Handle negative numbers
    if (n < 0)
    {
        *s++ = '-';
        n = -n;
    }

    // Handle zero case
    if (n == 0.0f)
    {
        *s++ = '0';
        if (precision > 0)
        {
            *s++ = '.';
            for (int i = 0; i < precision; i++)
            {
                *s++ = '0';
            }
        }
        *s = '\0';
        return;
    }

    // Extract the integer part
    long int_part = (long)n;
    char int_str[20]; // Buffer for integer part
    ltoa(int_part, int_str);
    strcpy(s, int_str);

    // Move the pointer to the end of the integer part
    while (*s != '\0')
    {
        s++;
    }

    // Add the decimal point
    if (precision > 0)
    {
        *s++ = '.';

        // Extract the fractional part
        float frac_part = n - (float)int_part;
        for (int i = 0; i < precision; i++)
        {
            frac_part *= 10;
            int digit = (int)frac_part;
            *s++ = digit + '0';
            frac_part -= digit;
        }
    }

    // Null-terminate the string
    *s = '\0';
}
