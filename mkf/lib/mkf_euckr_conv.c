/*
 *	$Id$
 */

#include  "mkf_euckr_conv.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_iso2022_intern.h"
#include  "mkf_ko_kr_map.h"


/* --- static functions --- */

static void
remap_unsupported_charset(
	mkf_char_t *  ch ,
	int  to_uhc
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
		if( ! mkf_map_johab_to_uhc( &c , ch))
		{
			return ;
		}
		
		*ch = c ;
	}
	
	if( to_uhc && ch->cs == KSC5601_1987)
	{
		if( mkf_map_ksc5601_1987_to_uhc( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( (! to_uhc) && ch->cs == UHC)
	{
		if( mkf_map_uhc_to_ksc5601_1987( &c , ch))
		{
			*ch = c ;
		}
	}
}

static size_t
convert_to_euckr_intern(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser ,
	int   is_uhc
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

		if( is_uhc)
		{
			remap_unsupported_charset( &ch , 1) ;
		}
		else
		{
			remap_unsupported_charset( &ch , 0) ;
		}
		
		if( ch.cs == US_ASCII)
		{
			if( filled_size >= dst_size)
			{
				mkf_parser_reset( parser) ;
			
				return  filled_size ;
			}

			*(dst ++) = *ch.ch ;

			filled_size ++ ;
		}
		else if( (! is_uhc) && ch.cs == KSC5601_1987)
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
		else if( is_uhc && ch.cs == UHC)
		{
			if( filled_size + 1 >= dst_size)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}

			*(dst ++) = ch.ch[0] ;
			*(dst ++) = ch.ch[1] ;
			
			filled_size += 2 ;
		}
		else
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" cs(%x) is not supported by euckr. char(%x) is discarded.\n" ,
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
convert_to_euckr(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_euckr_intern( conv , dst , dst_size , parser , 0) ;
}

static size_t
convert_to_uhc(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_euckr_intern( conv , dst , dst_size , parser , 1) ;
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

static mkf_conv_t *
euckr_conv_new(
	size_t (*convert)( struct mkf_conv * , u_char * , size_t , mkf_parser_t *)
	)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;

	return  conv ;
}


/* --- global functions --- */

mkf_conv_t *
mkf_euckr_conv_new(void)
{
	return  euckr_conv_new( convert_to_euckr) ;
}

mkf_conv_t *
mkf_uhc_conv_new(void)
{
	return  euckr_conv_new( convert_to_uhc) ;
}
