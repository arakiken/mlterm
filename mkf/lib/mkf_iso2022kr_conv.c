/*
 *	$Id$
 */

#include  "mkf_iso2022kr_conv.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_iso2022_conv.h"
#include  "mkf_iso2022_intern.h"
#include  "mkf_ucs4_map.h"
#include  "mkf_ko_kr_map.h"


typedef struct  mkf_iso2022kr_conv
{
	mkf_iso2022_conv_t  iso2022_conv ;
	int  is_designated ;

} mkf_iso2022kr_conv_t ;


/* --- static functions --- */

static void
remap_unsupported_charset(
	mkf_char_t *  ch
	)
{
	mkf_char_t  c ;

	if( ch->cs == ISO10646_UCS4_1)
	{
		if( mkf_map_ucs4_to_ko_kr( &c , ch))
		{
			*ch = c ;
		}
	}

	mkf_iso2022_remap_unsupported_charset( ch) ;
}

static size_t
convert_to_iso2022kr(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	mkf_iso2022kr_conv_t *  iso2022kr_conv ;
	size_t  filled_size ;
	mkf_char_t  ch ;

	iso2022kr_conv = (mkf_iso2022kr_conv_t*) conv ;

	filled_size = 0 ;

	if( ! iso2022kr_conv->is_designated)
	{
		if( dst_size < 4)
		{
			return  0 ;
		}

		*(dst ++) = ESC ;
		*(dst ++) = MB_CS ;
		*(dst ++) = CS94_TO_G1 ;
		*(dst ++) = CS94MB_FT(KSC5601_1987) ;

		filled_size += 4 ;
		
		iso2022kr_conv->is_designated = 1 ;
	}
	
	while( mkf_parser_next_char( parser , &ch))
	{
		int  counter ;

		remap_unsupported_charset( &ch) ;
		
		if( ch.cs == *iso2022kr_conv->iso2022_conv.gl)
		{
			if( filled_size + ch.size > dst_size)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}
		}
		else
		{
			iso2022kr_conv->iso2022_conv.g0 = ch.cs ;

			if( ch.cs == KSC5601_1987)
			{
				if( filled_size + ch.size >= dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = LS1 ;

				filled_size ++ ;

				iso2022kr_conv->iso2022_conv.gl = &iso2022kr_conv->iso2022_conv.g1 ;
			}
			else if( ch.cs == US_ASCII)
			{
				if( filled_size + ch.size >= dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = LS0 ;

				filled_size ++ ;
				
				iso2022kr_conv->iso2022_conv.gl = &iso2022kr_conv->iso2022_conv.g0 ;
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

	return  filled_size ;
}

static void
conv_init(
	mkf_conv_t *  conv
	)
{
	mkf_iso2022kr_conv_t *  iso2022kr_conv ;

	iso2022kr_conv = (mkf_iso2022kr_conv_t*) conv ;

	iso2022kr_conv->iso2022_conv.gl = &iso2022kr_conv->iso2022_conv.g0 ;
	iso2022kr_conv->iso2022_conv.gr = NULL ;
	iso2022kr_conv->iso2022_conv.g0 = US_ASCII ;
	iso2022kr_conv->iso2022_conv.g1 = UNKNOWN_CS ;
	iso2022kr_conv->iso2022_conv.g2 = UNKNOWN_CS ;
	iso2022kr_conv->iso2022_conv.g3 = UNKNOWN_CS ;
	
	iso2022kr_conv->is_designated = 0 ;
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
mkf_iso2022kr_conv_new(void)
{
	mkf_iso2022kr_conv_t *  iso2022kr_conv ;

	if( ( iso2022kr_conv = malloc( sizeof( mkf_iso2022kr_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv_init( (mkf_conv_t*) iso2022kr_conv) ;

	iso2022kr_conv->iso2022_conv.conv.convert = convert_to_iso2022kr ;
	iso2022kr_conv->iso2022_conv.conv.init = conv_init ;
	iso2022kr_conv->iso2022_conv.conv.delete = conv_delete ;
	iso2022kr_conv->iso2022_conv.conv.illegal_char = NULL ;
	
	return  (mkf_conv_t*)iso2022kr_conv ;
}
