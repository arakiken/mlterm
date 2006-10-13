/*
 *	$Id$
 */

#ifndef  __KIK_UTIL_H__
#define  __KIK_UTIL_H__


#define  K_MAX(n1,n2)  ((n1) > (n2) ? (n1) : (n2))

#define  K_MIN(n1,n2)  ((n1) > (n2) ? (n2) : (n1))

/* TYPE: MIN(signed) -- MAX(unsigned) (number of bytes needed)
 * char  : -128 -- 256 (4)
 * int16 : -32768 -- 65536 (6)
 * int32 : -2147483648 -- 4294967296 (11)
 * int64 : -9223372036854775808 -- 18446744073709551616 (20)
 * 
 * Since log10(2^8) = 2.4..., (sizeof(n)*3) is large enough
 * for all n >= 2.
 */
#define  DIGIT_STR_LEN(n)  \
	((sizeof(n) == 1) ? 4 : \
	(sizeof(n) == 2) ? 6 : \
	(sizeof(n) == 4) ? 11 : \
	(sizeof(n) == 8) ? 20 : (sizeof(n)*3))


#endif
