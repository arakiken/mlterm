/*
 *	$Id$
 */

#include  "mkf_iso2022cn_conv.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_iso2022_intern.h"
#include  "mkf_iso2022_conv.h"
#include  "mkf_ucs4_map.h"
#include  "mkf_zh_cn_map.h"
#include  "mkf_zh_tw_map.h"


/* --- static functions --- */

static void
remap_unsupported_charset(
	mkf_char_t *  ch
	)
{
	mkf_char_t  c ;

	if( ch->cs == ISO10646_UCS4_1)
	{
		if( mkf_map_ucs4_to_zh_cn( &c , ch))
		{
			*ch = c ;
		}
		else if( mkf_map_ucs4_to_zh_tw( &c , ch))
		{
			*ch = c ;
		}
		else if( mkf_map_ucs4_to_iso2022cs( &c , ch))
		{
			*ch = c ;
		}
		else
		{
			return ;
		}
	}

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
	}
	else if( ch->cs == GBK)
	{
		if( mkf_map_gbk_to_gb2312_80( &c , ch))
		{
			*ch = c ;
		}
	}
}

static size_t
convert_to_iso2022cn(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	mkf_iso2022_conv_t *  iso2022_conv ;
	size_t  filled_size ;
	mkf_char_t  ch ;
	int  counter ;

	iso2022_conv = (mkf_iso2022_conv_t*) conv ;

	filled_size = 0 ;
	while( mkf_parser_next_char( parser , &ch))
	{
		remap_unsupported_charset( &ch) ;
		
		if( ch.cs == *iso2022_conv->gl)
		{
			if( filled_size + ch.size > dst_size)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}
			
			if( ch.cs == US_ASCII && ch.ch[0] == '\n')
			{
				/* reset */
				
				iso2022_conv->g1 = UNKNOWN_CS ;
				iso2022_conv->g2 = UNKNOWN_CS ;
			}
		}
		else if( ch.cs == CNS11643_1992_2)
		{
			/* single shifted */
			
			if( iso2022_conv->g2 != CNS11643_1992_2)
			{
				if( filled_size + ch.size + 6 > dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = ESC ;
				*(dst ++) = MB_CS ;
				*(dst ++) = CS94_TO_G2 ;
				*(dst ++) = CS94MB_FT(CNS11643_1992_2) ;
				*(dst ++) = ESC ;
				*(dst ++) = SS2_7 ;

				filled_size += 6 ;

				iso2022_conv->g2 = CNS11643_1992_2 ;
			}
			else
			{
				if( filled_size + ch.size + 2 > dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = ESC ;
				*(dst ++) = SS2_7 ;

				filled_size += 2 ;
			}
		}
		else if( ch.cs == CNS11643_1992_1 || ch.cs == GB2312_80)
		{
			if( iso2022_conv->g1 != ch.cs)
			{
				if( filled_size + ch.size + 5 > dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = ESC ;
				*(dst ++) = MB_CS ;
				*(dst ++) = CS94_TO_G1 ;
				*(dst ++) = CS94MB_FT(ch.cs) ;
				*(dst ++) = LS1 ;

				filled_size += 5 ;

				iso2022_conv->g1 = ch.cs ;
			}
			else
			{
				if( filled_size + ch.size + 1 > dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = LS1 ;

				filled_size ++ ;
			}

			iso2022_conv->gl = &iso2022_conv->g1 ;
		}
		else if( ch.cs == US_ASCII)
		{
			if( filled_size + ch.size + 1 > dst_size)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}

			*(dst ++) = LS0 ;

			filled_size ++ ;

			if( ch.ch[0] == '\n')
			{
				/* reset */
				
				iso2022_conv->g1 = UNKNOWN_CS ;
				iso2022_conv->g2 = UNKNOWN_CS ;
			}

			iso2022_conv->gl = &iso2022_conv->g0 ;
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

			continue ;
		}
		else
		{
			continue ;
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
	mkf_iso2022_conv_t *  iso2022_conv ;

	iso2022_conv = (mkf_iso2022_conv_t*) conv ;

	iso2022_conv->gl = &iso2022_conv->g0 ;
	iso2022_conv->gr = NULL ;
	iso2022_conv->g0 = US_ASCII ;
	iso2022_conv->g1 = UNKNOWN_CS ;
	iso2022_conv->g2 = UNKNOWN_CS ;
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
mkf_iso2022cn_conv_new(void)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	if( ( iso2022_conv = malloc( sizeof( mkf_iso2022_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv_init( ( mkf_conv_t*) iso2022_conv) ;

	iso2022_conv->conv.convert = convert_to_iso2022cn ;
	iso2022_conv->conv.init = conv_init ;
	iso2022_conv->conv.delete = conv_delete ;
	iso2022_conv->conv.illegal_char = NULL ;

	return  (mkf_conv_t*)iso2022_conv ;
}
