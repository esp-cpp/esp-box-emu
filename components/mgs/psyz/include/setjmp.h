#ifndef SETJMP_H
#define SETJMP_H

/**
 * @brief Jump buffer type for non-local jumps
 */
typedef long jmp_buf[32];

/**
 * @brief Defines non-local jump destination
 *
 * Stores the destination information for a non-local jump at p. If longjmp(p,
 * val) is executed, the system returns from setjmp().
 *
 * @param p Environment storage variable
 * @return Returns the value given to the second argument of longjmp() when the
 * jump is executed
 */
int setjmp(jmp_buf p);

/**
 * @brief Non-local jump
 *
 * Makes a non-local jump to the destination specified by p.
 *
 * @param p Environment storage variable
 * @param val setjmp() Return value
 */
void longjmp(jmp_buf p, int val);

#endif
