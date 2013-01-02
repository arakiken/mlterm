/*
 *	$Id$
 */

#ifndef  __ML_DRCS_H__


#include  <kiklib/kik_types.h>
#include  <mkf/mkf_charset.h>


typedef struct ml_drcs
{
	mkf_charset_t  cs ;
	char *  glyphs[0x5f] ;

} ml_drcs_t ;


int  ml_drcs_final( mkf_charset_t  cs) ;

ml_drcs_t *  ml_drcs_get( mkf_charset_t  cs , int  create) ;

int  ml_drcs_add( ml_drcs_t *  font , int  idx , char *  seq , u_int  width , u_int  height) ;


#endif
