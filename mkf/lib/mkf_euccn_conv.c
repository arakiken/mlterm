/*
 *	update: <2001/11/26(02:41:25)>
 *	$Id$
 */

#include  "mkf_euccn_conv.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_iso2022_intern.h"
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
		if( ! mkf_map_ucs4_to_zh_cn( &c , ch))
		{
			return ;
		}
		
		*ch = c ;
	}

	if( encoding == EUCCN_NORMAL)
	{
		if( ch->cs == GBK)
		{
			if( mkf_map_gbk_to_gb2312_80( &c , ch))
			{
				*ch = c ;
			}
		}
	}
	else if( encoding == EUCCN_GBK || encoding == EUCCN_GB18030_2000)
	{
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
		else
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" cs(%x) is not supported by euccn. char(%x) is discarded.\n" ,
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
euccn_conv_new(
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
mkf_euccn_conv_new(void)
{
	return  euccn_conv_new( convert_to_euccn) ;
}

mkf_conv_t *
mkf_gbk_conv_new(void)
{
	return  euccn_conv_new( convert_to_gbk) ;
}

mkf_conv_t *
mkf_gb18030_2000_conv_new(void)
{
	return  euccn_conv_new( convert_to_gb18030_2000) ;
}
