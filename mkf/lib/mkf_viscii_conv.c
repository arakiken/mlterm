/*
 *	$Id$
 */

#include  "mkf_viscii_conv.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_ucs4_map.h"


#if  0
#define  __DEBUG
#endif


/* --- static functions --- */

static void
remap_unsupported_charset(
	mkf_char_t *  ch
	)
{
	mkf_char_t  c ;

	if( ch->cs == VISCII)
	{
		/* do nothing */
	}
	else if( ch->cs == US_ASCII)
	{
		if( ch->ch[0] == 0x02 || ch->ch[0] == 0x05 || ch->ch[0] == 0x06 ||
			ch->ch[0] == 0x14 || ch->ch[0] == 0x19 || ch->ch[0] == 0x1e)
		{
			ch->cs = VISCII ;
		}
	}
	else if( mkf_map_via_ucs( &c , ch , VISCII))
	{
		*ch = c ;
	}
}

static size_t
convert_to_viscii(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	size_t  filled_size ;
	mkf_char_t  ch ;

	filled_size = 0 ;
	while( mkf_parser_next_char( parser , &ch))
	{
		remap_unsupported_charset( &ch) ;

		if( ch.cs == VISCII)
		{
			if( filled_size >= dst_size)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}

			*(dst ++) = ch.ch[0] ;

			filled_size ++ ;
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
mkf_viscii_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_viscii ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}
