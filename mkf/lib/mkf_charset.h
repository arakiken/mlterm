/*
 *	$Id$
 */

#ifndef  __MKF_CHARSET_H__
#define  __MKF_CHARSET_H__


#include  <kiklib/kik_types.h>		/* u_xxx */


/* ISO2022 Ft should be within 0x30 and 0x7e */

/* 0x00 - 0x4e */
#define  CS94SB_ID(c)  ( (u_char)(c) - 0x30)
/* 0x50 - 0x9e */
#define  CS96SB_ID(c)  ( (u_char)(c) + 0x20)
/* 0xa0 - 0xde */
#define  CS94MB_ID(c)  ( (u_char)(c) + 0x70)
/* 0xf0 - 0x13e */
#define  CS96MB_ID(c)  ( (u_char)(c) + 0xc0)

/* 0x140 - 0x18e */
#define  NON_ISO2022_1_ID(c)  ( (u_char)(c) + 0x110)
/* 0x190 - 0x1de */
#define  NON_ISO2022_2_ID(c)  ( (u_char)(c) + 0x160)

/* 0x400 - 0x53e (= 0x400 | CS9XXB_ID) */
#define  CS_REVISION_1(cs)	( (cs) + 0x400)
/* 0x600 - 0x73e (= 0x600 | CS9XXB_ID) */
#define  CS_REVISION_2(cs)	( (cs) + 0x600)

#define  CS94SB_FT(i)  ( (int)(i) + 0x30)
#define  CS96SB_FT(i)  ( (int)(i) - 0x20)
#define  CS94MB_FT(i)  ( (int)(i) - 0x70)
#define  CS96MB_FT(i)  ( (int)(i) - 0xc0)

/*
 * 'and 0x3ff' should be done because 0x400 - region is used for 'or cs_revision'
 */
 
#define  IS_CS94SB(cs)  (0x00 <= ((cs) & 0x3ff) && ((cs) & 0x3ff) <= 0x4e)
#define  IS_CS96SB(cs)  (0x50 <= ((cs) & 0x3ff) && ((cs) & 0x3ff) <= 0x9e)
#define  IS_CS94MB(cs)  (0xa0 <= ((cs) & 0x3ff) && ((cs) & 0x3ff) <= 0xde)
#define  IS_CS96MB(cs)  (0xf0 <= ((cs) & 0x3ff) && ((cs) & 0x3ff) <= 0x13e)

#define  IS_NON_ISO2022(cs)  (0x140 <= ((cs) & 0x3ff) && ((cs) & 0x3ff) <= 0x1de)

#define  IS_CS_BASED_ON_ISO2022(cs)  ( 0x0 <= ((cs) & 0x3ff) && ((cs) & 0x3ff) <= 0x13e)


/*
 * these enumeration numbers are based on iso2022 Ft(0x30-0x7e).
 * total range is -1 <-> 0x7fff(int16)
 */
typedef enum  mkf_charset
{
	UNKNOWN_CS = -1 ,
	
	/* 94 sb cs */
	DEC_SPECIAL = CS94SB_ID('0') ,
	ISO646_IRV = CS94SB_ID('@') ,
	ISO646_EN = CS94SB_ID('A') ,
	US_ASCII = CS94SB_ID('B') ,
	NATS_PRIMARY_FOR_FIN_SWEDEN = CS94SB_ID('C') ,
	NATS_PRIMARY_FOR_DEN_NOR = CS94SB_ID('E') ,
	ISO646_SWEDEN = CS94SB_ID('G') ,
	ISO646_SWEDEN_NAME = CS94SB_ID('H') ,
	JISX0201_KATA = CS94SB_ID('I') ,
	JISX0201_ROMAN = CS94SB_ID('J') ,

	/* 96 sb cs */
	ISO8859_1_R = CS96SB_ID('A') ,
	ISO8859_2_R = CS96SB_ID('B') ,
	ISO8859_3_R = CS96SB_ID('C') ,
	ISO8859_4_R = CS96SB_ID('D') ,
	ISO8859_7_R = CS96SB_ID('F') ,
	ISO8859_6_R = CS96SB_ID('G') ,
	ISO8859_8_R = CS96SB_ID('H') ,
	ISO8859_5_R = CS96SB_ID('L') ,
	ISO8859_9_R = CS96SB_ID('M') ,
	ISO8859_10_R = CS96SB_ID('V') ,
	TIS620_2533 = CS96SB_ID('T') ,
	ISO8859_13_R = CS96SB_ID('Y') ,	/* Ft = 5/9 */
	ISO8859_14_R = CS96SB_ID('_') ,	/* Ft = 5/15 */
	ISO8859_15_R = CS96SB_ID('b') ,	/* Ft = 6/2 */
	ISO8859_16_R = CS96SB_ID('f') ,	/* Ft = 6/6 */
	TCVN5712_3_1993 = CS96SB_ID('Z') ,

	/* 94 mb cs */
	JISC6226_1978 = CS94MB_ID('@') ,
	GB2312_80 = CS94MB_ID('A') ,
	JISX0208_1983 = CS94MB_ID('B') ,
	KSC5601_1987 = CS94MB_ID('C') ,
	JISX0212_1990 = CS94MB_ID('D') ,
	CNS11643_1992_1 = CS94MB_ID('G') ,
	CNS11643_1992_2 = CS94MB_ID('H') ,
	CNS11643_1992_3 = CS94MB_ID('I') ,
	CNS11643_1992_4 = CS94MB_ID('J') ,
	CNS11643_1992_5 = CS94MB_ID('K') ,
	CNS11643_1992_6 = CS94MB_ID('L') ,
	CNS11643_1992_7 = CS94MB_ID('M') ,
	JISX0213_2000_1 = CS94MB_ID('O') ,
	JISX0213_2000_2 = CS94MB_ID('P') ,

	/* 96 mb cs */

	/* NOT ISO2022 - class 1 (ESC 2/5 Ft) */
	UTF1 = NON_ISO2022_1_ID('B') ,
	UTF8 = NON_ISO2022_1_ID('G') ,

	/* NOT ISO2022 - class 2 (ESC 2/5 2/15 Ft) */
	XCT_NON_ISO2022_CS_1 = NON_ISO2022_2_ID('1') ,	/* CTEXT */
	XCT_NON_ISO2022_CS_2 = NON_ISO2022_2_ID('2') ,	/* CTEXT */
	ISO10646_UCS2_1 = NON_ISO2022_2_ID('@') ,
	ISO10646_UCS4_1 = NON_ISO2022_2_ID('A') ,

	/* followings are mkf original classifications */
	
	/*
	 * those who are not ISO2022 registed characterset but confirm to ISO2022.
	 * 0x200 - 0x2ff
	 */
	JISC6226_1978_NEC_EXT = 0x200 ,
	JISC6226_1978_NECIBM_EXT = 0x201 ,
	JISX0208_1983_MAC_EXT = 0x202 ,

	/*
	 * those who are not ISO2022 registed characterset and do not confirm to ISO2022.
	 * 0x300 - 0x3ff
	 */
	SJIS_IBM_EXT = 0x300 ,
	UHC = 0x301 ,
	BIG5 = 0x302 ,
	CNS11643_1992_EUCTW_G2 = 0x303 ,
	GBK = 0x304 ,
	JOHAB = 0x305 ,
	VISCII = 0x306 ,
	TCVN5712_1_1993 = 0x307 ,
	KOI8_R = 0x308 ,
	KOI8_U = 0x309 ,
	HKSCS = 0x310 ,

	/* followings are ISO2022 charsets of rev 1 */
	JISX0208_1990 = CS_REVISION_1( CS94MB_ID('B')) ,
	
	MAX_CHARSET = 0x7ff ,
	
} mkf_charset_t ;


#endif
