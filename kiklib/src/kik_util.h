/*
 *	$Id$
 */

#ifndef  __KIK_UTIL_H__
#define  __KIK_UTIL_H__


#if  defined(NetBSD) || defined(FreeBSD)
#include  <sys/param.h>	/* __FreeBSD_version, __NetBSD_Version */
#endif

#include  "kik_def.h"	/* WORDS_BIGENDIAN */
#include  "kik_types.h"	/* u_int32_t */


#if  ( defined(__NetBSD_Version__) && (__NetBSD_Version__ >= 300000000)) || \
     ( defined(__FreeBSD_version) && (__FreeBSD_version >= 501000))
#include  <sys/endian.h>
#define  BE32DEC(p)  be32dec(p)
#define  BE16DEC(p)  be16dec(p)
#define  LE32DEC(p)  le32dec(p)
#define  LE16DEC(p)  le16dec(p)
#else
/* p is unsigned char */
#define  BE32DEC(p)  (((u_int32_t)(p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | (p)[3])
#define  BE16DEC(p)  (((p)[0] << 8) | (p)[1])
#define  LE32DEC(p)  (((u_int32_t)(p)[3] << 24) | ((p)[2] << 16) | ((p)[1] << 8) | (p)[0])
#define  LE16DEC(p)  (((p)[1] << 8) | (p)[0])
#endif

/* p is "unsigned char *". __NO_STRICT_ALIGNMENT is defined in sys/types.h of *BSD */
#if  defined(__NO_STRICT_ALIGNMENT) || defined(__i386)
#define  TOINT32(p)  (*((u_int32_t*)(p)))
#define  TOINT16(p)  (*((u_int16_t*)(p)))
#elif  defined(WORDS_BIGENDIAN)
#define  TOINT32(p)  BE32DEC(p)
#define  TOINT16(p)  BE16DEC(p)
#else
#define  TOINT32(p)  LE32DEC(p)
#define  TOINT16(p)  LE16DEC(p)
#endif

#if  defined(WORDS_BIGENDIAN) && ! defined(__mc68000) && ! defined(__mips)
#define  BIT_MSBLEFT
#endif

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

#define  KIK_INT_TO_STR(i)  _KIK_INT_TO_STR(i)
#define  _KIK_INT_TO_STR(i) #i


#endif
