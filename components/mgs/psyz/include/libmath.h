#ifndef LIBMATH_H
#define LIBMATH_H

/* Math error handling */
extern int math_errno;

/* Error codes */
#define EDOM 33   /**< Domain error */
#define ERANGE 34 /**< Range error */

/* Constants */
#define HUGE_VAL 1.797693134862316e+308 /**< Huge value for overflow */

/**
 * @brief Arccosine
 *
 * Determines the arccosine of x.
 *
 * @param x Value whose arccosine is to be determined, ranging from -1 to 1
 * @return Arccosine of x, ranging from 0 to pi. If fabs(x)>1, 0 is returned and
 * math_errno is set to EDOM
 */
double acos(double x);

/**
 * @brief Arcsine
 *
 * Determines the arcsine of x.
 *
 * @param x Value whose arcsine is to be determined, ranging from -1 to 1
 * @return Arcsine of x, ranging from -pi/2 to pi/2. If fabs(x)>1, 0 is returned
 * and math_errno is set to EDOM
 */
double asin(double x);

/**
 * @brief Arctangent
 *
 * Determines the arctangent of x.
 *
 * @param x Value whose arctangent is to be calculated
 * @return Arctangent of x, ranging from -pi/2 to pi/2 radians
 */
double atan(double x);

/**
 * @brief Arctangent of y/x
 *
 * Determines the arctangent of x/y. If x and y are 0, a value of 0 is returned.
 *
 * @param x Floating-point value
 * @param y Floating-point value
 * @return Arctangent of x/y, ranging from -pi to pi
 */
double atan2(double x, double y);

/**
 * @brief Convert a string to a floating-point equivalent
 *
 * Converts a string s to its floating-point (double type) equivalent.
 *
 * @param s Pointer to a string
 * @return The result from converting input string s to a double floating point
 * equivalent. On overflow, returns +/-HUGE_VAL and sets math_errno to ERANGE.
 * On underflow, returns 0 and sets math_errno to ERANGE.
 */
double atof(char* s);

/**
 * @brief Minimum integer not less than x (ceiling function)
 *
 * Determines the minimum integer (double type) not less than x.
 *
 * @param x Floating-point value
 * @return Minimum integer (double type) not less than x
 */
double ceil(double x);

/**
 * @brief Cosine
 *
 * Determines the cosine of x (in radians).
 *
 * @param x Angle in radians
 * @return Cosine of x, ranging from -1 to 1
 */
double cos(double x);

/**
 * @brief Hyperbolic cosine
 *
 * Determines the hyperbolic cosine of x.
 *
 * @param x Floating-point value
 * @return Hyperbolic cosine of x. On overflow, returns +/-HUGE_VAL and sets
 * math_errno to ERANGE
 */
double cosh(double x);

/**
 * @brief Exponential function
 *
 * Determines e to the power of x.
 *
 * @param x Power
 * @return e^x. On overflow, returns HUGE_VAL and sets math_errno to ERANGE. On
 * underflow, returns 0
 */
double exp(double x);

/**
 * @brief Absolute value
 *
 * Determines the absolute value of x.
 *
 * @param x Floating-point value
 * @return Absolute value of x
 */
double fabs(double x);

/**
 * @brief Maximum integer not greater than x (floor function)
 *
 * Determines the maximum integer (double type) not greater than x.
 *
 * @param x Floating-point value
 * @return Maximum integer (double type) not greater than x
 */
double floor(double x);

/**
 * @brief Floating-point remainder
 *
 * Determines the floating-point remainder when x is divided by y.
 *
 * @param x Dividend
 * @param y Divisor
 * @return Floating-point remainder of x/y. If y is 0, returns 0 and sets
 * math_errno to EDOM
 */
double fmod(double x, double y);

/**
 * @brief Split floating-point number into mantissa and exponent
 *
 * Splits floating-point number x into mantissa m and exponent n (x = m * 2^n).
 * The absolute value of m is 0.5 <= |m| < 1.0.
 *
 * @param x Floating-point value
 * @param n Pointer to storage location for exponent
 * @return Mantissa m
 */
double frexp(double x, int* n);

/**
 * @brief Hypotenuse
 *
 * Determines sqrt(x^2 + y^2).
 *
 * @param x Floating-point value
 * @param y Floating-point value
 * @return sqrt(x^2 + y^2). On overflow, returns HUGE_VAL and sets math_errno to
 * ERANGE
 */
double hypot(double x, double y);

/**
 * @brief Load exponent
 *
 * Determines x * 2^n.
 *
 * @param x Floating-point value
 * @param n Integer exponent
 * @return x * 2^n. On overflow, returns +/-HUGE_VAL and sets math_errno to
 * ERANGE. On underflow, returns 0
 */
double ldexp(double x, int n);

/**
 * @brief Natural logarithm
 *
 * Determines the natural logarithm of x.
 *
 * @param x Floating-point value (must be positive)
 * @return Natural logarithm of x. If x<=0, returns -HUGE_VAL and sets
 * math_errno to EDOM
 */
double log(double x);

/**
 * @brief Base 10 logarithm
 *
 * Determines the base 10 logarithm of x.
 *
 * @param x Floating-point value (must be positive)
 * @return Base 10 logarithm of x. If x<=0, returns -HUGE_VAL and sets
 * math_errno to EDOM
 */
double log10(double x);

/**
 * @brief Split into integer and fractional parts
 *
 * Splits floating-point number x into integer and fractional parts, both with
 * the same sign as x.
 *
 * @param x Floating-point value
 * @param i Pointer to storage location for integer part
 * @return Fractional part of x
 */
double modf(double x, double* i);

/**
 * @brief Power function
 *
 * Determines x to the power of y (x^y).
 *
 * @param x Base
 * @param y Exponent
 * @return x^y. On overflow, returns +/-HUGE_VAL and sets math_errno to ERANGE.
 *         If x<0 and y is not an integer, returns 0 and sets math_errno to
 * EDOM. If x=0 and y<=0, returns 0 and sets math_errno to EDOM
 */
double pow(double x, double y);

/**
 * @brief Print formatted output with floating-point support
 *
 * Same as printf() but supports floating-point format specifiers (f, e, E, g,
 * G).
 *
 * @param fmt Pointer to input format character string
 * @return The length of the output character string. If an error is generated,
 * returns NULL
 */
int printf2(char* fmt, ...);

/**
 * @brief Sine
 *
 * Determines the sine of x (in radians).
 *
 * @param x Angle in radians
 * @return Sine of x, ranging from -1 to 1
 */
double sin(double x);

/**
 * @brief Hyperbolic sine
 *
 * Determines the hyperbolic sine of x.
 *
 * @param x Floating-point value
 * @return Hyperbolic sine of x. On overflow, returns +/-HUGE_VAL and sets
 * math_errno to ERANGE
 */
double sinh(double x);

/**
 * @brief Write formatted output to a string with floating-point support
 *
 * Same as sprintf() but supports floating-point format specifiers (f, e, E, g,
 * G).
 *
 * @param s Storage location for variable character string
 * @param fmt Input format character string
 * @return The length of the output character string. NULL is returned when an
 * error occurs
 */
long sprintf2(char* s, const char* fmt, ...);

/**
 * @brief Square root
 *
 * Determines the square root of x.
 *
 * @param x Floating-point value (must be non-negative)
 * @return Square root of x. If x<0, returns 0 and sets math_errno to EDOM
 */
double sqrt(double x);

/**
 * @brief Convert string to double
 *
 * Converts a character string s to double type.
 *
 * @param s Pointer to character string
 * @param endp Storage destination of pointer to a non-convertible character
 * string
 * @return The result obtained by converting the input value s to a double.
 *         On overflow, returns +/-HUGE_VAL and sets math_errno to ERANGE. On
 * underflow, returns 0 and sets math_errno to ERANGE
 */
double strtod(char* s, char** endp);

/**
 * @brief Tangent
 *
 * Determines the tangent of x (in radians).
 *
 * @param x Angle in radians
 * @return Tangent of x
 */
double tan(double x);

/**
 * @brief Hyperbolic tangent
 *
 * Determines the hyperbolic tangent of x.
 *
 * @param x Floating-point value
 * @return Hyperbolic tangent of x, ranging from -1 to 1
 */
double tanh(double x);

#endif
