/*
 *	$Id$
 */

#include  "mkf_iscii_parser.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>


/* --- static functions --- */

static int
iscii_parser_next_char(
	mkf_parser_t *  iscii_parser ,
	mkf_char_t *  ch
	)
{
	if( iscii_parser->is_eos)
	{
		return  0 ;
	}

	mkf_parser_mark( iscii_parser) ;
	
	if( /* 0x0 <= *iscii_parser->str && */ *iscii_parser->str <= 0x7f)
	{
		ch->ch[0] = *iscii_parser->str ;
		ch->size = 1 ;
		ch->cs = US_ASCII ;
	}
	else if( 0xa1 <= *iscii_parser->str)
	{
		ch->ch[0] = *iscii_parser->str ;
		ch->size = 1 ;
		ch->cs = ISCII ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " iscii doesn't support %x char.\n" ,
			*iscii_parser->str) ;
	#endif

		return  0 ;
	}

	ch->property = 0 ;

	mkf_parser_increment( iscii_parser) ;
	
	return  1 ;
}

static void
iscii_parser_set_str(
	mkf_parser_t *  iscii_parser ,
	u_char *  str ,
	size_t  size
	)
{
	iscii_parser->str = str ;
	iscii_parser->left = size ;
	iscii_parser->marked_left = 0 ;
	iscii_parser->is_eos = 0 ;
}

static void
iscii_parser_delete(
	mkf_parser_t *  s
	)
{
	free( s) ;
}


/* --- global functions --- */

mkf_parser_t *
mkf_iscii_parser_new(void)
{
	mkf_parser_t *  iscii_parser ;
	
	if( ( iscii_parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( iscii_parser) ;

	iscii_parser->init = mkf_parser_init ;
	iscii_parser->next_char = iscii_parser_next_char ;
	iscii_parser->set_str = iscii_parser_set_str ;
	iscii_parser->delete = iscii_parser_delete ;

	return  iscii_parser ;
}
