/*
 *	$Id$
 */

#include  "ml_term_factory.h"

#include  <kiklib/kik_mem.h>	/* malloc */


/* --- static variables --- */

static ml_term_t **  detached_terms ;
static u_int  num_of_detached ;


/* --- global functions --- */

ml_term_t *
ml_attach_term(
	u_int  cols ,
	u_int  rows ,
	u_int  tab_size ,
	u_int  log_size ,
	ml_char_encoding_t  encoding ,
	int  not_use_unicode_font ,
	int  only_use_unicode_font ,
	int  col_size_a ,
	int  use_char_combining ,
	int  use_multi_col_char ,
	int  use_bce
	)
{
	ml_term_t *  term ;

	if( num_of_detached > 0)
	{
		term = detached_terms[ -- num_of_detached] ;

		ml_term_resize( term , cols , rows) ;
		ml_term_change_log_size( term , log_size) ;
		ml_term_change_encoding( term , encoding) ;
		ml_term_set_char_combining_flag( term , use_char_combining) ;
		ml_term_set_multi_col_char_flag( term , use_multi_col_char) ;

		if( num_of_detached == 0)
		{
			free( detached_terms) ;
			detached_terms = NULL ;
		}
	}
	else
	{
		if( ( term = ml_term_new( cols , rows , tab_size , log_size , encoding ,
				not_use_unicode_font , only_use_unicode_font , col_size_a ,
				use_char_combining , use_multi_col_char , use_bce)) == NULL)
		{
			return  NULL ;
		}
	}

	return  term ;
}

int
ml_detach_term(
	ml_term_t *  term
	)
{
	void *  p ;
	
	if( ( p = realloc( detached_terms , sizeof( ml_term_t *) * (num_of_detached + 1))))
	{
		detached_terms = p ;

		detached_terms[num_of_detached ++] = term ;

		return  1 ;
	}
	else
	{
		return  ml_term_delete( term) ;
	}
}

u_int
ml_get_detached_terms(
	ml_term_t ***  terms
	)
{
	*terms = detached_terms ;

	return  num_of_detached ;
}
