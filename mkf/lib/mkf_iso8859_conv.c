/*
 *	$Id$
 */

#include  "mkf_iso8859_conv.h"

#include  <stdio.h>		/* NULL */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_iso2022_intern.h"
#include  "mkf_viet_map.h"
#include  "mkf_ru_map.h"
#include  "mkf_ucs4_usascii.h"
#include  "mkf_ucs4_iso8859.h"
#include  "mkf_ucs4_map.h"


/* --- static functions --- */

static void
remap_unsupported_charset(
	mkf_char_t *  ch ,
	mkf_charset_t  gr_cs
	)
{
	mkf_char_t  c ;

	if( ch->cs == ISO10646_UCS4_1)
	{
		if( mkf_map_ucs4_to_us_ascii( &c , mkf_char_to_int( ch)))
		{
			*ch = c ;

			return ;
		}
		
		if( gr_cs == ISO8859_5_R)
		{
			if( ! mkf_map_ucs4_to_ru( &c , ch))
			{
				return ;
			}
		}
		else if( gr_cs == TCVN5712_3_1993)
		{
			if( ! mkf_map_ucs4_to_viet( &c , ch))
			{
				return ;
			}
		}
		else if( ! mkf_map_ucs4_to_cs( &c , ch , gr_cs))
		{
			return ;
		}

		*ch = c ;
	}
	
	if( gr_cs == TCVN5712_3_1993 && ch->cs == VISCII)
	{
		if( mkf_map_viscii_to_tcvn5712_3_1993( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( gr_cs == ISO8859_5_R && ch->cs == KOI8_R)
	{
		if( mkf_map_koi8_r_to_iso8859_5_r( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( gr_cs == ISO8859_5_R && ch->cs == KOI8_U)
	{
		if( mkf_map_koi8_u_to_iso8859_5_r( &c , ch))
		{
			*ch = c ;
		}
	}
}

static size_t
convert_to_iso8859(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser ,
	mkf_charset_t  gr_cs
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

		remap_unsupported_charset( &ch , gr_cs) ;

		if( ch.cs == US_ASCII)
		{
			if( filled_size >= dst_size)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}

			*(dst ++) = ch.ch[0] ;
			filled_size ++ ;
		}
		else if( ch.cs == gr_cs)
		{
			if( filled_size >= dst_size)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}

			*(dst ++) = SET_MSB(ch.ch[0]) ;
			filled_size ++ ;
		}
		else
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" cs(%x) is not supported by iso8859. char(%x) is discarded.\n" ,
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
convert_to_iso8859_1(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso8859( conv , dst , dst_size , parser , ISO8859_1_R) ;
}

static size_t
convert_to_iso8859_2(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso8859( conv , dst , dst_size , parser , ISO8859_2_R) ;
}

static size_t
convert_to_iso8859_3(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso8859( conv , dst , dst_size , parser , ISO8859_3_R) ;
}

static size_t
convert_to_iso8859_4(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso8859( conv , dst , dst_size , parser , ISO8859_4_R) ;
}

static size_t
convert_to_iso8859_5(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso8859( conv , dst , dst_size , parser , ISO8859_5_R) ;
}

static size_t
convert_to_iso8859_6(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso8859( conv , dst , dst_size , parser , ISO8859_6_R) ;
}

static size_t
convert_to_iso8859_7(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso8859( conv , dst , dst_size , parser , ISO8859_7_R) ;
}

static size_t
convert_to_iso8859_8(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso8859( conv , dst , dst_size , parser , ISO8859_8_R) ;
}

static size_t
convert_to_iso8859_9(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso8859( conv , dst , dst_size , parser , ISO8859_9_R) ;
}

static size_t
convert_to_iso8859_10(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso8859( conv , dst , dst_size , parser , ISO8859_10_R) ;
}

static size_t
convert_to_tis620_2533(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso8859( conv , dst , dst_size , parser , TIS620_2533) ;
}

static size_t
convert_to_iso8859_13(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso8859( conv , dst , dst_size , parser , ISO8859_13_R) ;
}

static size_t
convert_to_iso8859_14(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso8859( conv , dst , dst_size , parser , ISO8859_14_R) ;
}

static size_t
convert_to_iso8859_15(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso8859( conv , dst , dst_size , parser , ISO8859_15_R) ;
}

static size_t
convert_to_iso8859_16(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso8859( conv , dst , dst_size , parser , ISO8859_16_R) ;
}

static size_t
convert_to_tcvn5712_3_1993(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	return  convert_to_iso8859( conv , dst , dst_size , parser , TCVN5712_3_1993) ;
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
iso8859_conv_new(
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
mkf_iso8859_1_conv_new(void)
{
	return  iso8859_conv_new( convert_to_iso8859_1) ;
}

mkf_conv_t *
mkf_iso8859_2_conv_new(void)
{
	return  iso8859_conv_new( convert_to_iso8859_2) ;
}

mkf_conv_t *
mkf_iso8859_3_conv_new(void)
{
	return  iso8859_conv_new( convert_to_iso8859_3) ;
}

mkf_conv_t *
mkf_iso8859_4_conv_new(void)
{
	return  iso8859_conv_new( convert_to_iso8859_4) ;
}

mkf_conv_t *
mkf_iso8859_5_conv_new(void)
{
	return  iso8859_conv_new( convert_to_iso8859_5) ;
}

mkf_conv_t *
mkf_iso8859_6_conv_new(void)
{
	return  iso8859_conv_new( convert_to_iso8859_6) ;
}

mkf_conv_t *
mkf_iso8859_7_conv_new(void)
{
	return  iso8859_conv_new( convert_to_iso8859_7) ;
}

mkf_conv_t *
mkf_iso8859_8_conv_new(void)
{
	return  iso8859_conv_new( convert_to_iso8859_8) ;
}

mkf_conv_t *
mkf_iso8859_9_conv_new(void)
{
	return  iso8859_conv_new( convert_to_iso8859_9) ;
}

mkf_conv_t *
mkf_iso8859_10_conv_new(void)
{
	return  iso8859_conv_new( convert_to_iso8859_10) ;
}

mkf_conv_t *
mkf_tis620_2533_conv_new(void)
{
	return  iso8859_conv_new( convert_to_tis620_2533) ;
}

mkf_conv_t *
mkf_iso8859_13_conv_new(void)
{
	return  iso8859_conv_new( convert_to_iso8859_13) ;
}

mkf_conv_t *
mkf_iso8859_14_conv_new(void)
{
	return  iso8859_conv_new( convert_to_iso8859_14) ;
}

mkf_conv_t *
mkf_iso8859_15_conv_new(void)
{
	return  iso8859_conv_new( convert_to_iso8859_15) ;
}

mkf_conv_t *
mkf_iso8859_16_conv_new(void)
{
	return  iso8859_conv_new( convert_to_iso8859_16) ;
}

mkf_conv_t *
mkf_tcvn5712_3_1993_conv_new(void)
{
	return  iso8859_conv_new( convert_to_tcvn5712_3_1993) ;
}
