/*
 *	$Id$
 */

#ifndef  __ML_TERM_FACTORY_H__
#define  __ML_TERM_FACTORY_H__


#include  "ml_term.h"


ml_term_t *  ml_attach_term( u_int  cols , u_int  rows , u_int  tab_size , u_int  log_size ,
	ml_char_encoding_t  encoding , int  not_use_unicode_font , int  only_use_unicode_font ,
	int  col_size_a , int  use_char_combining , int  use_multi_col_char , int  use_bce) ;

int  ml_detach_term( ml_term_t *  term) ;

u_int  ml_get_detached_terms( ml_term_t ***  terms) ;


#endif
