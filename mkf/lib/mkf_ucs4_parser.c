/*
 *	$Id$
 */

#include  "mkf_ucs4_parser.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>

#include  "mkf_ucs_property.h"


typedef struct  mkf_ucs4_parser
{
	mkf_parser_t  parser ;
	int  is_big_endian ;

} mkf_ucs4_parser_t ;


/* --- static functions --- */

static void
ucs4_parser_init(
	mkf_parser_t *  parser
	)
{
	mkf_parser_init( parser) ;

	((mkf_ucs4_parser_t*)parser)->is_big_endian = 1 ;
}

static void
ucs4le_parser_init(
	mkf_parser_t *  parser
	)
{
	mkf_parser_init( parser) ;

	((mkf_ucs4_parser_t*)parser)->is_big_endian = 0 ;
}

static void
ucs4_parser_set_str(
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
ucs4_parser_delete(
	mkf_parser_t *  parser
	)
{
	free( parser) ;
}

static int
ucs4_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ucs4_ch
	)
{
	mkf_ucs4_parser_t *  ucs4_parser ;

	if( parser->is_eos)
	{
		return  0 ;
	}

	mkf_parser_mark( parser) ;
	
	ucs4_parser = (mkf_ucs4_parser_t*)parser ;
	
	if( parser->left < 4)
	{
		parser->is_eos = 1 ;
	
		return  0 ;
	}

	if( memcmp( parser->str , "\x00\x00\xfe\xff" , 4) == 0)
	{
		ucs4_parser->is_big_endian = 1 ;

		mkf_parser_n_increment( parser , 4) ;

		return  ucs4_parser_next_char( parser , ucs4_ch) ;
	}
	else if( memcmp( parser->str , "\xff\xfe\x00\x00" , 4) == 0)
	{
		ucs4_parser->is_big_endian = 0 ;
		
		mkf_parser_n_increment( parser , 4) ;

		return  ucs4_parser_next_char( parser , ucs4_ch) ;
	}
	else
	{
		if( ucs4_parser->is_big_endian)
		{
			memcpy( ucs4_ch->ch , parser->str , 4) ;
		}
		else
		{
			ucs4_ch->ch[0] = parser->str[3] ;
			ucs4_ch->ch[1] = parser->str[2] ;
			ucs4_ch->ch[2] = parser->str[1] ;
			ucs4_ch->ch[3] = parser->str[0] ;
		}

		mkf_parser_n_increment( parser , 4) ;

		ucs4_ch->cs = ISO10646_UCS4_1 ;
		ucs4_ch->size = 4 ;
		ucs4_ch->property = mkf_get_ucs_property( mkf_bytes_to_int( ucs4_ch->ch , ucs4_ch->size)) ;
		
		return  1 ;
	}
}


/* --- global functions --- */

mkf_parser_t *
mkf_ucs4_parser_new(void)
{
	mkf_ucs4_parser_t *  ucs4_parser ;

	if( ( ucs4_parser = malloc( sizeof( mkf_ucs4_parser_t))) == NULL)
	{
		return  NULL ;
	}
	
	ucs4_parser_init( (mkf_parser_t*) ucs4_parser) ;

	ucs4_parser->parser.init = ucs4_parser_init ;
	ucs4_parser->parser.set_str = ucs4_parser_set_str ;
	ucs4_parser->parser.delete = ucs4_parser_delete ;
	ucs4_parser->parser.next_char = ucs4_parser_next_char ;

	return  (mkf_parser_t*) ucs4_parser ;
}

mkf_parser_t *
mkf_utf32_parser_new(void)
{
	return  mkf_ucs4_parser_new() ;
}

mkf_parser_t *
mkf_ucs4le_parser_new(void)
{
	mkf_ucs4_parser_t *  ucs4_parser ;

	if( ( ucs4_parser = malloc( sizeof( mkf_ucs4_parser_t))) == NULL)
	{
		return  NULL ;
	}
	
	ucs4_parser_init( (mkf_parser_t*) ucs4_parser) ;

	ucs4_parser->parser.init = ucs4le_parser_init ;
	ucs4_parser->parser.set_str = ucs4_parser_set_str ;
	ucs4_parser->parser.delete = ucs4_parser_delete ;
	ucs4_parser->parser.next_char = ucs4_parser_next_char ;

	return  (mkf_parser_t*) ucs4_parser ;
}

mkf_parser_t *
mkf_utf32le_parser_new(void)
{
	return  mkf_ucs4le_parser_new() ;
}
