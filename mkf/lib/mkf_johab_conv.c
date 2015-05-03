/*
 *	$Id$
 */

#include  "mkf_johab_conv.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_ko_kr_map.h"

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

	if( ch->cs == ISO10646_UCS4_1)
	{
		if( ! mkf_map_ucs4_to_ko_kr( &c , ch))
		{
			return ;
		}
		
		*ch = c ;
	}

	/*
	 * once all korean characters are converted to UHC.
	 */
	if( ch->cs == KSC5601_1987)
	{
		if( mkf_map_ksc5601_1987_to_uhc( &c , ch))
		{
			*ch = c ;
		}
	}
	
	if( ch->cs == UHC)
	{
		/*
		 * converting hangul to johab.
		 */
		if( mkf_map_uhc_to_johab( &c , ch))
		{
			*ch = c ;
		}

		/*
		 * the rest may be chinese characters or so , and they all are converted
		 * to ksc5601.
		 */
		if( mkf_map_uhc_to_ksc5601_1987( &c , ch))
		{
			*ch = c ;
		}
	}
}

static size_t
convert_to_johab(
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

		if( ch.cs == JOHAB)
		{
			/* Hangul */
			
			if( filled_size + 1 >= dst_size)
			{
				mkf_parser_full_reset( parser) ;

				return  filled_size ;
			}

			*(dst ++) = ch.ch[0] ;
			*(dst ++) = ch.ch[1] ;

			filled_size += 2 ;
		}
		else if( ch.cs == KSC5601_1987)
		{
			/*
			 * not Hangul
			 * KSC5601_1987 Hangul chars are remapped to JOHAB in
			 * remap_unsupported_charset()
			 */

			u_char  byte1 ;
			u_char  byte2 ;
			
			if( filled_size + 1 >= dst_size)
			{
				mkf_parser_full_reset( parser) ;

				return  filled_size ;
			}
			
		#ifdef  __DEBUG
			kik_debug_printf( "0x%.2x%.2x -> " , ch.ch[0] , ch.ch[1]) ;
		#endif
		
			if( ch.ch[0] <= 0x2c)
			{
				if( ch.ch[0] % 2 == 1)
				{
					byte1 = (ch.ch[0] - 0x20 + 0x1b1) / 2 ;

					goto  pattern_1 ;
				}
				else
				{
					byte1 = (ch.ch[0] - 0x20 + 0x1b0) / 2 ;

					goto  pattern_2 ;
				}
			}
			else if( ch.ch[0] == 0x49)
			{
				byte1 = 0xd8 ;

				goto  pattern_1 ;
			}
			else if( ch.ch[0] == 0x7e)
			{
				byte1 = 0xd8 ;

				goto  pattern_2 ;
			}
			else if( 0x4a <= ch.ch[0] && ch.ch[0] <= 0x7d)
			{
				if( ch.ch[0] % 2 == 0)
				{
					byte1 = (ch.ch[0] - 0x20 + 0x196) / 2 ;

					goto  pattern_1 ;
				}
				else
				{
					byte1 = (ch.ch[0] - 0x20 + 0x195) / 2 ;

					goto  pattern_2 ;
				}
			}
			else
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" illegal johab format. cs(%x)/char(%x) is discarded.\n" ,
					ch.cs , mkf_char_to_int( &ch)) ;
			#endif

				kik_msg_printf( "convertion failed.\n") ;
				
				continue ;
			}

		pattern_1:
			if( ch.ch[1] <= 0x6e)
			{
				byte2 = ch.ch[1] - 0x20 + 0x30 ;
			}
			else
			{
				byte2 = ch.ch[1] - 0x20 + 0x42 ;
			}

			goto  encoded ;

		pattern_2:
			byte2 = ch.ch[1] - 0x20 + 0xa0 ;

		encoded:
		#ifdef  __DEBUG
			kik_debug_printf( "0x%.2x%.2x\n" , byte1 , byte2) ;
		#endif
		
			*(dst ++) = byte1 ;
			*(dst ++) = byte2 ;

			filled_size += 2 ;
		}
		else if( ch.cs == US_ASCII)
		{
			if( filled_size >= dst_size)
			{
				mkf_parser_full_reset( parser) ;

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
				mkf_parser_full_reset( parser) ;

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
mkf_johab_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_johab ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}
