/*
 *	update: <2001/11/26(22:46:12)>
 *	$Id$
 */

#include  "ml_xcompound_text.h"

#include  <stdio.h>
#include  <string.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>	/* K_MIN */
#include  <mkf/mkf_charset.h>	/* mkf_charset_t */
#include  <mkf/mkf_char.h>
#include  <mkf/mkf_ucs4_map.h>
#include  <mkf/mkf_ja_jp_map.h>
#include  <mkf/mkf_zh_tw_map.h>
#include  <mkf/mkf_zh_cn_map.h>
#include  <mkf/mkf_ko_kr_map.h>
#include  <mkf/mkf_viet_map.h>
#include  <mkf/mkf_ru_map.h>

#include  "ml_locale.h"


typedef int (*map_func_t)( mkf_char_t *  , mkf_char_t *) ;

typedef struct  map_ucs4_to_func_table
{
	char *  locale ;
	map_func_t  func ;

} map_ucs4_to_func_table_t ;

/*
 * XXX
 * in the future , mkf_map_ucs4_to_[locale]_iso2022cs() may be prepared and used.
 */
static map_ucs4_to_func_table_t  map_ucs4_to_func_table[] =
{
	{ "ja" , mkf_map_ucs4_to_ja_jp } ,
	{ "ko" , mkf_map_ucs4_to_ko_kr } ,
	{ "ru" , mkf_map_ucs4_to_ru } ,
	{ "vi" , mkf_map_ucs4_to_viet } ,
	{ "zh_CN" , mkf_map_ucs4_to_zh_cn } ,
	{ "zh_TW" , mkf_map_ucs4_to_zh_tw } ,
} ;


/* --- static functions --- */

static map_func_t
get_map_ucs4_to_func_for_current_locale(void)
{
	int  counter ;
	char *  locale ;

	locale = ml_get_locale() ;

	for( counter = 0 ;
		counter < sizeof( map_ucs4_to_func_table) / sizeof( map_ucs4_to_func_table[0]) ;
		counter ++)
	{
		if( strncmp( map_ucs4_to_func_table[counter].locale , locale ,
				K_MIN( strlen( map_ucs4_to_func_table[counter].locale) ,
					strlen( locale))) == 0)
		{
			return  map_ucs4_to_func_table[counter].func ;
		}
	}

	return  NULL ;
}


/* --- global functions --- */

static int
convert_to_cs_based_on_iso2022(
	u_char *  bytes ,	/* should be at least 4 bytes */
	size_t  len ,
	mkf_charset_t *  cs
	)
{
	mkf_char_t  iso2022_ch ;
	mkf_char_t  ch ;

	/*
	 * UCS -> ANY OTHER CS
	 */
	if( *cs == ISO10646_UCS2_1)
	{
		ch.ch[0] = 0x0 ;
		ch.ch[1] = 0x0 ;
		ch.ch[2] = bytes[0] ;
		ch.ch[3] = bytes[1] ;
		ch.cs = ISO10646_UCS4_1 ;
		ch.property = 0 ;
		ch.size = 4 ;
	}
	else
	{
		memcpy( ch.ch , bytes , len) ;
		ch.size = len ;
		ch.property = 0 ;
		ch.cs = *cs ;
	}
	
	if( ch.cs == ISO10646_UCS4_1)
	{
		map_func_t  func ;

		if( ( func = get_map_ucs4_to_func_for_current_locale()) == NULL ||
			(*func)( &iso2022_ch , &ch) == 0)
		{
			if( mkf_map_ucs4_to_iso2022cs( &iso2022_ch , &ch) == 0)
			{
				return  0 ;
			}
		}
		
		ch = iso2022_ch ;
	}

	/*
	 * NON ISO2022 CS -> ISO2022 CS
	 */
	if( *cs == BIG5)
	{
		if( mkf_map_big5_to_cns11643_1992( &iso2022_ch , &ch) == 0)
		{
			return  0 ;
		}
	}
	else if( ch.cs == UHC)
	{
		if( mkf_map_uhc_to_ksc5601_1987( &iso2022_ch , &ch) == 0)
		{
			return  0 ;
		}
	}
	else if( ch.cs == GBK)
	{
		if( mkf_map_gbk_to_gb2312_80( &iso2022_ch , &ch) == 0)
		{
			return  0 ;
		}
	}
	else if( ch.cs == VISCII)
	{
		if( mkf_map_viscii_to_tcvn5712_3_1993( &iso2022_ch , &ch) == 0)
		{
			return  0 ;
		}
	}
	else if( ch.cs == KOI8_R)
	{
		if( mkf_map_koi8_r_to_iso8859_5_r( &iso2022_ch , &ch) == 0)
		{
			return  0 ;
		}
	}
	else if( ch.cs == KOI8_U)
	{
		if( mkf_map_koi8_u_to_iso8859_5_r( &iso2022_ch , &ch) == 0)
		{
			return  0 ;
		}
	}
	else if( IS_CS_BASED_ON_ISO2022(ch.cs))
	{
		iso2022_ch = ch ;
	}
	else
	{
		/* a charset not based on ISO2022 */
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " charset %x is not based on iso2022.\n" , ch.cs) ;
	#endif
	
		return  0 ;
	}
	
	memcpy( bytes , iso2022_ch.ch , iso2022_ch.size) ;
	*cs = iso2022_ch.cs ;

	return  iso2022_ch.size ;
}

static size_t
convert_to_xct(
	u_char *  xct ,
	size_t  xct_len ,
	ml_char_t *  mlchars ,
	u_int  mlchars_len ,
	mkf_charset_t *  gl_cs ,
	mkf_charset_t *  gr_cs
	)
{
	int  mlch_counter ;
	int  xct_counter ;
	ml_char_t *  comb_chars ;
	u_int  comb_size ;

	xct_counter = 0 ;
	for( mlch_counter = 0 ; mlch_counter < mlchars_len ; mlch_counter ++)
	{
		u_char  bytes[4] ;
		size_t  byte_len ;
		mkf_charset_t  next_cs ;

		byte_len = ml_char_size( &mlchars[mlch_counter]) ;
		memcpy( bytes , ml_char_bytes( &mlchars[mlch_counter]) , byte_len) ;
		next_cs = ml_char_cs( &mlchars[mlch_counter]) ;

		if( ( byte_len = convert_to_cs_based_on_iso2022( bytes , byte_len , &next_cs)) == 0)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " convert_to_cs_based_on_iso2022() failed.\n") ;
		#endif
		
			continue ;
		}

		if( IS_CS94MB(next_cs) || IS_CS94SB(next_cs))
		{
			/*
			 * is GL charset.
			 */
			 
			if( *gl_cs != next_cs)
			{
				if( IS_CS94MB(next_cs))
				{
					/* ESC - $ - Ft is not allowed */

					if( xct_counter + 4 > xct_len)
					{
					#ifdef  DEBUG
						kik_warn_printf( KIK_DEBUG_TAG " shortage of buffer !\n") ;
					#endif

						return  xct_counter ;
					}

					xct[xct_counter++] = '\x1b' ;
					xct[xct_counter++] = '$' ;
					xct[xct_counter++] = '(' ;
					xct[xct_counter++] = CS94MB_FT(next_cs) ;
				}
				else /* if( IS_CS94SB(next_cs)) */
				{
					if( xct_counter + 3 > xct_len)
					{
					#ifdef  DEBUG
						kik_warn_printf( KIK_DEBUG_TAG " shortage of buffer !\n") ;
					#endif

						return  xct_counter ;
					}

					xct[xct_counter++] = '\x1b' ;
					xct[xct_counter++] = '(' ;
					xct[xct_counter++] = CS94SB_FT(next_cs) ;
				}
				
				*gl_cs = next_cs ;
			}
		}
		else if( IS_CS96MB(next_cs) || IS_CS96SB(next_cs))
		{
			/*
			 * is GR charset.
			 */
			 
			int  counter ;
			
			if( *gr_cs != next_cs)
			{
				if( IS_CS96MB(next_cs))
				{
					if( xct_counter + 4 > xct_len)
					{
					#ifdef  DEBUG
						kik_warn_printf( KIK_DEBUG_TAG " shortage of buffer !\n") ;
					#endif

						return  xct_counter ;
					}

					xct[xct_counter++] = '\x1b' ;
					xct[xct_counter++] = '$' ;
					xct[xct_counter++] = '-' ;
					xct[xct_counter++] = CS96MB_FT(next_cs) ;
				}
				else /* if( IS_CS96SB(next_cs)) */
				{
					if( xct_counter + 3 > xct_len)
					{
					#ifdef  DEBUG
						kik_warn_printf( KIK_DEBUG_TAG " shortage of buffer !\n") ;
					#endif

						return  xct_counter ;
					}

					xct[xct_counter++] = '\x1b' ;
					xct[xct_counter++] = '-' ;
					xct[xct_counter++] = CS96SB_FT(next_cs) ;
				}
				
				*gr_cs = next_cs ;
			}
			
			for( counter = 0 ; counter < byte_len ; counter ++)
			{
				SET_MSB(bytes[counter]) ;
			}
		}
		else
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG , " not iso2022 based charset , ignored.\n") ;
		#endif

			continue ;
		}

		if( xct_counter + byte_len > xct_len)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " shortage of buffer !\n") ;
		#endif

			return  xct_counter ;
		}

		memcpy( &xct[xct_counter] , bytes , byte_len) ;
		xct_counter += byte_len ;

		if( ( comb_chars = ml_get_combining_chars( &mlchars[mlch_counter] , &comb_size)))
		{
			xct_counter += convert_to_xct( &xct[xct_counter] , xct_len - xct_counter ,
				comb_chars , comb_size , gl_cs , gr_cs) ;
		}
	}

	return  xct_counter ;
}


/* --- global functions --- */

size_t
ml_convert_to_xct(
	u_char *  xct ,
	size_t  xct_len ,
	ml_char_t *  str ,
	u_int  str_len
	)
{
	mkf_charset_t  gl_cs ;
	mkf_charset_t  gr_cs ;

	gl_cs = US_ASCII ;
	gr_cs = ISO8859_1_R ;
	
	return  convert_to_xct( xct , xct_len , str , str_len , &gl_cs , &gr_cs) ;
}
