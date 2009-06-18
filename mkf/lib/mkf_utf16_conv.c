/*
 *	$Id$
 */

#include  "mkf_utf16_conv.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_ucs4_map.h"


typedef struct  mkf_utf16_conv
{
	mkf_conv_t  conv ;
	int  is_bof ;		/* beginning of file */

}  mkf_utf16_conv_t ;


/* --- static functions --- */

static size_t
convert_to_utf16(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	mkf_utf16_conv_t *  utf16_conv ;
	size_t  filled_size ;
	mkf_char_t  ch ;

	utf16_conv = (mkf_utf16_conv_t*) conv ;

	filled_size = 0 ;

	if( utf16_conv->is_bof)
	{
		if( dst_size < 2)
		{
			return  0 ;
		}

		/*
		 * mark big endian
		 */

		*(dst ++) = 0xfe ;
		*(dst ++) = 0xff ;

		filled_size += 2 ;

		utf16_conv->is_bof = 0 ;
	}
	
	while( 1)
	{
		if( ! mkf_parser_next_char( parser , &ch))
		{
			return  filled_size ;
		}
		
		if( ch.cs == ISO10646_UCS2_1)
		{
			if( filled_size + 2 > dst_size)
			{
				mkf_parser_reset( parser) ;

				return  filled_size ;
			}

			(*dst++) = ch.ch[0] ;
			(*dst++) = ch.ch[1] ;
			
			filled_size += 2 ;
		}
		else
		{
			if( ch.cs != ISO10646_UCS4_1)
			{
				mkf_char_t  ucs4_ch ;

				if( mkf_map_to_ucs4( &ucs4_ch , &ch))
				{
					ch = ucs4_ch ;
				}
			}
			
			if( ch.cs != ISO10646_UCS4_1 || ch.ch[0] > 0x0 || ch.ch[1] > 0x1)
			{
				if( conv->illegal_char)
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
			else if( ch.ch[1] == 0x0)
			{
				/* BMP */
				
				if( filled_size + 2 > dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				(*dst++) = ch.ch[2] ;
				(*dst++) = ch.ch[3] ;

				filled_size += 2 ;
			}
			else /* if( ch.ch[1] == 0x1) */
			{
				/* surrogate pair */

				u_int32_t  linear ;
				u_char  c ;
				
				if( filled_size + 4 > dst_size)
				{
					mkf_parser_reset( parser) ;

					return  filled_size ;
				}

				linear = mkf_bytes_to_int( ch.ch , 4) - 0x10000 ;

				c = (u_char)( linear / (0x100 * 0x400)) ;
				linear -= (c * 0x100 * 0x400) ;
				(*dst++) = c + 0xd8 ;

				c = (u_char)( linear / 0x400) ;
				linear -= (c * 0x400) ;
				(*dst++) = c ;

				c = (u_char)( linear / 0x100) ;
				linear -= (c * 0x100) ;
				(*dst++) = c + 0xdc ;

				(*dst++) = (u_char) linear ;

				filled_size += 4 ;
			}
		}
	}
}

static size_t
convert_to_utf16le(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	size_t  size ;
	int  count ;

	if( ( size = convert_to_utf16( conv, dst, dst_size, parser)) == 0)
	{
		return  0 ;
	}

	for( count = 0 ; count < size - 1 ; count += 2)
	{
		u_char  c ;
		
		c = dst[count] ;
		dst[count] = dst[count+1] ;
		dst[count+1] = c ;
	}

	return  size ;
}

static void
conv_init(
	mkf_conv_t *  conv
	)
{
	mkf_utf16_conv_t *  utf16_conv ;

	utf16_conv = (mkf_utf16_conv_t*) conv ;

	utf16_conv->is_bof = 1 ;
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
mkf_utf16_conv_new(void)
{
	mkf_utf16_conv_t *  utf16_conv ;

	if( ( utf16_conv = malloc( sizeof( mkf_utf16_conv_t))) == NULL)
	{
		return  NULL ;
	}

	utf16_conv->conv.convert = convert_to_utf16 ;
	utf16_conv->conv.init = conv_init ;
	utf16_conv->conv.delete = conv_delete ;
	utf16_conv->conv.illegal_char = NULL ;
	
	utf16_conv->is_bof = 1 ;

	return  (mkf_conv_t*)utf16_conv ;
}

mkf_conv_t *
mkf_utf16le_conv_new(void)
{
	mkf_utf16_conv_t *  utf16_conv ;

	if( ( utf16_conv = malloc( sizeof( mkf_utf16_conv_t))) == NULL)
	{
		return  NULL ;
	}

	utf16_conv->conv.convert = convert_to_utf16le ;
	utf16_conv->conv.init = conv_init ;
	utf16_conv->conv.delete = conv_delete ;
	utf16_conv->conv.illegal_char = NULL ;
	
	utf16_conv->is_bof = 1 ;

	return  (mkf_conv_t*)utf16_conv ;
}
