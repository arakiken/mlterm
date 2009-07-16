/*
 *	$Id$
 */

#include  "mkf_ucs4_johab.h"


/* --- static functions --- */

/* 32 = 2^5 */

static int8_t  johab_first_to_linear[32] =
{
	0 , 0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 , 10 , 11 , 12 , 13 , 14 , 15 , 16 ,
	17 , 18 , 19 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0
} ;

static int8_t  johab_middle_to_linear[32] =
{
	0 , 0 , 0 , 1 , 2 , 3 , 4 , 5 , 0 , 0 , 6 , 7 , 8 , 9 , 10 , 11 ,
	0 , 0 , 12 , 13 , 14 , 15 , 16 , 17 , 0 , 0 , 18 , 19 , 20 , 21 , 0 , 0
} ;

static int8_t  johab_last_to_linear[32] =
{
	0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 , 10 , 11 , 12 , 13 , 14 , 15 ,
	16 , 17 , 0 , 18 , 19 , 20 , 21 , 22 , 23 , 24 , 25 , 26 , 27 , 28 , 0 , 0
} ;

static int8_t  linear_to_johab_first[32] =
{
	2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 , 10 , 11 , 12 , 13 , 14 , 15 , 16 , 17 ,
	18 , 19 , 20 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0
} ;

static int8_t  linear_to_johab_middle[32] =
{
	3 , 4 , 5 , 6 , 7 , 10 , 11 , 12 , 13 , 14 , 15 , 18 , 19 , 20 , 21 , 22 ,
	23 , 26 , 27 , 28 , 29 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0
} ;

static int8_t  linear_to_johab_last[32] =
{
	1 , 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 , 10 , 11 , 12 , 13 , 14 , 15 , 16 ,
	17 , 19 , 20 , 21 , 22 , 23 , 24 , 25 , 26 , 27 , 28 , 29 , 0 , 0 , 0 , 0
} ;


/* --- global functions --- */

int
mkf_map_johab_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  johab
	)
{
	int  first_linear ;
	int  middle_linear ;
	int  last_linear ;
	u_int16_t  johab_linear ;

	first_linear = johab_first_to_linear[(johab >> 10) & 0x1f] ;
	middle_linear = johab_middle_to_linear[(johab >> 5) & 0x1f] ;
	last_linear = johab_last_to_linear[johab & 0x1f] ;

	if( first_linear == 0 || middle_linear == 0 || last_linear == 0)
	{
		/* illegal johab format */
		
		return  0 ;
	}
	
	johab_linear = ((first_linear - 1) * 21 + (middle_linear - 1)) * 28 + (last_linear - 1) ;

	mkf_int_to_bytes( ucs4->ch , 4 , johab_linear + 0xac00) ;
	ucs4->size = 4 ;
	ucs4->cs = ISO10646_UCS4_1 ;
	ucs4->property = 0 ;

	return  1 ;
}

int
mkf_map_ucs4_to_johab(
	mkf_char_t *  johab ,
	u_int32_t  ucs4_code
	)
{
	int  first ;
	int  middle ;
	int  last ;
	u_int32_t  linear ;
	u_int16_t  johab_code ;

	if( ucs4_code < 0xac00 || 0xd7a3 < ucs4_code)
	{
		/* not hangul */
		
		return  0 ;
	}
	
	linear = ucs4_code - 0xac00 ;

	first = linear_to_johab_first[(linear / 28) / 21] ;
	middle = linear_to_johab_middle[(linear / 28) % 21] ;
	last = linear_to_johab_last[linear % 28] ;

	johab_code = 0x8000 + (first << 10) + (middle << 5) + last ;
	
	mkf_int_to_bytes( johab->ch , 2 , johab_code) ;
	johab->size = 2 ;
	johab->cs = JOHAB ;
	johab->property = 0 ;

	return  1 ;
}
