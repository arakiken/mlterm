/*
 *	$Id$
 */

#include  "mkf_conv.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_ru_map.h"
#include  "mkf_ucs4_map.h"


#if  0
#define  __DEBUG
#endif


/* --- static functions --- */

static int
map_direct(
	mkf_char_t *  dst ,
	mkf_char_t *  src ,
	mkf_charset_t  to_cs
	)
{
	if( src->cs == KOI8_U && to_cs == KOI8_R)
	{
		return  mkf_map_koi8_u_to_koi8_r( dst , src) ;
	}
	else if( src->cs == KOI8_R && to_cs == KOI8_U)
	{
		return  mkf_map_koi8_r_to_koi8_u( dst , src) ;
	}

	return  0 ;
}

static void
remap_unsupported_charset(
	mkf_char_t *  ch ,
	mkf_charset_t  to_cs
	)
{
	mkf_char_t  c ;

	if( ch->cs == to_cs)
	{
		/* do nothing */
	}
	else if( map_direct( &c , ch , to_cs))
	{
		*ch = c ;
	}
	else if( mkf_map_via_ucs( &c , ch , to_cs))
	{
		*ch = c ;
	}
}

static size_t
convert_to_intern(
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
	while( mkf_parser_next_char( parser , &ch))
	{
		remap_unsupported_charset( &ch , to_cs) ;

		if( to_cs == ch.cs || ch.cs == US_ASCII)
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

static size_t
convert_to_koi8_r(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , KOI8_R) ;
}

static size_t
convert_to_koi8_u(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , KOI8_U) ;
}

static size_t
convert_to_koi8_t(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , KOI8_T) ;
}

static size_t
convert_to_georgian_ps(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_intern( conv , dst , dst_size , parser , GEORGIAN_PS) ;
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
	conv->illegal_char = NULL ;

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
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_koi8_t_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_koi8_t ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_georgian_ps_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_georgian_ps ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}
