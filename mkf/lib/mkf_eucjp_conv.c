/*
 *	$Id$
 */

#include  "mkf_eucjp_conv.h"

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
	mkf_charset_t  g1 ,
	mkf_charset_t  g3
	)
{
	mkf_char_t  c ;

	if( ch->cs == ISO10646_UCS4_1)
	{
		if( ! mkf_map_ucs4_to_ja_jp( &c , ch) && ! mkf_map_ucs4_to_iso2022cs( &c , ch))
		{
			return ;
		}
		
		*ch = c ;
	}
	
	/*
	 * various private chars -> jis
	 */
	if( ch->cs == JISC6226_1978_NEC_EXT)
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
	else if( ch->cs == SJIS_IBM_EXT)
	{
		if( mkf_map_ibm_ext_to_jisx0208_1983( &c , ch))
		{
			*ch = c ;
		}
		else if( mkf_map_ibm_ext_to_jisx0212_1990( &c , ch))
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
	/*
	 * conversion between JIS charsets.
	 */
	else if( ch->cs == JISC6226_1978)
	{
		/*
		 * we mkf_eucjp_parser don't support JISC6226_1978.
		 * If you want to use JISC6226_1978 , use iso2022(jp).
		 *
		 * XXX
		 * 22 characters are swapped between 1978 and 1983.
		 * so , we should reswap these here , but for the time being ,
		 * we do nothing.
		 */

		ch->cs = JISX0208_1983 ;
	}
	else if( g1 == JISX0208_1983 && ch->cs == JISX0213_2000_1)
	{
		if( mkf_map_jisx0213_2000_1_to_jisx0208_1983( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( g1 == JISX0213_2000_1 && ch->cs == JISX0208_1983)
	{
		if( mkf_map_jisx0208_1983_to_jisx0213_2000_1( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( g3 == JISX0212_1990 && ch->cs == JISX0213_2000_2)
	{
		if( mkf_map_jisx0213_2000_2_to_jisx0212_1990( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( g3 == JISX0213_2000_2 && ch->cs == JISX0212_1990)
	{
		if( mkf_map_jisx0212_1990_to_jisx0213_2000_2( &c , ch))
		{
			*ch = c ;
		}
	}
}

static size_t
convert_to_eucjp(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	size_t  filled_size ;
	mkf_char_t  ch ;
	mkf_iso2022_conv_t *  iso2022_conv ;

	iso2022_conv = (mkf_iso2022_conv_t*) conv ;

	filled_size = 0 ;
	while( mkf_parser_next_char( parser , &ch))
	{
		remap_unsupported_charset( &ch , iso2022_conv->g1 , iso2022_conv->g3) ;
		
		if( ch.cs == US_ASCII || ch.cs == JISX0201_ROMAN)
		{
			if( filled_size >= dst_size)
			{
				mkf_parser_reset( parser) ;
			
				return  filled_size ;
			}

			*(dst ++) = *ch.ch ;

			filled_size ++ ;
		}
		else if( ch.cs == iso2022_conv->g1)
		{
			if( filled_size + 1 >= dst_size)
			{
				mkf_parser_reset( parser) ;
				
				return  filled_size ;
			}
			
			*(dst ++) = MAP_TO_GR( ch.ch[0]) ;
			*(dst ++) = MAP_TO_GR( ch.ch[1]) ;

			filled_size += 2 ;
		}
		else if( ch.cs == JISX0201_KATA)
		{
			if( filled_size + 1 >= dst_size)
			{
				mkf_parser_reset( parser) ;
			
				return  filled_size ;
			}

			*(dst ++) = SS2 ;
			*(dst ++) = SET_MSB(*ch.ch) ;

			filled_size += 2 ;
		}
		else if( ch.cs == iso2022_conv->g3)
		{
			if( filled_size + 2 >= dst_size)
			{
				mkf_parser_reset( parser) ;
			
				return  filled_size ;
			}
			
			*(dst ++) = SS3 ;
			*(dst ++) = ch.ch[0] ;
			*(dst ++) = ch.ch[1] ;

			filled_size += 3 ;
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

static void
eucjp_conv_init(
	mkf_conv_t *  conv
	)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	iso2022_conv = ( mkf_iso2022_conv_t*) conv ;

	iso2022_conv->gl = &iso2022_conv->g0 ;
	iso2022_conv->gr = &iso2022_conv->g1 ;
	iso2022_conv->g0 = US_ASCII ;
	iso2022_conv->g1 = JISX0208_1983 ;
	iso2022_conv->g2 = JISX0201_KATA ;
	iso2022_conv->g3 = JISX0212_1990 ;
}

static void
eucjisx0213_conv_init(
	mkf_conv_t *  conv
	)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	iso2022_conv = ( mkf_iso2022_conv_t*) conv ;

	iso2022_conv->gl = &iso2022_conv->g0 ;
	iso2022_conv->gr = &iso2022_conv->g1 ;
	iso2022_conv->g0 = US_ASCII ;
	iso2022_conv->g1 = JISX0213_2000_1 ;
	iso2022_conv->g2 = JISX0201_KATA ;
	iso2022_conv->g3 = JISX0213_2000_2 ;
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
mkf_eucjp_conv_new(void)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	if( ( iso2022_conv = malloc( sizeof( mkf_iso2022_conv_t))) == NULL)
	{
		return  NULL ;
	}

	eucjp_conv_init( ( mkf_conv_t*) iso2022_conv) ;
	
	iso2022_conv->conv.convert = convert_to_eucjp ;
	iso2022_conv->conv.init = eucjp_conv_init ;
	iso2022_conv->conv.delete = conv_delete ;
	iso2022_conv->conv.illegal_char = NULL ;

	return  (mkf_conv_t*) iso2022_conv ;
}

mkf_conv_t *
mkf_eucjisx0213_conv_new(void)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	if( ( iso2022_conv = malloc( sizeof( mkf_iso2022_conv_t))) == NULL)
	{
		return  NULL ;
	}

	eucjisx0213_conv_init( ( mkf_conv_t*) iso2022_conv) ;

	iso2022_conv->conv.convert = convert_to_eucjp ;
	iso2022_conv->conv.init = eucjisx0213_conv_init ;
	iso2022_conv->conv.delete = conv_delete ;
	iso2022_conv->conv.illegal_char = NULL ;

	return  (mkf_conv_t*) iso2022_conv ;
}
