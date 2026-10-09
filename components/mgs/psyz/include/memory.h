#ifndef MEMORY_H
#define MEMORY_H

typedef unsigned char u_char;

#ifndef __psyz

/**
 * @brief Compare memory blocks
 *
 * Compares the first n bytes of b1 and b2.
 *
 * @param b1 Pointer to first block
 * @param b2 Pointer to second block
 * @param n Number of bytes to be compared
 * @return 0 if b1==b2, <0 if b1<b2, >0 if b1>b2
 */
int bcmp(u_char* b1, u_char* b2, int n);

/**
 * @brief Copy a memory block
 *
 * Copies the first n bytes of src to dest.
 *
 * @param src Pointer to copy source
 * @param dest Pointer to copy destination
 * @param n Number of bytes copied
 */
void bcopy(u_char* src, u_char* dest, int n);

/**
 * @brief Fill a memory block with zeros
 *
 * Sets n bytes to the value 0, starting from p.
 *
 * @param p Pointer to memory block
 * @param n Size
 */
void bzero(u_char* p, int n);

/**
 * @brief Search a memory block for a character
 *
 * Searches the memory block of n bytes starting from s, looking for the first
 * appearance of the character c.
 *
 * @param s Pointer to memory block
 * @param c Character
 * @param n Number of bytes
 * @return A pointer to the location at which c was found. If c was not found,
 * NULL is returned.
 */
void* memchr(u_char* s, u_char c, int n);

/**
 * @brief Compare memory blocks
 *
 * Compares the first n bytes of s1 and s2.
 *
 * @param s1 Pointer to first memory block
 * @param s2 Pointer to second memory block
 * @param n Number of bytes to be compared
 * @return 0 if s1=s2, <0 if s1<s2, >0 if s1>s2
 */
void* memcmp(u_char* s1, u_char* s2, int n);

/**
 * @brief Copy a memory block
 *
 * Copies the first n bytes of src to dest.
 *
 * @param dest Pointer to copy destination memory block
 * @param src Pointer to copy source memory block
 * @param n Number of bytes copied
 * @return Pointer to destination (dest)
 */
void* memcpy(u_char* dest, u_char* src, int n);

/**
 * @brief Copy a memory block
 *
 * Copies the first n bytes of src to dest. The block is copied correctly, even
 * between overlapping objects.
 *
 * @param dest Pointer to copy destination memory block
 * @param src Pointer to copy source memory block
 * @param n Number of bytes copied
 * @return Pointer to destination (dest)
 */
void* memmove(u_char* dest, u_char* src, int n);

/**
 * @brief Write a character to a memory block
 *
 * Writes c to the first n bytes of s.
 *
 * @param s Pointer to memory block
 * @param c Character
 * @param n Number of characters
 * @return Pointer to block (s)
 */
void* memset(u_char* s, u_char c, int n);
#endif

#endif
