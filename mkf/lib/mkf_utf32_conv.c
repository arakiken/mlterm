/*
 *	$Id$
 */

#include  "mkf_utf32_conv.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>

#include  "mkf_ucs4_map.h"


typedef struct  mkf_utf32_conv
{
	mkf_conv_t  conv ;
	int  is_bof ;		/* beginning of file */

}  mkf_utf32_conv_t ;


/* --- static functions --- */

static size_t
convert_to_utf32(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	mkf_utf32_conv_t *  utf32_conv ;
	size_t  filled_size ;
	mkf_char_t  ch ;

	utf32_conv = (mkf_utf32_conv_t*) conv ;

	filled_size = 0 ;

	if( utf32_conv->is_bof)
	{
		if( dst_size < 4)
		{
			return  0 ;
		}

		/*
		 * mark big endian
		 */

		*(dst ++) = 0x0 ;
		*(dst ++) = 0x0 ;
		*(dst ++) = 0xfe ;
		*(dst ++) = 0xff ;

		filled_size += 4 ;

		utf32_conv->is_bof = 0 ;
	}
	
	while( filled_size + 4 <= dst_size)
	{
		if( ! mkf_parser_next_char( parser , &ch))
		{
			return  filled_size ;
		}

		if( ch.cs == US_ASCII)
		{
			dst[0] = 0x0 ;
			dst[1] = 0x0 ;
			dst[2] = 0x0 ;
			dst[3] = ch.ch[0] ;
		}
	#if  0
		else if( ch.cs == ISO10646_UCS2_1)
		{
			dst[0] = 0x0 ;
			dst[1] = 0x0 ;
			dst[2] = ch.ch[0] ;
			dst[3] = ch.ch[1] ;
		}
	#endif
		else if( ch.cs == ISO10646_UCS4_1)
		{
			dst[0] = ch.ch[0] ;
			dst[1] = ch.ch[1] ;
			dst[2] = ch.ch[2] ;
			dst[3] = ch.ch[3] ;
		}
		else
		{
			mkf_char_t  ucs4_ch ;
			
			if( mkf_map_to_ucs4( &ucs4_ch , &ch))
			{
				memcpy( dst , ucs4_ch.ch , 4) ;
			}
			else if( conv->illegal_char)
			{
				size_t  size ;
				int  is_full ;

				size = (*conv->illegal_char)( conv , dst , dst_size - filled_size ,
						&is_full , &ch) ;
				if( is_full)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				dst += size ;
				filled_size += size ;
			}
		}
		
		dst += 4 ;
		filled_size += 4 ;
	}

	return  filled_size ;
}

static void
conv_init(
	mkf_conv_t *  conv
	)
{
	mkf_utf32_conv_t *  utf32_conv ;

	utf32_conv = (mkf_utf32_conv_t*) conv ;

	utf32_conv->is_bof = 1 ;
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
mkf_utf32_conv_new(void)
{
	mkf_utf32_conv_t *  utf32_conv ;

	if( ( utf32_conv = malloc( sizeof( mkf_utf32_conv_t))) == NULL)
	{
		return  NULL ;
	}

	utf32_conv->conv.convert = convert_to_utf32 ;
	utf32_conv->conv.init = conv_init ;
	utf32_conv->conv.delete = conv_delete ;
	utf32_conv->conv.illegal_char = NULL ;
	
	utf32_conv->is_bof = 1 ;

	return  (mkf_conv_t*)utf32_conv ;
}
