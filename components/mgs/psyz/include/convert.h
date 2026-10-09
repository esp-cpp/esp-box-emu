#ifndef CONVERT_H
#define CONVERT_H

/**
 * @brief Convert a string to an integer
 *
 * Converts a string to its integer equivalent. On this system, it is equivalent
 * to atol().
 *
 * @param s Pointer to a character string
 * @return Integer equivalent of s
 */
int atoi(char* s);

/**
 * @brief Convert a character string to a long
 *
 * Converts a string to its long equivalent. On this system, it is equivalent to
 * atoi().
 *
 * @param s Pointer to a character string
 * @return Integer equivalent of s
 */
long atol(char* s);

/**
 * @brief Calculate absolute value
 *
 * Calculates the absolute value of i.
 *
 * @param i Long value
 * @return Absolute value of the argument
 */
long labs(long i);

/**
 * @brief Convert a character string to a long
 *
 * Converts a character string s to a long (the same as an int in R3000). s must
 * be formatted as follows: [ws][sn][ddd] [ws] white space (may be omitted) [sn]
 * sign (may be omitted) [ddd] number string (may be omitted)
 *
 * @param s Pointer to character string
 * @param endp Storage destination of pointer to a non-convertible character
 * string
 * @param base Radix specification
 * @return The result obtained by converting the input value s to a long. If an
 * error is generated, it returns 0.
 */
long strtol(char* s, char** endp, unsigned int base);

/**
 * @brief Convert a character string to an unsigned long
 *
 * Converts a character string s to unsigned long type (the same as unsigned int
 * type in R3000). s must be formatted as follows: [ws][sn][ddd] [ws] white
 * space (may be omitted) [sn] sign (may be omitted) [ddd] number string (may be
 * omitted)
 *
 * @param s Pointer to character string
 * @param endp Storage destination of pointer to a non-convertible character
 * string
 * @param base Radix specification
 * @return The result obtained by converting the input value s to a long
 */
unsigned long strtoul(char* s, char** endp, int base);

#endif
