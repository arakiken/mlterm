/*
 *	$Id$
 */

#include  "mkf_iso2022_conv.h"
#include  "mkf_xct_conv.h"

#include  <string.h>		/* strncmp */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_locale.h>
#include  <kiklib/kik_util.h>	/* K_MIN */

#include  "mkf_ucs4_map.h"
#include  "mkf_ucs4_map.h"
#include  "mkf_ja_jp_map.h"
#include  "mkf_zh_tw_map.h"
#include  "mkf_zh_cn_map.h"
#include  "mkf_ko_kr_map.h"
#include  "mkf_viet_map.h"
#include  "mkf_ru_map.h"
#include  "mkf_iso2022_intern.h"


typedef struct  mkf_iso2022_conv
{
	mkf_conv_t   conv ;
	mkf_charset_t  gl ;
	mkf_charset_t  gr ;
	
} mkf_iso2022_conv_t ;

typedef int (*map_func_t)( mkf_char_t *  , mkf_char_t *) ;

typedef struct  map_ucs4_to_func_table
{
	char *  locale ;
	map_func_t  func ;

} map_ucs4_to_func_table_t ;


/* --- static variables --- */

/*
 * XXX
 * in the future , mkf_map_ucs4_to_[locale]_iso2022cs() should be implemented.
 *
 * XXX
 * other languages(especially ISO8859 variants) are not supported yet.
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

	locale = kik_get_locale() ;

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

static void
remap_unsupported_charset(
	mkf_char_t *  ch
	)
{
	mkf_char_t  c ;

	/*
	 * UCS => Any Other CS
	 */
	 
	if( ch->cs == ISO10646_UCS2_1)
	{
		ch->ch[2] = ch->ch[0] ;
		ch->ch[3] = ch->ch[1] ;
		ch->ch[0] = 0x0 ;
		ch->ch[1] = 0x0 ;
		ch->cs = ISO10646_UCS4_1 ;
		ch->size = 4 ;
	}
	
	if( ch->cs == ISO10646_UCS4_1)
	{
		map_func_t  func ;

		/* UCS => other cs conforming to current locale. */
		if( ( func = get_map_ucs4_to_func_for_current_locale()) == NULL ||
			(*func)( &c , ch) == 0)
		{
			if( mkf_map_ucs4_to_iso2022cs( &c , ch) == 0)
			{
				return ;
			}
		}
		
		*ch = c ;
	}

	/*
	 * NON ISO2022 CS -> ISO2022 CS
	 */
	 
	if( ch->cs == UHC)
	{
		if( mkf_map_uhc_to_ksc5601_1987( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( ch->cs == GB2312_80 && strcasecmp( kik_get_codeset() , "GBK") == 0)
	{
		if( mkf_map_gb2312_80_to_gbk( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( ch->cs == GBK && strcasecmp( kik_get_codeset() , "GBK") != 0)
	{
		if( mkf_map_gbk_to_gb2312_80( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( ch->cs == CNS11643_1992_1 && strcasecmp( kik_get_codeset() , "BIG5") == 0)
	{
		if( mkf_map_cns11643_1992_1_to_big5( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( ch->cs == CNS11643_1992_2 && strcasecmp( kik_get_codeset() , "BIG5") == 0)
	{
		if( mkf_map_cns11643_1992_2_to_big5( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( ch->cs == BIG5 && strcasecmp( kik_get_codeset() , "BIG5") != 0)
	{
		if( mkf_map_big5_to_cns11643_1992( &c , ch))
		{
			*ch = c ;
		}
	}
	
	/*
	 * XXX
	 * KOI8-R , KOI8-U ...
	 */
}

static size_t
convert_to_iso2022_intern(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser ,
	int  is_xct ,
	int  big5_buggy
	)
{
	size_t  filled_size ;
	mkf_char_t  ch ;
	mkf_iso2022_conv_t *  iso2022_conv ;

	iso2022_conv = (mkf_iso2022_conv_t*)conv ;

	filled_size = 0 ;
	while( 1)
	{
		int  counter ;

		mkf_parser_mark( parser) ;

		if( ! (*parser->next_char)( parser , &ch))
		{
			if( parser->is_eos)
			{
			#ifdef  __DEBUG
				kik_debug_printf( KIK_DEBUG_TAG
					" parser reached the end of string.\n") ;
			#endif
			
				return  filled_size ;
			}
			else
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" parser->next_char() returns error , but the process is continuing...\n") ;
			#endif

				/*
				 * passing unrecognized byte...
				 */
				if( mkf_parser_increment( parser) == 0)
				{
					return  filled_size ;
				}

				continue ;
			}
		}

		if( is_xct)
		{
			remap_unsupported_charset( &ch) ;
		}

		if( IS_CS94SB(ch.cs) || IS_CS94MB(ch.cs))
		{
			if( ch.cs != iso2022_conv->gl)
			{
				if( IS_CS94SB(ch.cs))
				{
					if( filled_size + ch.size + 2 >= dst_size)
					{
						mkf_parser_reset( parser) ;

						return  filled_size ;
					}

					*(dst ++) = '\x1b' ;
					*(dst ++) = '(' ;
					*(dst ++) = CS94SB_FT(ch.cs) ;

					filled_size += 3 ;
				}
				else
				{
					if( filled_size + ch.size + 3 >= dst_size)
					{
						mkf_parser_reset( parser) ;

						return  filled_size ;
					}

					*(dst ++) = '\x1b' ;
					*(dst ++) = '$' ;
					*(dst ++) = '(' ;
					*(dst ++) = CS94MB_FT(ch.cs) ;

					filled_size += 4 ;
				}
				
				iso2022_conv->gl = ch.cs ;
			}
			else
			{
				if( filled_size + ch.size - 1 >= dst_size)
				{
					mkf_parser_reset( parser) ;
			
					return  filled_size ;
				}
			}
			
			for( counter = 0 ; counter < ch.size ; counter++)
			{
				*(dst ++) = ch.ch[counter] ;
			}

			filled_size += ch.size ;
		}
		else if( IS_CS96SB(ch.cs) || IS_CS96MB(ch.cs))
		{
			if( ch.cs != iso2022_conv->gr)
			{
				if( IS_CS96SB(ch.cs))
				{
					if( filled_size + ch.size + 2 >= dst_size)
					{
						mkf_parser_reset( parser) ;

						return  filled_size ;
					}

					*(dst ++) = '\x1b' ;
					*(dst ++) = '-' ;
					*(dst ++) = CS96SB_FT(ch.cs) ;

					filled_size += 3 ;
				}
				else
				{
					if( filled_size + ch.size + 3 >= dst_size)
					{
						mkf_parser_reset( parser) ;

						return  filled_size ;
					}

					*(dst ++) = '\x1b' ;
					*(dst ++) = '$' ;
					*(dst ++) = '-' ;
					*(dst ++) = CS96MB_FT(ch.cs) ;

					filled_size += 4 ;
				}
				
				iso2022_conv->gr = ch.cs ;
			}
			else
			{
				if( filled_size + ch.size - 1 >= dst_size)
				{
					mkf_parser_reset( parser) ;
			
					return  filled_size ;
				}
			}
			
			for( counter = 0 ; counter < ch.size ; counter++)
			{
				*(dst ++) = MAP_TO_GR(ch.ch[counter]) ;
			}

			filled_size += ch.size ;
		}
		else if( is_xct && (ch.cs == BIG5 || ch.cs == GBK))
		{
			char *  prefix ;
			
			if( ch.cs == BIG5)
			{
				/*
				 * XXX
				 * Big5 CTEXT implementation of XFree86 4.1.0 or before is very BUGGY!
				 */
				if( big5_buggy)
				{
					prefix = "\x1b\x25\x2f\x32\x80\x89" "BIG5-0"
						"\x02\x80\x89" "BIG5-0" "\x02" ;

					iso2022_conv->gl = BIG5 ;
					iso2022_conv->gr = BIG5 ;
				}
				else
				{
					prefix = "\x1b\x25\x2f\x32\x80\x89" "big5-0" "\x02" ;
				}
			}
			else
			{
				prefix = "\x1b\x25\x2f\x32\x80\x88" "gbk-0" "\x02" ;
			}
			
			if( filled_size + strlen( prefix) + ch.size > dst_size)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}

			memcpy( dst , prefix , strlen( prefix)) ;
			dst += strlen( prefix) ;
			
			*(dst ++) = ch.ch[0] ;
			*(dst ++) = ch.ch[1] ;

			filled_size += ( strlen( prefix) + 2) ;
		}
		else if( is_xct && (ch.cs == KOI8_R || ch.cs == KOI8_U || ch.cs == VISCII))
		{
			char *  prefix ;
			
			if( ch.cs == KOI8_R)
			{
				prefix = "\x1b\x25\x2f\x31\x80\x88" "koi8-r" "\x02" ;
			}
			else if( ch.cs == KOI8_U)
			{
				prefix = "\x1b\x25\x2f\x31\x80\x88" "koi8-u" "\x02" ;
			}
			else /* if( ch.cs == VISCII) */
			{
				prefix = "\x1b\x25\x2f\x31\x80\x8d" "viscii1.1-1" "\x02" ;
			}
			
			if( filled_size + strlen( prefix) + ch.size > dst_size)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}

			memcpy( dst , prefix , strlen( prefix)) ;
			dst += strlen( prefix) ;
			
			*(dst ++) = ch.ch[0] ;

			filled_size += ( strlen( prefix) + 1) ;
		}
		else
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" cs(%x) is not supported by iso2022. char(%x) is discarded.\n" ,
				ch.cs , mkf_char_to_int( &ch)) ;
		#endif

			if( filled_size >= dst_size)
			{
				mkf_parser_reset( parser) ;
			
				return  filled_size ;
			}
			
			*(dst ++) = ' ' ;
			filled_size ++ ;
		}		
	}
}

static size_t
convert_to_iso2022(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso2022_intern( conv , dst , dst_size , parser , 0 , 0) ;
}

static size_t
convert_to_xct(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso2022_intern( conv , dst , dst_size , parser , 1 , 0) ;
}

static size_t
convert_to_xct_big5_buggy(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso2022_intern( conv , dst , dst_size , parser , 1 , 1) ;
}

static void
iso2022_conv_init(
	mkf_conv_t *  conv
	)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	iso2022_conv = (mkf_iso2022_conv_t*) conv ;
	
	/* GL = G0 / GR = G1 is implicitly invoked , and G0 / G1 is not still set */
	iso2022_conv->gl = UNKNOWN_CS ;
	iso2022_conv->gr = UNKNOWN_CS ;
}

static void
xct_conv_init(
	mkf_conv_t *  conv
	)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	iso2022_conv = (mkf_iso2022_conv_t*) conv ;
	
	/*
	 * GL = G0 / GR = G1 is implicitly invoked and
	 * GL = US_ASCII / GR = ISO8859_1_R is implicitely designated.
	 */
	iso2022_conv->gl = US_ASCII ;
	iso2022_conv->gr = ISO8859_1_R ;
}

static void
conv_delete(
	mkf_conv_t *  conv
	)
{
	free( conv) ;
}


/* --- global functions --- */

/*
 * 8 bit ISO2022
 */
mkf_conv_t *
mkf_iso2022_conv_new(void)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	if( ( iso2022_conv = malloc( sizeof( mkf_iso2022_conv_t))) == NULL)
	{
		return  NULL ;
	}

	iso2022_conv_init( ( mkf_conv_t*) iso2022_conv) ;

	iso2022_conv->conv.convert = convert_to_iso2022 ;
	iso2022_conv->conv.init = iso2022_conv_init ;
	iso2022_conv->conv.delete = conv_delete ;

	return  (mkf_conv_t*)iso2022_conv ;
}

mkf_conv_t *
mkf_xct_conv_new(void)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	if( ( iso2022_conv = malloc( sizeof( mkf_iso2022_conv_t))) == NULL)
	{
		return  NULL ;
	}

	xct_conv_init( ( mkf_conv_t*) iso2022_conv) ;
	
	iso2022_conv->conv.convert = convert_to_xct ;
	iso2022_conv->conv.init = iso2022_conv_init ;
	iso2022_conv->conv.delete = conv_delete ;

	return  (mkf_conv_t*)iso2022_conv ;
}

mkf_conv_t *
mkf_xct_big5_buggy_conv_new(void)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	if( ( iso2022_conv = malloc( sizeof( mkf_iso2022_conv_t))) == NULL)
	{
		return  NULL ;
	}

	xct_conv_init( ( mkf_conv_t*) iso2022_conv) ;
	
	iso2022_conv->conv.convert = convert_to_xct_big5_buggy ;
	iso2022_conv->conv.init = iso2022_conv_init ;
	iso2022_conv->conv.delete = conv_delete ;

	return  (mkf_conv_t*)iso2022_conv ;
}
