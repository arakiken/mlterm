/*
 *	$Id$
 */

#include  "mkf_conv.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_ru_map.h"


#if  0
#define  __DEBUG
#endif


/* --- static functions --- */

static void
remap_unsupported_charset(
	mkf_char_t *  ch ,
	mkf_charset_t  to_cs
	)
{
	mkf_char_t  c ;

	if( ch->cs == ISO10646_UCS4_1)
	{
		if( ! mkf_map_ucs4_to_ru( &c , ch))
		{
			return ;
		}

		*ch = c ;
	}

	if( to_cs == KOI8_R)
	{
		if( ch->cs == KOI8_U)
		{
			if( mkf_map_koi8_u_to_koi8_r( &c , ch))
			{
				*ch = c ;
			}
		}
		else if( ch->cs == ISO8859_5_R)
		{
			if( mkf_map_iso8859_5_r_to_koi8_r( &c , ch))
			{
				*ch = c ;
			}
		}
	}
	else if( to_cs == KOI8_U)
	{
		if( ch->cs == KOI8_R)
		{
			if( mkf_map_koi8_r_to_koi8_u( &c , ch))
			{
				*ch = c ;
			}
		}
		else if( ch->cs == ISO8859_5_R)
		{
			if( mkf_map_iso8859_5_r_to_koi8_u( &c , ch))
			{
				*ch = c ;
			}
		}
	}
}

static size_t
convert_to_koi8_intern(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser ,
	mkf_charset_t  to_cs
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

		remap_unsupported_charset( &ch , to_cs) ;

		if( (to_cs == KOI8_R && ch.cs == KOI8_R) ||
			(to_cs == KOI8_U && ch.cs == KOI8_U) ||
			ch.cs == US_ASCII)
		{
			if( filled_size >= dst_size)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}

			*(dst ++) = ch.ch[0] ;

			filled_size ++ ;
		}
		else
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" cs(%x) is not supported by koi8. char(%x) is discarded.\n" ,
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
convert_to_koi8_r(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_koi8_intern( conv , dst , dst_size , parser , KOI8_R) ;
}

static size_t
convert_to_koi8_u(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_koi8_intern( conv , dst , dst_size , parser , KOI8_U) ;
}

static void
conv_init(
	mkf_conv_t *  conv
	)
{
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
mkf_koi8_r_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_koi8_r ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;

	return  conv ;
}

mkf_conv_t *
mkf_koi8_u_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_koi8_u ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;

	return  conv ;
}
