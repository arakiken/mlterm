/*
 *	$Id$
 */

#include  "mkf_hz_conv.h"

#include  <stdio.h>		/* NULL */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_zh_cn_map.h"


typedef struct  mkf_hz_conv
{
	mkf_conv_t   conv ;
	mkf_charset_t  cur_cs ;

} mkf_hz_conv_t ;


/* --- static functions --- */

static void
remap_unsupported_charset(
	mkf_char_t *  ch
	)
{
	mkf_char_t  c ;

	if( ch->cs == ISO10646_UCS4_1)
	{
		if( ! mkf_map_ucs4_to_zh_cn( &c , ch))
		{
			return ;
		}
		
		*ch = c ;
	}
	
	if( ch->cs == GBK)
	{
		if( mkf_map_gbk_to_gb2312_80( &c , ch))
		{
			*ch = c ;
		}
	}
}

static size_t
convert_to_hz(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	mkf_hz_conv_t *  hz_conv ;
	size_t  filled_size ;
	mkf_char_t  ch ;
	int  count ;

	hz_conv = (mkf_hz_conv_t *)conv ;

	filled_size = 0 ;
	while( mkf_parser_next_char( parser , &ch))
	{
		remap_unsupported_charset( &ch) ;

		if( ch.ch[0] == '~' && ch.cs == US_ASCII)
		{
			ch.ch[1] = '~' ;
			ch.size = 2 ;
		}

		if( ch.cs == hz_conv->cur_cs)
		{
			if( filled_size + ch.size - 1 > dst_size)
			{
				mkf_parser_full_reset( parser) ;

				return  filled_size ;
			}
		}
		else
		{
			hz_conv->cur_cs = ch.cs ;
			
			if( ch.cs == GB2312_80)
			{
				if( filled_size + ch.size + 1 >= dst_size)
				{
					mkf_parser_full_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = '~' ;
				*(dst ++) = '{' ;

				filled_size += 2 ;
			}
			else if( ch.cs == US_ASCII)
			{
				if( filled_size + ch.size + 1 >= dst_size)
				{
					mkf_parser_full_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = '~' ;
				*(dst ++) = '}' ;

				filled_size += 2 ;
			}
			else if( conv->illegal_char)
			{
				size_t  size ;
				int  is_full ;

				size = (*conv->illegal_char)( conv , dst , dst_size - filled_size ,
						&is_full , &ch) ;
				if( is_full)
				{
					mkf_parser_full_reset( parser) ;

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
		
		for( count = 0 ; count < ch.size ; count ++)
		{
			*(dst ++) = ch.ch[count] ;
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
	mkf_hz_conv_t *  hz_conv ;

	hz_conv = (mkf_hz_conv_t*) conv ;
	
	hz_conv->cur_cs = US_ASCII ;
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
mkf_hz_conv_new(void)
{
	mkf_hz_conv_t *  hz_conv ;

	if( ( hz_conv = malloc( sizeof( mkf_hz_conv_t))) == NULL)
	{
		return  NULL ;
	}

	hz_conv->conv.convert = convert_to_hz ;
	hz_conv->conv.init = conv_init ;
	hz_conv->conv.delete = conv_delete ;
	hz_conv->conv.illegal_char = NULL ;

	hz_conv->cur_cs = US_ASCII ;

	return  (mkf_conv_t*)hz_conv ;
}
