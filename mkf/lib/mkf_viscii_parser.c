/*
 *	$Id$
 */

#include  "mkf_viscii_parser.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>


/* --- static functions --- */

static int
viscii_parser_next_char(
	mkf_parser_t *  viscii_parser ,
	mkf_char_t *  ch
	)
{
	u_char c ;
	
	if( viscii_parser->is_eos)
	{
		return  0 ;
	}

	mkf_parser_mark( viscii_parser) ;

	c = *viscii_parser->str ;

	if( (/* 0x0 <= c && */ c <= 0x1f && c != 0x02 && c != 0x05 && c != 0x06 &&
		c != 0x14 && c != 0x19 && c != 0x1e) ||
		c == 0x7f)
	{
		/* C0 */

		ch->cs = US_ASCII ;
	}
	else
	{
		ch->cs = VISCII ;
	}
	
	ch->ch[0] = c ;
	ch->size = 1 ;
	ch->property = 0 ;

	mkf_parser_increment( viscii_parser) ;
	
	return  1 ;
}

static void
viscii_parser_set_str(
	mkf_parser_t *  viscii_parser ,
	u_char *  str ,
	size_t  size
	)
{
	viscii_parser->str = str ;
	viscii_parser->left = size ;
	viscii_parser->marked_left = 0 ;
	viscii_parser->is_eos = 0 ;
}

static void
viscii_parser_delete(
	mkf_parser_t *  s
	)
{
	free( s) ;
}


/* --- global functions --- */

mkf_parser_t *
mkf_viscii_parser_new(void)
{
	mkf_parser_t *  viscii_parser ;
	
	if( ( viscii_parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( viscii_parser) ;

	viscii_parser->init = mkf_parser_init ;
	viscii_parser->next_char = viscii_parser_next_char ;
	viscii_parser->set_str = viscii_parser_set_str ;
	viscii_parser->delete = viscii_parser_delete ;

	return  viscii_parser ;
}
