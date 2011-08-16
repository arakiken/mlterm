/*
 *	$Id$
 */

#include  "mkf_big5_conv.h"

#include  <string.h>		/* strncmp */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_locale.h>
#include  <kiklib/kik_util.h>	/* K_MIN */

#include  "mkf_zh_tw_map.h"
#include  "mkf_zh_hk_map.h"


/* --- static functions --- */

static void
remap_unsupported_charset(
	mkf_char_t *  ch
	)
{
	mkf_char_t  c ;

	if( ch->cs == ISO10646_UCS4_1)
	{
		char *  locale ;

		locale = kik_get_locale() ;

		if( strncmp( locale , "zh_HK" , 5) == 0)
		{
			if( ! mkf_map_ucs4_to_zh_hk( &c , ch))
			{
				return ;
			}
		}
		else
		{
			if( ! mkf_map_ucs4_to_zh_tw( &c , ch))
			{
				return ;
			}
		}
		
		*ch = c ;
	}
	
	if( ch->cs == CNS11643_1992_1)
	{
		if( mkf_map_cns11643_1992_1_to_big5( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( ch->cs == CNS11643_1992_2)
	{
		if( mkf_map_cns11643_1992_2_to_big5( &c , ch))
		{
			*ch = c ;
		}
	}
}

static size_t
convert_to_big5(
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

		if( ch.cs == BIG5 || ch.cs == HKSCS)
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
		else if( ch.cs == US_ASCII)
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
mkf_big5_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_big5 ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_big5hkscs_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_big5 ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}
