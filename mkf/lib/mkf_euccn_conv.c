/*
 *	$Id$
 */

#include  "mkf_euccn_conv.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_iso2022_conv.h"
#include  "mkf_iso2022_intern.h"
#include  "mkf_ucs4_map.h"
#include  "mkf_zh_cn_map.h"
#include  "mkf_gb18030_2000_intern.h"


typedef enum  euccn_encoding
{
	EUCCN_NORMAL ,
	EUCCN_GBK ,
	EUCCN_GB18030_2000

} enccn_encoding_t ;


/* --- static functions --- */

static void
remap_unsupported_charset(
	mkf_char_t *  ch ,
	enccn_encoding_t  encoding
	)
{
	mkf_char_t  c ;

	if( ch->cs == ISO10646_UCS4_1)
	{
		if( mkf_map_ucs4_to_zh_cn( &c , ch))
		{
			*ch = c ;
		}
	}

	if( encoding == EUCCN_NORMAL)
	{
		mkf_iso2022_remap_unsupported_charset( ch) ;
	}
	else
	{
		if( ch->cs == ISO10646_UCS4_1)
		{
			return ;
		}

		if( ch->cs == GB2312_80)
		{
			if( mkf_map_gb2312_80_to_gbk( &c , ch))
			{
				*ch = c ;
			}
		}
	}
}

static size_t
convert_to_euccn_intern(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser ,
	enccn_encoding_t  encoding
	)
{
	size_t  filled_size ;
	mkf_char_t  ch ;

	filled_size = 0 ;
	while( mkf_parser_next_char( parser , &ch))
	{
		remap_unsupported_charset( &ch , encoding) ;
		
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
		else if( encoding == EUCCN_NORMAL && ch.cs == GB2312_80)
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
		else if( (encoding == EUCCN_GBK || encoding == EUCCN_GB18030_2000) && ch.cs == GBK)
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
		else if( encoding == EUCCN_GB18030_2000 && ch.cs == ISO10646_UCS4_1)
		{
			u_char  gb18030[4] ;
			
			if( filled_size + 3 >= dst_size)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}

			if( mkf_encode_ucs4_to_gb18030_2000( gb18030 , ch.ch) == 0)
			{
				continue ;
			}

			*(dst ++) = gb18030[0] ;
			*(dst ++) = gb18030[1] ;
			*(dst ++) = gb18030[2] ;
			*(dst ++) = gb18030[3] ;
			
			filled_size += 4 ;
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
convert_to_euccn(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_euccn_intern( conv , dst , dst_size , parser , EUCCN_NORMAL) ;
}

static size_t
convert_to_gbk(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_euccn_intern( conv , dst , dst_size , parser , EUCCN_GBK) ;
}

static size_t
convert_to_gb18030_2000(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_euccn_intern( conv , dst , dst_size , parser , EUCCN_GB18030_2000) ;
}

static void
euccn_conv_init(
	mkf_conv_t *  conv
	)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	iso2022_conv = ( mkf_iso2022_conv_t*) conv ;

	iso2022_conv->gl = &iso2022_conv->g0 ;
	iso2022_conv->gr = &iso2022_conv->g1 ;
	iso2022_conv->g0 = US_ASCII ;
	iso2022_conv->g1 = GB2312_80 ;
	iso2022_conv->g2 = UNKNOWN_CS ;
	iso2022_conv->g3 = UNKNOWN_CS ;
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
mkf_euccn_conv_new(void)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	if( ( iso2022_conv = malloc( sizeof( mkf_iso2022_conv_t))) == NULL)
	{
		return  NULL ;
	}

	euccn_conv_init( ( mkf_conv_t*) iso2022_conv) ;

	iso2022_conv->conv.convert = convert_to_euccn ;
	iso2022_conv->conv.init = euccn_conv_init ;
	iso2022_conv->conv.delete = conv_delete ;
	iso2022_conv->conv.illegal_char = NULL ;

	return  (mkf_conv_t*)iso2022_conv ;
}

mkf_conv_t *
mkf_gbk_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_gbk ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}

mkf_conv_t *
mkf_gb18030_2000_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_gb18030_2000 ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}
