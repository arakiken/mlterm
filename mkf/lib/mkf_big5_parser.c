/*
 *	$Id$
 */

#include  "mkf_big5_parser.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>


/*
 * the same macro is defined in mkf_ucs4_big5.c
 */
#define  IS_HKSCS(code) \
	( (0x8140 <= (code) && (code) <= 0xa0fe) || \
	  (0xc6a1 <= (code) && (code) <= 0xc8fe) || \
	  (0xf9d6 <= (code) && (code) <= 0xfefe) )


/* --- static functions --- */

static int
big5_parser_next_char_intern(
	mkf_parser_t *  big5_parser ,
	mkf_char_t *  ch ,
	int  use_hkscs
	)
{	
	if( big5_parser->is_eos)
	{
		return  0 ;
	}

	mkf_parser_mark( big5_parser) ;

	if( /* 0x0 <= *big5_parser->str && */ *big5_parser->str <= 0x7f)
	{
		ch->ch[0] = *big5_parser->str ;
		ch->size = 1 ;
		ch->cs = US_ASCII ;
	}
	/*
	 * 8140-a0fe is user defined area.
	 */
	else if( 0x81 <= *big5_parser->str && *big5_parser->str <= 0xfe)
	{
		u_int16_t  code ;
		
		ch->ch[0] = *big5_parser->str ;

		if( mkf_parser_increment( big5_parser) == 0)
		{
			mkf_parser_reset( big5_parser) ;

			return  0 ;
		}

		if( ( 0x40 <= *big5_parser->str && *big5_parser->str <= 0x7e) ||
			( 0xa1 <= *big5_parser->str && *big5_parser->str <= 0xfe))
		{
			ch->ch[1] = *big5_parser->str ;
		}
		else
		{
			goto  error ;
		}

		code = mkf_bytes_to_int( ch->ch , 2) ;

		if( use_hkscs && IS_HKSCS(code))
		{
			ch->cs = HKSCS ;
		}
		else
		{
			ch->cs = BIG5 ;
		}
		
		ch->size = 2 ;
	}
	else
	{
		goto  error ;
	}

	ch->property = 0 ;

	mkf_parser_increment( big5_parser) ;
	
	return  1 ;

error:
	mkf_parser_increment( big5_parser) ;

	return  0 ;
}

static int
big5_parser_next_char(
	mkf_parser_t *  big5_parser ,
	mkf_char_t *  ch
	)
{
	return  big5_parser_next_char_intern( big5_parser , ch , 0) ;
}

static int
big5hkscs_parser_next_char(
	mkf_parser_t *  big5_parser ,
	mkf_char_t *  ch
	)
{
	return  big5_parser_next_char_intern( big5_parser , ch , 1) ;
}

static void
big5_parser_set_str(
	mkf_parser_t *  big5_parser ,
	u_char *  str ,
	size_t  size
	)
{
	big5_parser->str = str ;
	big5_parser->left = size ;
	big5_parser->marked_left = 0 ;
	big5_parser->is_eos = 0 ;
}

static void
big5_parser_delete(
	mkf_parser_t *  s
	)
{
	free( s) ;
}


/* --- global functions --- */

mkf_parser_t *
mkf_big5_parser_new(void)
{
	mkf_parser_t *  big5_parser ;
	
	if( ( big5_parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( big5_parser) ;

	big5_parser->init = mkf_parser_init ;
	big5_parser->next_char = big5_parser_next_char ;
	big5_parser->set_str = big5_parser_set_str ;
	big5_parser->delete = big5_parser_delete ;

	return  big5_parser ;
}

mkf_parser_t *
mkf_big5hkscs_parser_new(void)
{
	mkf_parser_t *  big5_parser ;
	
	if( ( big5_parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( big5_parser) ;

	big5_parser->init = mkf_parser_init ;
	big5_parser->next_char = big5hkscs_parser_next_char ;
	big5_parser->set_str = big5_parser_set_str ;
	big5_parser->delete = big5_parser_delete ;

	return  big5_parser ;
}
