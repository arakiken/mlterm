/*
 *	$Id$
 */

#ifndef  __ML_TERM_MANAGER_H__
#define  __ML_TERM_MANAGER_H__


#include  "ml_term.h"


int  ml_term_manager_init( void) ;

int  ml_term_manager_final( void) ;

ml_term_t *  ml_create_term( u_int  cols , u_int  rows , u_int  tab_size , u_int  log_size ,
	ml_char_encoding_t  encoding , int  not_use_unicode_font , int  only_use_unicode_font ,
	int  col_size_a , int  use_char_combining , int  use_multi_col_char , int  use_bce ,
	ml_bs_mode_t  bs_mode) ;

ml_term_t *  ml_get_term( char *  dev) ;

ml_term_t *  ml_next_term( ml_term_t *  term) ;

int  ml_put_back_term( ml_term_t *  term) ;

u_int  ml_get_all_terms( ml_term_t ***  terms) ;

int  ml_close_dead_terms(void) ;

char *  ml_get_pty_list(void) ;


#endif
