/*
 *	$Id$
 */

#include  "mkf_iso2022_conv.h"

#include  "mkf_iso2022_intern.h"
#include  "mkf_zh_cn_map.h"
#include  "mkf_zh_tw_map.h"
#include  "mkf_ko_kr_map.h"
#include  "mkf_viet_map.h"
#include  "mkf_ru_map.h"
#include  "mkf_uk_map.h"
#include  "mkf_tg_map.h"
#include  "mkf_ucs4_map.h"


/* --- static functions --- */

static size_t
designate_to_g0(
	u_char *  dst ,
	size_t  dst_size ,
	int *  is_full ,
	mkf_charset_t  cs
	)
{
	*is_full = 0 ;
	
	if( IS_CS94SB(cs))
	{
		if( 3 > dst_size)
		{
			*is_full = 1 ;

			return  0 ;
		}

		*(dst ++) = '\x1b' ;
		*(dst ++) = '(' ;
		*(dst ++) = CS94SB_FT(cs) ;

		return  3 ;
	}
	else if( IS_CS94MB(cs))
	{
		if( 4 > dst_size)
		{
			*is_full = 1 ;

			return  0 ;
		}

		*(dst ++) = '\x1b' ;
		*(dst ++) = '$' ;
		*(dst ++) = '(' ;
		*(dst ++) = CS94MB_FT(cs) ;

		return  4 ;
	}
	else if( IS_CS96SB(cs))
	{
		if( 3 > dst_size)
		{
			*is_full = 1 ;

			return  0 ;
		}

		*(dst ++) = '\x1b' ;
		*(dst ++) = '-' ;
		*(dst ++) = CS96SB_FT(cs) ;

		return  3 ;
	}
	else if( IS_CS96MB(cs))
	{
		if( 4 > dst_size)
		{
			*is_full = 1 ;

			return  0 ;
		}

		*(dst ++) = '\x1b' ;
		*(dst ++) = '$' ;
		*(dst ++) = '-' ;
		*(dst ++) = CS96MB_FT(cs) ;

		return  4 ;
	}

	/* error */
	
	return  0 ;
}


/* --- global functions --- */

size_t
mkf_iso2022_illegal_char(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	int *  is_full ,
	mkf_char_t *  ch
	)
{
	mkf_iso2022_conv_t *  iso2022_conv ;
	size_t  filled_size ;
	size_t  size ;
	int  count ;

	iso2022_conv = (mkf_iso2022_conv_t*) conv ;

	*is_full = 0 ;

	if( ! IS_CS_BASED_ON_ISO2022(ch->cs))
	{
		/* error */
		
		return  0 ;
	}
	
	filled_size = 0 ;
	
	/*
	 * locking shift G0 to GL
	 */
	if( iso2022_conv->gl != &iso2022_conv->g0)
	{
		if( filled_size + 1 > dst_size)
		{
			*is_full = 1 ;
			
			return  0 ;
		}
		
		*(dst ++) = LS0 ;
		
		filled_size ++ ;
	}

	/*
	 * designating ch->cs to G0.
	 */
	 
	if( ( size = designate_to_g0( dst , dst_size - filled_size , is_full , ch->cs)) == 0)
	{
		return  0 ;
	}

	dst += size ;
	filled_size += size ;

	/*
	 * appending character bytes.
	 */
	 
	if( filled_size + ch->size > dst_size)
	{
		*is_full = 1 ;

		return  0 ;
	}

	if( IS_CS94SB(ch->cs) || IS_CS94MB(ch->cs))
	{
		for( count = 0 ; count < ch->size ; count++)
		{
			*(dst ++) = ch->ch[count] ;
		}
	}
	else if( IS_CS96SB(ch->cs) || IS_CS96MB(ch->cs))
	{
		for( count = 0 ; count < ch->size ; count++)
		{
			*(dst ++) = MAP_TO_GR(ch->ch[count]) ;
		}
	}
	else
	{
		/* error */
		
		return  0 ;
	}

	filled_size += ch->size ;

	/*
	 * restoring GL
	 */
	 
	if( iso2022_conv->gl == &iso2022_conv->g1)
	{
		if( filled_size + 1 > dst_size)
		{
			*is_full = 1 ;
			
			return  0 ;
		}
		
		*(dst ++) = LS1 ;

		filled_size ++ ;
	}
	else if( iso2022_conv->gl == &iso2022_conv->g2)
	{
		if( filled_size + 2 > dst_size)
		{
			*is_full = 1 ;
			
			return  0 ;
		}
		
		*(dst ++) = ESC ;
		*(dst ++) = LS2 ;

		filled_size += 2 ;
	}
	else if( iso2022_conv->gl == &iso2022_conv->g3)
	{
		if( filled_size + 2 > dst_size)
		{
			*is_full = 1 ;
			
			return  0 ;
		}
		
		*(dst ++) = ESC ;
		*(dst ++) = LS3 ;
		
		filled_size += 2 ;
	}

	/*
	 * restoring G0
	 */

	if( ( size = designate_to_g0( dst , dst_size - filled_size , is_full , iso2022_conv->g0)) == 0)
	{
		return  0 ;
	}

	return  filled_size + size ;
}

void
mkf_iso2022_remap_unsupported_charset(
	mkf_char_t *  ch
	)
{
	mkf_char_t   c ;
	
	if( IS_CS_BASED_ON_ISO2022(ch->cs))
	{
		/* do nothing */
	}
	else if( ch->cs == ISO10646_UCS4_1)
	{
		if( mkf_map_ucs4_to_iso2022cs( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( ch->cs == VISCII)
	{
		if( mkf_map_viscii_to_tcvn5712_3_1993( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( ch->cs == KOI8_R)
	{
		if( mkf_map_koi8_r_to_iso8859_5_r( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( ch->cs == KOI8_U)
	{
		if( mkf_map_koi8_u_to_iso8859_5_r( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( ch->cs == KOI8_T)
	{
		if( mkf_map_koi8_t_to_iso8859_5_r( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( ch->cs == GBK)
	{
		if( mkf_map_gbk_to_gb2312_80( &c , ch))
		{
			*ch = c ;
		}
	}
	else
	{
		if( ch->cs == JOHAB)
		{
			if( ! mkf_map_johab_to_uhc( &c , ch))
			{
				return ;
			}

			*ch = c ;
		}

		if( ch->cs == UHC)
		{
			if( mkf_map_uhc_to_ksc5601_1987( &c , ch))
			{
				*ch = c ;
			}

			return ;
		}

		/* HKSCS includes BIG5 */
		if( ch->cs == HKSCS)
		{
			ch->cs = BIG5 ;
		}

		if( ch->cs == BIG5)
		{
			if( mkf_map_big5_to_cns11643_1992( &c , ch))
			{
				*ch = c ;
			}

			return ;
		}
	}
}
