#ifndef ABS_H
#define ABS_H

/**
 * @brief Calculate absolute value
 *
 * Calculates the absolute value of the integer i. On the R3000, int and long
 * are the same size, so this function is equivalent to labs().
 *
 * @param i Integer
 * @return Absolute value of the argument
 */
int abs(int i);

#endif
