/*
 *	$Id$
 */

#ifndef  __ML_TERM_MANAGER_H__
#define  __ML_TERM_MANAGER_H__


#include  "ml_term.h"


int  ml_term_manager_init( u_int  multiple) ;

int  ml_term_manager_final( void) ;

ml_term_t *  ml_create_term( u_int  cols , u_int  rows , u_int  tab_size , u_int  log_size ,
	ml_char_encoding_t  encoding , int  is_auto_encoding , ml_unicode_policy_t  policy ,
	int  col_size_a , int  use_char_combining , int  use_multi_col_char , int  use_bidi ,
	ml_bidi_mode_t  bidi_mode , int  use_ind , int  use_bce , int  use_dynamic_comb ,
	ml_bs_mode_t  bs_mode , ml_vertical_mode_t  vertical_mode , int  use_local_echo ,
	char *  win_name , char *  icon_name , ml_alt_color_mode_t  alt_color_mode) ;

int  ml_destroy_term( ml_term_t *  term) ;

ml_term_t *  ml_get_term( char *  dev) ;

ml_term_t *  ml_get_detached_term( char *  dev) ;

ml_term_t *  ml_next_term( ml_term_t *  term) ;

ml_term_t *  ml_prev_term( ml_term_t *  term) ;

u_int  ml_get_all_terms( ml_term_t ***  terms) ;

int  ml_close_dead_terms(void) ;

char *  ml_get_pty_list(void) ;

void  ml_term_manager_enable_zombie_pty( void) ;


#endif
