/*
 *	update: <2001/11/26(02:43:07)>
 *	$Id$
 */

#include  "mkf_iso2022kr_conv.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_iso2022_intern.h"
#include  "mkf_ko_kr_map.h"


typedef struct  mkf_iso2022kr_conv
{
	mkf_conv_t   conv ;
	mkf_charset_t  cur_cs ;
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
		if( ! mkf_map_ucs4_to_ko_kr( &c , ch))
		{
			return ;
		}
		
		*ch = c ;
	}
	
	if( ch->cs == JOHAB)
	{
		if( mkf_map_johab_to_uhc( &c , ch))
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
	}
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
					" parser->init() returns error , but the process is continuing...\n") ;
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
		
		if( ch.cs == iso2022kr_conv->cur_cs)
		{
			if( filled_size + ch.size > dst_size)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}
		}
		else
		{
			iso2022kr_conv->cur_cs = ch.cs ;

			if( ch.cs == KSC5601_1987)
			{
				if( filled_size + ch.size >= dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = LS1 ;

				filled_size ++ ;
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
			}
			else
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" cs(%x) is not supported by iso2022kr. char(%x) is discarded.\n" ,
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

static void
conv_init(
	mkf_conv_t *  conv
	)
{
	mkf_iso2022kr_conv_t *  iso2022kr_conv ;

	iso2022kr_conv = (mkf_iso2022kr_conv_t*) conv ;

	iso2022kr_conv->cur_cs = US_ASCII ;
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

	iso2022kr_conv->conv.convert = convert_to_iso2022kr ;
	iso2022kr_conv->conv.init = conv_init ;
	iso2022kr_conv->conv.delete = conv_delete ;
	iso2022kr_conv->cur_cs = US_ASCII ;
	iso2022kr_conv->is_designated = 0 ;

	return  (mkf_conv_t*)iso2022kr_conv ;
}
