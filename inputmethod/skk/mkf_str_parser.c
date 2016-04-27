/*
 *	$Id$
 */

#include  "mkf_str_parser.h"

#include  <stdio.h>


/* --- static functions --- */

static void
set_str(
	mkf_parser_t *  parser ,
	u_char *  str ,		/* mkf_char_t* */
	size_t  size
	)
{
	parser->str = str ;
	parser->left = size ;
	parser->marked_left = 0 ;
	parser->is_eos = 0 ;
}

static void
delete(
	mkf_parser_t *  parser
	)
{
}

static int
next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	if( parser->is_eos)
	{
		return  0 ;
	}

	*ch = ((mkf_char_t*)parser->str)[0] ;
	mkf_parser_n_increment( parser , sizeof(mkf_char_t)) ;

	return  1 ;
}


/* --- static variables --- */

static mkf_parser_t  parser =
{
	NULL , 0 , 0 , 0 ,
	mkf_parser_init ,
	set_str ,
	delete ,
	next_char ,
} ;


/* --- global functions --- */

mkf_parser_t *
mkf_str_parser_init(
	mkf_char_t *  src ,
	u_int  src_len
	)
{
	(*parser.init)( &parser) ;
	(*parser.set_str)( &parser , (u_char*)src ,
			src_len * sizeof(mkf_char_t)) ;

	return  &parser ;
}
