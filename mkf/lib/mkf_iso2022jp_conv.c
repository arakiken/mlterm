/*
 *	$Id$
 */

#include  "mkf_iso2022jp_conv.h"

#include  <stdio.h>		/* NULL */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_iso2022_conv.h"
#include  "mkf_iso2022_intern.h"
#include  "mkf_ucs4_map.h"
#include  "mkf_ja_jp_map.h"


/* --- static functions --- */

static void
remap_unsupported_charset(
	mkf_char_t *  ch ,
	int  version
	)
{
	mkf_char_t  c ;

	if( ch->cs == ISO10646_UCS4_1)
	{
		if( mkf_map_ucs4_to_ja_jp( &c , ch))
		{
			*ch = c ;
		}
	}

	mkf_iso2022_remap_unsupported_charset( ch) ;
	
	/*
	 * various gaiji chars => jis
	 */
	if( ch->cs == SJIS_IBM_EXT)
	{
		/*
		 * IBM extension characters cannot be regarded as jisc6226_1978/jisx0208_1983
		 * gaiji (which is based on iso2022 94n charset) , so we managed to remap here.
		 */
		 
		if( ! mkf_map_sjis_ibm_ext_to_jisx0208_1983( &c , ch) &&
			! mkf_map_sjis_ibm_ext_to_jisx0212_1990( &c , ch))
		{
			return ;
		}
		
		*ch = c ;
	}
	/*
	 * NEC special characters and NEC selected IBM characters are exactly in gaiji area
	 * of jisc6226_1978 , and MAC extension charcters are also in gaiji area of
	 * jisx0208_1983 , so we do not remap these.
	 */
	else if( ch->cs == JISC6226_1978_NEC_EXT || ch->cs == JISC6226_1978_NECIBM_EXT)
	{
		ch->cs = JISC6226_1978 ;
	}
	else if( ch->cs == JISX0208_1983_MAC_EXT)
	{
		ch->cs = JISX0208_1983 ;
	}
	
	/*
	 * conversion between JIS charsets.
	 */
	if( version == 3)
	{
		if( ch->cs == JISX0208_1983)
		{
			if( mkf_map_jisx0208_1983_to_jisx0213_2000_1( &c , ch))
			{
				*ch = c ;
			}
		}
	}
	else
	{
		if( ch->cs == JISX0213_2000_1)
		{
			if( mkf_map_jisx0213_2000_1_to_jisx0208_1983( &c , ch))
			{
				*ch = c ;
			}
		}
	}
}

static size_t
convert_to_iso2022jp(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser ,
	int  is_7 ,
	int  version
	)
{
	mkf_iso2022_conv_t *  iso2022_conv ;
	size_t  filled_size ;
	mkf_char_t  ch ;

	iso2022_conv = (mkf_iso2022_conv_t*) conv ;
	
	filled_size = 0 ;
	while( mkf_parser_next_char( parser , &ch))
	{
		remap_unsupported_charset( &ch , version) ;

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

			if( ch.cs == iso2022_conv->g0)
			{
				if( filled_size + ch.size > dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}
			}
			else
			{
				iso2022_conv->g0 = ch.cs ;

				if( ch.cs == JISX0208_1983 ||
					(version <= 2 && ch.cs == JISC6226_1978) ||
					/* GB2312_80 for ISO2022JP-2(rfc1154) */
					(version == 2 && ch.cs == GB2312_80))
				{
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
					/* KSC5601_1987 for ISO2022JP-2(rfc1154) */
					(version == 2 && ch.cs == KSC5601_1987) ||
					(version >= 3 &&
						(ch.cs == JISX0213_2000_1 || ch.cs == JISX0213_2000_2)))
				{
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
				else if( conv->illegal_char)
				{
					size_t  size ;
					int  is_full ;

					size = (*conv->illegal_char)( conv , dst , dst_size - filled_size ,
							&is_full , &ch) ;
					if( is_full)
					{
						mkf_parser_reset( parser) ;

						return  filled_size ;
					}

					dst += size ;
					filled_size += size ;

					continue ;
				}
				else
				{
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

	return  filled_size ;
}

static size_t
convert_to_iso2022jp_8(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso2022jp( conv , dst , dst_size , parser , 0 , 1) ;
}

static size_t
convert_to_iso2022jp_7(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso2022jp( conv , dst , dst_size , parser , 1 , 1) ;
}

static size_t
convert_to_iso2022jp2(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso2022jp( conv , dst , dst_size , parser , 1 , 2) ;
}

static size_t
convert_to_iso2022jp3(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso2022jp( conv , dst , dst_size , parser , 1 , 3) ;
}

static void
iso2022jp_7_conv_init(
	mkf_conv_t *  conv
	)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	iso2022_conv = (mkf_iso2022_conv_t*) conv ;

	iso2022_conv->gl = &iso2022_conv->g0 ;
	iso2022_conv->gr = NULL ;
	iso2022_conv->g0 = US_ASCII ;
	iso2022_conv->g1 = UNKNOWN_CS ;
	iso2022_conv->g2 = UNKNOWN_CS ;
	iso2022_conv->g3 = UNKNOWN_CS ;
}

static void
iso2022jp_8_conv_init(
	mkf_conv_t *  conv
	)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	iso2022_conv = (mkf_iso2022_conv_t*) conv ;

	iso2022_conv->gl = &iso2022_conv->g0 ;
	iso2022_conv->gr = &iso2022_conv->g1 ;
	iso2022_conv->g0 = US_ASCII ;
	iso2022_conv->g1 = JISX0201_KATA ;
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
mkf_iso2022jp_8_conv_new(void)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	if( ( iso2022_conv = malloc( sizeof( mkf_iso2022_conv_t))) == NULL)
	{
		return  NULL ;
	}

	iso2022jp_8_conv_init( (mkf_conv_t*) iso2022_conv) ;
	
	iso2022_conv->conv.convert = convert_to_iso2022jp_8 ;
	iso2022_conv->conv.init = iso2022jp_8_conv_init ;
	iso2022_conv->conv.delete = conv_delete ;
	iso2022_conv->conv.illegal_char = NULL ;

	return  (mkf_conv_t*)iso2022_conv ;
}

mkf_conv_t *
mkf_iso2022jp_7_conv_new(void)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	if( ( iso2022_conv = malloc( sizeof( mkf_iso2022_conv_t))) == NULL)
	{
		return  NULL ;
	}

	iso2022jp_7_conv_init( (mkf_conv_t*) iso2022_conv) ;
	
	iso2022_conv->conv.convert = convert_to_iso2022jp_7 ;
	iso2022_conv->conv.init = iso2022jp_7_conv_init ;
	iso2022_conv->conv.delete = conv_delete ;
	iso2022_conv->conv.illegal_char = NULL ;

	return  (mkf_conv_t*)iso2022_conv ;
}

mkf_conv_t *
mkf_iso2022jp2_conv_new(void)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	if( ( iso2022_conv = malloc( sizeof( mkf_iso2022_conv_t))) == NULL)
	{
		return  NULL ;
	}

	iso2022jp_7_conv_init( (mkf_conv_t*) iso2022_conv) ;
	
	iso2022_conv->conv.convert = convert_to_iso2022jp2 ;
	iso2022_conv->conv.init = iso2022jp_7_conv_init ;
	iso2022_conv->conv.delete = conv_delete ;
	iso2022_conv->conv.illegal_char = NULL ;
	
	return  (mkf_conv_t*)iso2022_conv ;
}

mkf_conv_t *
mkf_iso2022jp3_conv_new(void)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	if( ( iso2022_conv = malloc( sizeof( mkf_iso2022_conv_t))) == NULL)
	{
		return  NULL ;
	}

	iso2022jp_7_conv_init( (mkf_conv_t*) iso2022_conv) ;
	
	iso2022_conv->conv.convert = convert_to_iso2022jp3 ;
	iso2022_conv->conv.init = iso2022jp_7_conv_init ;
	iso2022_conv->conv.delete = conv_delete ;
	iso2022_conv->conv.illegal_char = NULL ;
	
	return  (mkf_conv_t*)iso2022_conv ;
}
