/*
 *	update: <2001/11/26(02:42:20)>
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
	int  counter ;

	hz_conv = (mkf_hz_conv_t *)conv ;

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
				mkf_parser_reset( parser) ;

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
					mkf_parser_reset( parser) ;

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
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = '~' ;
				*(dst ++) = '}' ;

				filled_size += 2 ;
			}
			else
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" cs(%x) is not supported by hz. char(%x) is discarded.\n" ,
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

	hz_conv->cur_cs = US_ASCII ;

	return  (mkf_conv_t*)hz_conv ;
}
