/*
 *	$Id$
 */

#include  "mkf_iso2022cn_conv.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_iso2022_intern.h"
#include  "mkf_zh_cn_map.h"
#include  "mkf_zh_tw_map.h"


typedef struct  mkf_iso2022cn_conv
{
	mkf_conv_t   conv ;
	mkf_charset_t  cur_cs ;
	mkf_charset_t  cur_g1 ;
	mkf_charset_t  cur_g2 ;	

} mkf_iso2022cn_conv_t ;


/* --- static functions --- */

static void
remap_unsupported_charset(
	mkf_char_t *  ch
	)
{
	mkf_char_t  c ;

	if( ch->cs == ISO10646_UCS4_1)
	{
		if( mkf_map_ucs4_to_zh_cn( &c , ch))
		{
			*ch = c ;
		}
		else if( mkf_map_ucs4_to_zh_tw( &c , ch))
		{
			*ch = c ;
		}
		else
		{
			return ;
		}
	}
	
	if( ch->cs == BIG5)
	{
		if( mkf_map_big5_to_cns11643_1992( &c , ch))
		{
			*ch = c ;
		}
	}
	else if( ch->cs == GBK)
	{
		if( mkf_map_gbk_to_gb2312_80( &c , ch))
		{
			*ch = c ;
		}
	}
}

static size_t
convert_to_iso2022cn(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	mkf_iso2022cn_conv_t *  iso2022cn_conv ;
	size_t  filled_size ;
	mkf_char_t  ch ;

	iso2022cn_conv = (mkf_iso2022cn_conv_t*) conv ;

	filled_size = 0 ;
	
	while( 1)
	{
		int  counter ;

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
		
		if( ch.cs == iso2022cn_conv->cur_cs)
		{
			if( filled_size + ch.size > dst_size)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}
			
			if( ch.cs == US_ASCII && ch.ch[0] == '\n')
			{
				/* reset */
				
				iso2022cn_conv->cur_g1 = UNKNOWN_CS ;
				iso2022cn_conv->cur_g2 = UNKNOWN_CS ;
			}
		}
		else if( ch.cs == CNS11643_1992_2)
		{
			if( iso2022cn_conv->cur_g2 != CNS11643_1992_2)
			{
				if( filled_size + ch.size + 5 >= dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = ESC ;
				*(dst ++) = MB_CS ;
				*(dst ++) = CS94_TO_G2 ;
				*(dst ++) = CS94MB_FT(CNS11643_1992_2) ;
				*(dst ++) = ESC ;
				*(dst ++) = SS2_7 ;

				filled_size += 6 ;

				iso2022cn_conv->cur_g2 = CNS11643_1992_2 ;
			}
			else
			{
				if( filled_size + ch.size + 1 >= dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = ESC ;
				*(dst ++) = SS2_7 ;

				filled_size += 2 ;
			}
			
			/* this is single shifted , so iso2022cn_conv->cur_cs is not changed */
		}
		else
		{
			if( ch.cs == CNS11643_1992_1 || ch.cs == GB2312_80)
			{
				if( iso2022cn_conv->cur_g1 != ch.cs)
				{
					if( filled_size + ch.size + 4 >= dst_size)
					{
						mkf_parser_reset( parser) ;

						return  filled_size ;
					}

					*(dst ++) = ESC ;
					*(dst ++) = MB_CS ;
					*(dst ++) = CS94_TO_G1 ;
					*(dst ++) = CS94MB_FT(ch.cs) ;
					*(dst ++) = LS1 ;

					filled_size += 5 ;

					iso2022cn_conv->cur_g1 = ch.cs ;
				}
				else
				{
					if( filled_size + ch.size >= dst_size)
					{
						mkf_parser_reset( parser) ;

						return  filled_size ;
					}

					*(dst ++) = LS1 ;
					
					filled_size ++ ;
				}
			}
			else if( ch.cs == US_ASCII)
			{
				if( filled_size + ch.size >= dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = LS0 ;

				filled_size ++ ;
				
				if( ch.ch[0] == '\n')
				{
					/* reset */
					iso2022cn_conv->cur_g1 = UNKNOWN_CS ;
					iso2022cn_conv->cur_g2 = UNKNOWN_CS ;
				}
			}
			else
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" cs(%x) is not supported by iso2022cn. char(%x) is discarded.\n" ,
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
			
			iso2022cn_conv->cur_cs = ch.cs ;
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
	mkf_iso2022cn_conv_t *  iso2022cn_conv ;

	iso2022cn_conv = (mkf_iso2022cn_conv_t*) conv ;
	
	iso2022cn_conv->cur_cs = US_ASCII ;
	iso2022cn_conv->cur_g1 = UNKNOWN_CS ;
	iso2022cn_conv->cur_g2 = UNKNOWN_CS ;
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
mkf_iso2022cn_conv_new(void)
{
	mkf_iso2022cn_conv_t *  iso2022cn_conv ;

	if( ( iso2022cn_conv = malloc( sizeof( mkf_iso2022cn_conv_t))) == NULL)
	{
		return  NULL ;
	}

	iso2022cn_conv->conv.convert = convert_to_iso2022cn ;
	iso2022cn_conv->conv.init = conv_init ;
	iso2022cn_conv->conv.delete = conv_delete ;
	
	iso2022cn_conv->cur_cs = US_ASCII ;
	iso2022cn_conv->cur_g1 = UNKNOWN_CS ;
	iso2022cn_conv->cur_g2 = UNKNOWN_CS ;

	return  (mkf_conv_t*)iso2022cn_conv ;
}
