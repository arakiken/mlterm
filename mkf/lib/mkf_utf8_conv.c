/*
 *	$Id$
 */

#include  "mkf_utf8_conv.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_ucs4_map.h"


/* --- static functions --- */

static void
remap_unsupported_charset(
	mkf_char_t *  ch
	)
{
	mkf_char_t  c ;

	if( ch->cs != ISO10646_UCS4_1 && ch->cs != ISO10646_UCS2_1)
	{
		if( mkf_map_to_ucs4( &c , ch))
		{
			*ch = c ;
		}
	}
}

static size_t
convert_to_utf8(
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
		
		/*
		 * utf8 encoding
		 */
		if( ch.cs == ISO10646_UCS4_1 || ch.cs == ISO10646_UCS2_1)
		{
			u_int32_t  ucs_ch ;

			if( ch.cs == ISO10646_UCS4_1)
			{
				/* UCS4 */
				
				ucs_ch = ((ch.ch[0] << 24) & 0xff000000) +
					((ch.ch[1] << 16) & 0x00ff0000) +
					((ch.ch[2] << 8) & 0x0000ff00) +
					(ch.ch[3] & 0x000000ff) ;
			}
			else
			{
				/* UCS2 */
				
				ucs_ch = ((ch.ch[0] << 8) & 0xff00) +
					(ch.ch[1] & 0x00ff) ;
			}
			
			if( 0x00 <= ucs_ch && ucs_ch <= 0x7f)
			{
				/* encoded to 8 bit */
				
				if( filled_size + 1 > dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = ucs_ch ;
				filled_size ++ ;
			}
			else if( 0x80 <= ucs_ch && ucs_ch <= 0x07ff)
			{
				/* encoded to 16bit */

				if( filled_size + 2 > dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = ((ucs_ch >> 6) & 0xff) | 0xc0 ;
				*(dst ++) = (ucs_ch & 0x3f) | 0x80 ;
				filled_size += 2 ;
			}
			else if( 0x0800 <= ucs_ch && ucs_ch <= 0xffff)
			{
				if( filled_size + 3 > dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = ((ucs_ch >> 12) & 0x0f) | 0xe0 ;
				*(dst ++) = ((ucs_ch >> 6) & 0x3f) | 0x80 ;
				*(dst ++) = (ucs_ch & 0x3f) | 0x80 ;
				filled_size += 3 ;
			}
			else if( 0x010000 <= ucs_ch && ucs_ch <= 0x1fffff)
			{
				if( filled_size + 4 > dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = ((ucs_ch >> 18) & 0x07) | 0xf0 ;
				*(dst ++) = ((ucs_ch >> 12) & 0x3f) | 0x80 ;
				*(dst ++) = ((ucs_ch >> 6) & 0x3f) | 0x80 ;
				*(dst ++) = (ucs_ch & 0x3f) | 0x80 ;
				filled_size += 4 ;
			}
			else if( 0x200000 <= ucs_ch && ucs_ch <= 0x03ffffff)
			{
				if( filled_size + 5 > dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = ((ucs_ch >> 24) & 0x03) | 0xf8 ;
				*(dst ++) = ((ucs_ch >> 18) & 0x3f) | 0x80 ;
				*(dst ++) = ((ucs_ch >> 12) & 0x3f) | 0x80 ;
				*(dst ++) = ((ucs_ch >> 6) & 0x3f) | 0x80 ;
				*(dst ++) = (ucs_ch & 0x3f) | 0x80 ;
				filled_size += 5 ;
			}
			else if( 0x04000000 <= ucs_ch && ucs_ch <= 0x7fffffff)
			{
				if( filled_size + 6 > dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				*(dst ++) = ((ucs_ch >> 30) & 0x01) | 0xfc ;
				*(dst ++) = ((ucs_ch >> 24) & 0x3f) | 0x80 ;
				*(dst ++) = ((ucs_ch >> 18) & 0x3f) | 0x80 ;
				*(dst ++) = ((ucs_ch >> 12) & 0x3f) | 0x80 ;
				*(dst ++) = ((ucs_ch >> 6) & 0x3f) | 0x80 ;
				*(dst ++) = (ucs_ch & 0x3f) | 0x80 ;
				filled_size += 6 ;
			}
			else
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " strange ucs4 character %x\n" , ucs_ch) ;
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
mkf_utf8_conv_new(void)
{
	mkf_conv_t *  conv ;

	if( ( conv = malloc( sizeof( mkf_conv_t))) == NULL)
	{
		return  NULL ;
	}

	conv->convert = convert_to_utf8 ;
	conv->init = conv_init ;
	conv->delete = conv_delete ;
	conv->illegal_char = NULL ;

	return  conv ;
}
