/*
 *	$Id$
 */

#include  "ml_str_parser.h"

#include  <string.h>			/* memcpy */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "ml_char_encoding.h"		/* ml_is_msb_set */


typedef struct ml_str_parser
{
	mkf_parser_t  parser ;

	ml_char_t *  str ;
	u_int  left ;
	u_int  comb_left ;

} ml_str_parser_t ;


/* --- static functions --- */

static int
next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	ml_str_parser_t *  ml_str_parser ;
	ml_char_t *  ml_ch ;
	u_int  comb_size ;

	ml_str_parser = (ml_str_parser_t*) parser ;

	while( 1)
	{
		if( ml_str_parser->parser.is_eos)
		{
			return  0 ;
		}

		/*
		 * skipping NULL
		 */
		if( ! ml_char_is_null( ml_str_parser->str))
		{
			break ;
		}

		ml_str_parser->left -- ;
		ml_str_parser->str ++ ;

		if( ml_str_parser->left == 0)
		{
			ml_str_parser->parser.is_eos = 1 ;
		}
	}

	ml_ch = ml_str_parser->str ;
	
	if( ml_str_parser->comb_left > 0)
	{
		ml_char_t *  combs ;
		
		if( ( combs = ml_get_combining_chars( ml_ch , &comb_size)) == NULL ||
			comb_size < ml_str_parser->comb_left)
		{
			/* strange ! */
			
			ml_str_parser->comb_left = 0 ;

			return  0 ;
		}
		
		ml_ch = &combs[ comb_size - ml_str_parser->comb_left] ;
		
		if( -- ml_str_parser->comb_left == 0)
		{
			ml_str_parser->left -- ;
			ml_str_parser->str ++ ;
		}
	}
	else
	{
		if( ml_get_combining_chars( ml_ch , &comb_size))
		{
			ml_str_parser->comb_left = comb_size ;
		}
		else
		{
			ml_str_parser->left -- ;
			ml_str_parser->str ++ ;
		}
	}
	
	ch->size = ml_char_size( ml_ch) ;
	memcpy( ch->ch , ml_char_bytes( ml_ch) , ch->size) ;
	ch->cs = ml_char_cs( ml_ch) ;

	if( ml_is_msb_set( ch->cs))
	{
		UNSET_MSB(ch->ch[0]) ;
	}
	
	/* XXX */
	ch->property = 0 ;

	if( ch->cs == ISO10646_UCS2_1)
	{
		ch->ch[2] = ch->ch[0] ;
		ch->ch[3] = ch->ch[1] ;
		ch->ch[0] = 0x0 ;
		ch->ch[1] = 0x0 ;
		ch->cs = ISO10646_UCS4_1 ;
		ch->size = 4 ;
	}
	
	if( ml_str_parser->left == 0)
	{
		ml_str_parser->parser.is_eos = 1 ;
	}

	return  1 ;
}

static void
init(
	mkf_parser_t *  mkf_parser
	)
{
	ml_str_parser_t *  ml_str_parser ;

	ml_str_parser = (ml_str_parser_t*) mkf_parser ;

	ml_str_parser->parser.str = NULL ;
	ml_str_parser->parser.marked_left = 0 ;
	ml_str_parser->parser.left = 0 ;
	ml_str_parser->parser.is_eos = 0 ;

	ml_str_parser->str = NULL ;
	ml_str_parser->left = 0 ;
}

static void
set_str(
	mkf_parser_t *  mkf_parser ,
	u_char *  str ,
	size_t  size
	)
{
	/* do nothing */
}

static void
delete(
	mkf_parser_t *  s
	)
{
	free( s) ;
}


/* --- global functions --- */

mkf_parser_t *
ml_str_parser_new(void)
{
	ml_str_parser_t *  ml_str_parser ;
	
	if( ( ml_str_parser = malloc( sizeof( ml_str_parser_t))) == NULL)
	{
		return  NULL ;
	}

	init( (mkf_parser_t*) ml_str_parser) ;

	ml_str_parser->parser.init = init ;
	ml_str_parser->parser.set_str = set_str ;
	ml_str_parser->parser.delete = delete ;
	ml_str_parser->parser.next_char = next_char ;

	return  (mkf_parser_t*)ml_str_parser ;
}

void
ml_str_parser_set_str(
	mkf_parser_t *  mkf_parser ,
	ml_char_t *  str ,
	u_int  size
	)
{
	ml_str_parser_t *  ml_str_parser ;

	ml_str_parser = (ml_str_parser_t*) mkf_parser ;

	ml_str_parser->parser.is_eos = 0 ;
	
	ml_str_parser->str = str ;
	ml_str_parser->left = size ;
}
