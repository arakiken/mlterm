/*
 *	$Id$
 */

#include  "mkf_utf16_parser.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_str.h>

#include  "mkf_ucs_property.h"


typedef struct  mkf_utf16_parser
{
	mkf_parser_t  parser ;
	int  is_big_endian ;
	
} mkf_utf16_parser_t ;


/* --- static functions --- */

static void
utf16_parser_init(
	mkf_parser_t *  parser
	)
{
	mkf_parser_init( parser) ;

	((mkf_utf16_parser_t*)parser)->is_big_endian = 1 ;
}

static void
utf16le_parser_init(
	mkf_parser_t *  parser
	)
{
	mkf_parser_init( parser) ;

	((mkf_utf16_parser_t*)parser)->is_big_endian = 0 ;
}

static void
utf16_parser_set_str(
	mkf_parser_t *  parser ,
	u_char *  str ,
	size_t  size
	)
{
	parser->str = str ;
	parser->left = size ;
	parser->marked_left = 0 ;
	parser->is_eos = 0 ;
}

static void
utf16_parser_delete(
	mkf_parser_t *  parser
	)
{
	free( parser) ;
}

static int
utf16_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ucs4_ch
	)
{
	mkf_utf16_parser_t *  utf16_parser ;

	if( parser->is_eos)
	{
		return  0 ;
	}

	mkf_parser_mark( parser) ;

	if( parser->left < 2)
	{
		parser->is_eos = 1 ;

		return  0 ;
	}

	utf16_parser = (mkf_utf16_parser_t*) parser ;

	if( memcmp( parser->str , "\xfe\xff" , 2) == 0)
	{
		utf16_parser->is_big_endian = 1 ;

		mkf_parser_n_increment( parser , 2) ;

		return  utf16_parser_next_char( parser , ucs4_ch) ;
	}
	else if( memcmp( parser->str , "\xff\xfe" , 2) == 0)
	{
		utf16_parser->is_big_endian = 0 ;

		mkf_parser_n_increment( parser , 2) ;

		return  utf16_parser_next_char( parser , ucs4_ch) ;
	}
	else
	{
		u_char  ch[2] ;
			
		if( utf16_parser->is_big_endian)
		{
			ch[0] = parser->str[0] ;
			ch[1] = parser->str[1] ;
		}
		else
		{
			ch[0] = parser->str[1] ;
			ch[1] = parser->str[0] ;
		}

		if( 0xd8 <= ch[0] && ch[0] <= 0xdb)
		{
			/* surrogate pair */

			u_char  ch2[2] ;
			u_int32_t  ucs4 ;

			if( parser->left < 4)
			{
				parser->is_eos = 1 ;

				return  0 ;
			}

			if( utf16_parser->is_big_endian)
			{
				ch2[0] = parser->str[2] ;
				ch2[1] = parser->str[3] ;
			}
			else
			{
				ch2[0] = parser->str[3] ;
				ch2[1] = parser->str[2] ;
			}

			if( ch2[0] < 0xdc || 0xdf < ch2[0])
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " illegal UTF-16 surrogate-pair format.\n") ;
			#endif
			
				mkf_parser_n_increment( parser , 4) ;
				
				return  0 ;
			}

			ucs4 = ( (ch[0] - 0xd8) * 0x100 * 0x400 + ch[1] * 0x400 +
					(ch2[0] - 0xdc) * 0x100 + ch2[1] ) + 0x10000 ;

		#ifdef  DEBUG
			if( ucs4 < 0x10000 || 0x1fffff < ucs4)
			{
				kik_warn_printf( KIK_DEBUG_TAG " illegal UTF-16 surrogate-pair format.\n") ;

				mkf_parser_n_increment( parser , 4) ;
				
				return  0 ;
			}
		#endif

			mkf_int_to_bytes( ucs4_ch->ch , 4 , ucs4) ;
			
			mkf_parser_n_increment( parser , 4) ;
		}
		else
		{
			ucs4_ch->ch[0] = 0x0 ;
			ucs4_ch->ch[1] = 0x0 ;
			ucs4_ch->ch[2] = ch[0] ;
			ucs4_ch->ch[3] = ch[1] ;
			
			mkf_parser_n_increment( parser , 2) ;
		}

		ucs4_ch->cs = ISO10646_UCS4_1 ;
		ucs4_ch->size = 4 ;
		ucs4_ch->property = mkf_get_ucs_property( mkf_bytes_to_int( ucs4_ch->ch , ucs4_ch->size)) ;

		return  1 ;
	}
}


/* --- global functions --- */

mkf_parser_t *
mkf_utf16_parser_new(void)
{
	mkf_utf16_parser_t *  utf16_parser ;

	if( ( utf16_parser = malloc( sizeof( mkf_utf16_parser_t))) == NULL)
	{
		return  NULL ;
	}

	utf16_parser_init( ( mkf_parser_t*) utf16_parser) ;

	utf16_parser->parser.init = utf16_parser_init ;
	utf16_parser->parser.set_str = utf16_parser_set_str ;
	utf16_parser->parser.delete = utf16_parser_delete ;
	utf16_parser->parser.next_char = utf16_parser_next_char ;

	return  (mkf_parser_t*) utf16_parser ;
}

mkf_parser_t *
mkf_utf16le_parser_new(void)
{
	mkf_utf16_parser_t *  utf16_parser ;

	if( ( utf16_parser = malloc( sizeof( mkf_utf16_parser_t))) == NULL)
	{
		return  NULL ;
	}

	utf16_parser_init( ( mkf_parser_t*) utf16_parser) ;

	utf16_parser->parser.init = utf16le_parser_init ;
	utf16_parser->parser.set_str = utf16_parser_set_str ;
	utf16_parser->parser.delete = utf16_parser_delete ;
	utf16_parser->parser.next_char = utf16_parser_next_char ;

	return  (mkf_parser_t*) utf16_parser ;
}
