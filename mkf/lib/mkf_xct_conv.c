/*
 *	$Id$
 */

#include  "mkf_xct_conv.h"

#include  <string.h>		/* strcasecmp */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_locale.h>	/* kik_get_codeset() */
#include  <kiklib/kik_debug.h>

#include  "mkf_locale_ucs4_map.h"
#include  "mkf_ucs4_map.h"
#include  "mkf_zh_tw_map.h"
#include  "mkf_zh_cn_map.h"
#include  "mkf_iso2022_conv.h"
#include  "mkf_iso2022_intern.h"


/* --- static functions --- */

static void
remap_unsupported_charset(
	mkf_char_t *  ch
	)
{
	mkf_char_t  c ;

	if( ch->cs == ISO10646_UCS4_1)
	{
		if( ! mkf_map_locale_ucs4_to( &c , ch))
		{
			return ;
		}
		
		*ch = c ;
	}

	if( strcasecmp( kik_get_codeset() , "GBK") == 0)
	{
		if( ch->cs == GB2312_80)
		{
			if( mkf_map_gb2312_80_to_gbk( &c , ch))
			{
				*ch = c ;
			}

			return ;
		}
		else if( ch->cs == GBK)
		{
			return ;
		}
	}
	else
	{
		if( ch->cs == GBK)
		{
			if( mkf_map_gbk_to_gb2312_80( &c , ch))
			{
				*ch = c ;
			}

			return ;
		}
		else if( ch->cs == GB2312_80)
		{
			return ;
		}
	}
	
	if( strcasecmp( kik_get_codeset() , "BIG5") == 0 ||
			strcasecmp( kik_get_codeset() , "BIG5HKSCS") == 0)
	{
		if( ch->cs == CNS11643_1992_1)
		{
			if( mkf_map_cns11643_1992_1_to_big5( &c , ch))
			{
				*ch = c ;
			}

			return ;
		}
		else if( ch->cs == CNS11643_1992_2)
		{
			if( mkf_map_cns11643_1992_2_to_big5( &c , ch))
			{
				*ch = c ;
			}

			return ;
		}
		else if( ch->cs == BIG5)
		{
			return ;
		}
	}
	else
	{
		if( ch->cs == BIG5)
		{
			if( mkf_map_big5_to_cns11643_1992( &c , ch))
			{
				*ch = c ;
			}

			return ;
		}
		else if( ch->cs == CNS11643_1992_1 || ch->cs == CNS11643_1992_2)
		{
			return ;
		}
	}
	
	mkf_iso2022_remap_unsupported_charset( ch) ;
}

static size_t
convert_to_xct_intern(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser ,
	int  big5_buggy
	)
{
	size_t  filled_size ;
	mkf_char_t  ch ;
	mkf_iso2022_conv_t *  iso2022_conv ;

	iso2022_conv = (mkf_iso2022_conv_t*)conv ;

	filled_size = 0 ;
	while( mkf_parser_next_char( parser , &ch))
	{
		int  counter ;

		remap_unsupported_charset( &ch) ;

		if( IS_CS94SB(ch.cs) || IS_CS94MB(ch.cs))
		{
			if( ch.cs != iso2022_conv->g0)
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
				
				iso2022_conv->g0 = ch.cs ;
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
			if( ch.cs != iso2022_conv->g1)
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
				
				iso2022_conv->g1 = ch.cs ;
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
		/*
		 * Non-Standard Character Sets
		 */
		else if( ch.cs == BIG5 || ch.cs == HKSCS || ch.cs == GBK)
		{
			char *  prefix ;
			
			if( ch.cs == BIG5 || ch.cs == HKSCS)
			{
				/*
				 * !! Notice !!
				 * Big5 CTEXT implementation of XFree86 4.1.0 or before is very BUGGY!
				 */
				if( big5_buggy)
				{
					prefix = "\x1b\x25\x2f\x32\x80\x89" "BIG5-0"
						"\x02\x80\x89" "BIG5-0" "\x02" ;

					iso2022_conv->g0 = BIG5 ;
					iso2022_conv->g1 = BIG5 ;
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
		else if( ch.cs == KOI8_R || ch.cs == KOI8_U || ch.cs == VISCII)
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
		else if( conv->illegal_char)
		{
			size_t  size ;
			int  is_full ;
			
			size = (*conv->illegal_char)( conv , dst , dst_size - filled_size , &is_full , &ch) ;
			if( is_full)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}

			dst += size ;
			filled_size += size ;
		}
	}

	return  filled_size ;
}

static size_t
convert_to_xct(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_xct_intern( conv , dst , dst_size , parser , 1) ;
}

static size_t
convert_to_xct_big5_buggy(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_xct_intern( conv , dst , dst_size , parser , 1) ;
}

static void
xct_conv_init(
	mkf_conv_t *  conv
	)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	iso2022_conv = (mkf_iso2022_conv_t*) conv ;
	
	iso2022_conv->gl = &iso2022_conv->g0 ;
	iso2022_conv->gr = &iso2022_conv->g1 ;
	iso2022_conv->g0 = US_ASCII ;
	iso2022_conv->g1 = ISO8859_1_R ;
	iso2022_conv->g2 = UNKNOWN_CS ;
	iso2022_conv->g3 = UNKNOWN_CS ;
}

static void
conv_delete(
	mkf_conv_t *  conv
	)
{
	free( conv) ;
}


/* --- global functions --- */

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
	iso2022_conv->conv.init = xct_conv_init ;
	iso2022_conv->conv.delete = conv_delete ;
	iso2022_conv->conv.illegal_char = NULL ;

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
	iso2022_conv->conv.init = xct_conv_init ;
	iso2022_conv->conv.delete = conv_delete ;
	iso2022_conv->conv.illegal_char = NULL ;

	return  (mkf_conv_t*)iso2022_conv ;
}
