/*
 *	$Id$
 */

#ifndef  __MKF_UCS4_INTERN_H__
#define  __MKF_UCS4_INTERN_H__


#define  IS_ALPHABET(ucs4)  ( /* 0 <= (ucs4) && */ (ucs4) <= 0x33ff)

#define  IS_EXTENSION_A(ucs4)  (0x3400 <= (ucs4) && (ucs4) <= 0x4dff)

#define  IS_CJK(ucs4)  (0x4e00 <= (ucs4) && (ucs4) <= 0x9fff)

#define  IS_HANGUL(ucs4)  (0xac00 <= (ucs4) && (ucs4) <= 0xd7ff)

#define  IS_COMPAT(ucs4)  (0xf900 <= (ucs4) && (ucs4) <= 0xfffd)


#endif
