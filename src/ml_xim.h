/*
 *	$Id$
 */

#ifndef  __ML_XIM_H__
#define  __ML_XIM_H__


#include  <mkf/mkf_parser.h>

#include  "ml_char_encoding.h"
#include  "ml_window.h"


#define  MAX_XICS_PER_XIM  5


typedef struct  ml_xim
{
	XIM  im ;
	char *  xmod ;

	char *  locale ;
	mkf_parser_t *  parser ;
	ml_char_encoding_t  encoding ;

	ml_window_t *  xic_wins[MAX_XICS_PER_XIM] ;
	u_int  num_of_xic_wins ;
	
} ml_xim_t ;


int  ml_xim_final(void) ;

ml_xim_t *  ml_get_xim( Display *  display , char *  name , char *  locale) ;

int  ml_xic_created( ml_xim_t *  xim , ml_window_t *  win) ;

int  ml_xic_destroyed( ml_xim_t *  xim , ml_window_t *  win) ;

char *  ml_get_xim_name( ml_xim_t *  xim) ;


#endif
