/*
 *	$Id$
 */

#ifndef  __KIK_UTIL_H__
#define  __KIK_UTIL_H__


#define  K_MAX(n1,n2)  ((n1) > (n2) ? (n1) : (n2))

#define  K_MIN(n1,n2)  ((n1) > (n2) ? (n2) : (n1))

/*
 * char  : 0 - 256 (3)
 * int16 : 0 - 65536 (5)
 * int32 : 0 - 4294967296 (10)
 * int64 : 0 - 18446744073709551616 (20)
 * 
 * 40 is evenly selected in other cases just to be sure.
 */
#define  DIGIT_STR_LEN(n)  \
	((sizeof(n) == 1) ? 3 : \
	(sizeof(n) == 2) ? 5 : \
	(sizeof(n) == 4) ? 10 : \
	(sizeof(n) == 8) ? 20 : 40)


#endif
