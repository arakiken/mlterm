/*
 *	$Id$
 */

#include  "mkf_iso2022jp_conv.h"

#include  <stdio.h>		/* NULL */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_iso2022_intern.h"
#include  "mkf_ja_jp_map.h"


typedef struct  mkf_iso2022jp_conv
{
	mkf_conv_t   conv ;
	mkf_charset_t  cur_gl ;

} mkf_iso2022jp_conv_t ;


/* --- static functions --- */

static void
remap_unsupported_charset(
	mkf_char_t *  ch
	)
{
	mkf_char_t  c ;

	if( ch->cs == ISO10646_UCS4_1)
	{
		if( ! mkf_map_ucs4_to_ja_jp( &c , ch))
		{
			return ;
		}
		
		*ch = c ;
	}
	
	if( ch->cs == SJIS_IBM_EXT)
	{
		/*
		 * IBM extension characters is cannot be mapped as jisc6226 1978 gaiji
		 * (which is iso2022 based 94n charset) , so we managed to remap here.
		 */
		 
		if( mkf_map_ibm_ext_to_jisx0208_1983( &c , ch))
		{
			*ch = c ;
		}
		else if( mkf_map_ibm_ext_to_jisx0212_1990( &c , ch))
		{
			*ch = c ;
		}
	}
#if  0
	/*
	 * NEC special characters , NEC selected IBM characters and MAC extension charcters
	 * are exactly in gaiji area of jisc6226_1978 , so we do not remap these.
	 */
	 
	else if( ch->cs == JISC6226_1978_NEC_EXT)
	{
		if( mkf_map_nec_ext_to_jisx0208_1983( &c , ch))
		{
			*ch = c ;
		}
		else if( mkf_map_nec_ext_to_jisx0212_1990( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( ch->cs == JISC6226_1978_NECIBM_EXT)
	{
		if( mkf_map_necibm_ext_to_jisx0208_1983( &c , ch))
		{
			*ch = c ;
		}
		else if( mkf_map_necibm_ext_to_jisx0212_1990( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( ch->cs == JISX0208_1983_MAC_EXT)
	{
		if( mkf_map_mac_ext_to_jisx0208_1983( &c , ch))
		{
			*ch = c ;
		}
		else if( mkf_map_mac_ext_to_jisx0212_1990( &c , ch))
		{
			*ch = c ;
		}
	}
#else
	else if( ch->cs == JISC6226_1978_NEC_EXT || ch->cs == JISC6226_1978_NECIBM_EXT)
	{
		ch->cs = JISC6226_1978 ;
	}
	else if( ch->cs == JISX0208_1983_MAC_EXT)
	{
		ch->cs = JISX0208_1983 ;
	}
#endif
}

static size_t
convert_to_iso2022jp(
	mkf_iso2022jp_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser ,
	int  is_7 ,
	int  version
	)
{
	size_t  filled_size ;
	mkf_char_t  ch ;

	filled_size = 0 ;
	while( 1)
	{
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

		remap_unsupported_charset( &ch) ;

		if( (! is_7) && ch.cs == JISX0201_KATA)
		{
			if( filled_size >= dst_size)
			{
				mkf_parser_reset( parser) ;
			
				return  filled_size ;
			}
		
			*(dst ++) = MAP_TO_GR(*ch.ch) ;

			filled_size ++ ;
		}
		else
		{
			int  counter ;

			if( ch.cs == conv->cur_gl)
			{
				if( filled_size + ch.size > dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}
			}
			else
			{
				conv->cur_gl = ch.cs ;

				if( ch.cs == JISX0208_1983 ||
					(version <= 2 && ch.cs == JISC6226_1978) ||
					(version >= 2 && ch.cs == GB2312_80))
				{
					/* GB2312_80 for ISO2022JP-2(rfc1154) */
				#if  1
					/* based on old iso2022 */
					
					if( filled_size + ch.size + 2 >= dst_size)
					{
						mkf_parser_reset( parser) ;

						return  filled_size ;
					}
					
					*(dst ++) = ESC ;
					*(dst ++) = MB_CS ;
					*(dst ++) = CS94MB_FT(ch.cs) ;
					
					filled_size += 3 ;

				#else
					/* based on new iso2022 */
					
					if( filled_size + ch.size + 3 >= dst_size)
					{
						mkf_parser_reset( parser) ;

						return  filled_size ;
					}

					*(dst ++) = ESC ;
					*(dst ++) = MB_CS ;
					*(dst ++) = CS94_TO_G0 ;
					*(dst ++) = CS94MB_FT(ch.cs) ;
					
					filled_size += 4 ;
				#endif
				}
				else if( ch.cs == JISX0212_1990 ||
					(version >= 2 && ch.cs == KSC5601_1987) ||
					(version >= 3 &&
						(ch.cs == JISX0213_2000_1 || ch.cs == JISX0213_2000_2)))
				{
					/* KSC5601_1987 for ISO2022JP-2(rfc1154) */
					if( filled_size + ch.size + 3 >= dst_size)
					{
						mkf_parser_reset( parser) ;

						return  filled_size ;
					}

					*(dst ++) = ESC ;
					*(dst ++) = MB_CS ;
					*(dst ++) = CS94_TO_G0 ;
					*(dst ++) = CS94MB_FT(ch.cs) ;

					filled_size += 4 ;
				}
				else if( ch.cs == US_ASCII ||
					(version <= 2 && 
						(ch.cs == JISX0201_ROMAN || ch.cs == JISX0201_KATA)))
				{
					if( filled_size + ch.size + 2 >= dst_size)
					{
						mkf_parser_reset( parser) ;

						return  filled_size ;
					}

					*(dst ++) = ESC ;
					*(dst ++) = CS94_TO_G0 ;
					*(dst ++) = CS94SB_FT(ch.cs) ;

					filled_size += 3 ;
				}
				else if( version >= 2 && (ch.cs == ISO8859_1_R || ch.cs == ISO8859_7_R))
				{
					/* for ISO2022JP-2(rfc1154) */
					if( filled_size + ch.size + 2 >= dst_size)
					{
						mkf_parser_reset( parser) ;

						return  filled_size ;
					}

					*(dst ++) = ESC ;
					*(dst ++) = CS96_TO_G2 ;
					*(dst ++) = CS96SB_FT(ch.cs) ;

					filled_size += 3 ;
				}
				else
				{
				#ifdef  DEBUG
					kik_warn_printf( KIK_DEBUG_TAG
						" cs(%x) is not supported by iso2022jp. char(%x) is discarded.\n" ,
						ch.cs , mkf_char_to_int( &ch)) ;
				#endif

					if( filled_size >= dst_size)
					{
						mkf_parser_reset( parser) ;

						return  filled_size ;
					}
					
					*(dst ++) = ' ' ;
					filled_size ++ ;

					continue ;
				}
			}

			for( counter = 0 ; counter < ch.size ; counter ++)
			{
				*(dst ++) = ch.ch[counter] ;
			}
			
			filled_size += ch.size ;
		}
	}
}

static size_t
convert_to_iso2022jp_8(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso2022jp( ( mkf_iso2022jp_conv_t*)conv , dst , dst_size , parser , 0 , 1) ;
}

static size_t
convert_to_iso2022jp_7(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso2022jp( ( mkf_iso2022jp_conv_t*)conv , dst , dst_size , parser , 1 , 1) ;
}

static size_t
convert_to_iso2022jp2(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso2022jp( ( mkf_iso2022jp_conv_t*)conv , dst , dst_size , parser , 1 , 2) ;
}

static size_t
convert_to_iso2022jp3(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso2022jp( ( mkf_iso2022jp_conv_t*)conv , dst , dst_size , parser , 1 , 3) ;
}

static void
iso2022jp_conv_init(
	mkf_conv_t *  conv
	)
{
	mkf_iso2022jp_conv_t *  iso2022jp_conv ;

	iso2022jp_conv = (mkf_iso2022jp_conv_t*) conv ;

	iso2022jp_conv->cur_gl = US_ASCII ;
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
mkf_iso2022jp_8_conv_new(void)
{
	mkf_iso2022jp_conv_t *  iso2022jp_conv ;

	if( ( iso2022jp_conv = malloc( sizeof( mkf_iso2022jp_conv_t))) == NULL)
	{
		return  NULL ;
	}

	iso2022jp_conv->conv.convert = convert_to_iso2022jp_8 ;
	iso2022jp_conv->conv.init = iso2022jp_conv_init ;
	iso2022jp_conv->conv.delete = conv_delete ;
	iso2022jp_conv->cur_gl = US_ASCII ;

	return  (mkf_conv_t*)iso2022jp_conv ;
}

mkf_conv_t *
mkf_iso2022jp_7_conv_new(void)
{
	mkf_iso2022jp_conv_t *  iso2022jp_conv ;

	if( ( iso2022jp_conv = malloc( sizeof( mkf_iso2022jp_conv_t))) == NULL)
	{
		return  NULL ;
	}
	memset( iso2022jp_conv , 0 , sizeof( mkf_iso2022jp_conv_t)) ;

	iso2022jp_conv->conv.convert = convert_to_iso2022jp_7 ;
	iso2022jp_conv->conv.init = iso2022jp_conv_init ;
	iso2022jp_conv->conv.delete = conv_delete ;
	iso2022jp_conv->cur_gl = US_ASCII ;

	return  (mkf_conv_t*)iso2022jp_conv ;
}

mkf_conv_t *
mkf_iso2022jp2_conv_new(void)
{
	mkf_iso2022jp_conv_t *  iso2022jp_conv ;

	if( ( iso2022jp_conv = malloc( sizeof( mkf_iso2022jp_conv_t))) == NULL)
	{
		return  NULL ;
	}
	memset( iso2022jp_conv , 0 , sizeof( mkf_iso2022jp_conv_t)) ;

	iso2022jp_conv->conv.convert = convert_to_iso2022jp2 ;
	iso2022jp_conv->conv.init = iso2022jp_conv_init ;
	iso2022jp_conv->conv.delete = conv_delete ;
	iso2022jp_conv->cur_gl = US_ASCII ;

	return  (mkf_conv_t*)iso2022jp_conv ;
}

mkf_conv_t *
mkf_iso2022jp3_conv_new(void)
{
	mkf_iso2022jp_conv_t *  iso2022jp_conv ;

	if( ( iso2022jp_conv = malloc( sizeof( mkf_iso2022jp_conv_t))) == NULL)
	{
		return  NULL ;
	}

	iso2022jp_conv->conv.convert = convert_to_iso2022jp3 ;
	iso2022jp_conv->conv.init = iso2022jp_conv_init ;
	iso2022jp_conv->conv.delete = conv_delete ;
	iso2022jp_conv->cur_gl = US_ASCII ;

	return  (mkf_conv_t*)iso2022jp_conv ;
}
