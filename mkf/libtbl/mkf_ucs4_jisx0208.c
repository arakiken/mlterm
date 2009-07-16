/*
 *	$Id$
 */

#include  "../lib/mkf_ucs4_jisx0208.h"

#include  "table/mkf_jisx0208_1983_to_ucs4.table"
#include  "table/mkf_jisx0208_nec_ext_to_ucs4.table"
#include  "table/mkf_jisx0208_necibm_ext_to_ucs4.table"
#include  "table/mkf_sjis_ibm_ext_to_ucs4.table"

#include  "table/mkf_ucs4_alphabet_to_jisx0208_1983.table"
#include  "table/mkf_ucs4_cjk_to_jisx0208_1983.table"
#include  "table/mkf_ucs4_compat_to_jisx0208_1983.table"


/* --- global functions --- */

int
mkf_map_jisx0208_1983_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  jis
	)
{
	u_int32_t  c ;

	if( ( c = CONV_JISX0208_1983_TO_UCS4(jis)))
	{
		mkf_int_to_bytes( ucs4->ch , 4 , c) ;
		ucs4->size = 4 ;
		ucs4->cs = ISO10646_UCS4_1 ;
		ucs4->property = 0 ;

		return  1 ;
	}

	return  0 ;
}

int
mkf_map_jisx0208_nec_ext_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  nec_ext
	)
{
	u_int32_t  c ;

	if( ( c = CONV_JISX0208_NEC_EXT_TO_UCS4(nec_ext)))
	{
		mkf_int_to_bytes( ucs4->ch , 4 , c) ;
		ucs4->size = 4 ;
		ucs4->cs = ISO10646_UCS4_1 ;
		ucs4->property = 0 ;

		return  1 ;
	}
	
	return  0 ;
}

int
mkf_map_jisx0208_necibm_ext_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  necibm_ext
	)
{
	u_int32_t  c ;

	if( ( c = CONV_JISX0208_NECIBM_EXT_TO_UCS4(necibm_ext)))
	{
		mkf_int_to_bytes( ucs4->ch , 4 , c) ;
		ucs4->size = 4 ;
		ucs4->cs = ISO10646_UCS4_1 ;
		ucs4->property = 0 ;

		return  1 ;
	}
	
	return  0 ;
}

int
mkf_map_sjis_ibm_ext_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  ibm_ext
	)
{
	u_int32_t  c ;

	if( ( c = CONV_SJIS_IBM_EXT_TO_UCS4(ibm_ext)))
	{
		mkf_int_to_bytes( ucs4->ch , 4 , c) ;
		ucs4->size = 4 ;
		ucs4->cs = ISO10646_UCS4_1 ;
		ucs4->property = 0 ;

		return  1 ;
	}
	
	return  0 ;
}

int
mkf_map_ucs4_to_jisx0208_1983(
	mkf_char_t *  non_ucs ,
	u_int32_t  ucs4_code
	)
{
	u_int16_t  c ;

	if( ( c = CONV_UCS4_ALPHABET_TO_JISX0208_1983(ucs4_code)) ||
		( c = CONV_UCS4_CJK_TO_JISX0208_1983(ucs4_code)) ||
		( c = CONV_UCS4_COMPAT_TO_JISX0208_1983(ucs4_code)))
	{
		mkf_int_to_bytes( non_ucs->ch , 2 , c) ;
		non_ucs->size = 2 ;
		non_ucs->cs = JISX0208_1983 ;
		non_ucs->property = 0 ;

		return  1 ;
	}

	return  0 ;
}

int
mkf_map_ucs4_to_jisx0208_nec_ext(
	mkf_char_t *  non_ucs ,
	u_int32_t  ucs4_code
	)
{
	u_int16_t  offset ;

	for( offset = 0 ; offset <= jisx0208_nec_ext_to_ucs4_end - jisx0208_nec_ext_to_ucs4_beg ; offset ++)
	{
		if( jisx0208_nec_ext_to_ucs4_table[offset] == (u_int16_t)ucs4_code)
		{
			mkf_int_to_bytes( non_ucs->ch , 2 , offset + jisx0208_nec_ext_to_ucs4_beg) ;
			non_ucs->cs = JISC6226_1978_NEC_EXT ;
			non_ucs->size = 2 ;
			non_ucs->property = 0 ;

			return  1 ;
		}
	}

	return  0 ;
}

int
mkf_map_ucs4_to_jisx0208_necibm_ext(
	mkf_char_t *  non_ucs ,
	u_int32_t  ucs4_code
	)
{
	u_int16_t  offset ;

	for( offset = 0 ; offset <= jisx0208_necibm_ext_to_ucs4_end - jisx0208_necibm_ext_to_ucs4_beg ;
		offset ++)
	{
		if( jisx0208_necibm_ext_to_ucs4_table[offset] == (u_int16_t)ucs4_code)
		{
			mkf_int_to_bytes( non_ucs->ch , 2 , offset + jisx0208_necibm_ext_to_ucs4_beg) ;
			non_ucs->cs = JISC6226_1978_NECIBM_EXT ;
			non_ucs->size = 2 ;
			non_ucs->property = 0 ;

			return  1 ;
		}
	}

	return  0 ;
}

int
mkf_map_ucs4_to_sjis_ibm_ext(
	mkf_char_t *  non_ucs ,
	u_int32_t  ucs4_code
	)
{
	u_int16_t  offset ;

	for( offset = 0 ; offset <= sjis_ibm_ext_to_ucs4_end - sjis_ibm_ext_to_ucs4_beg ; offset ++)
	{
		if( sjis_ibm_ext_to_ucs4_table[offset] == (u_int16_t)ucs4_code)
		{
			mkf_int_to_bytes( non_ucs->ch , 2 , offset + sjis_ibm_ext_to_ucs4_beg) ;
			non_ucs->cs = SJIS_IBM_EXT ;
			non_ucs->size = 2 ;
			non_ucs->property = 0 ;

			return  1 ;
		}
	}

	return  0 ;
}
