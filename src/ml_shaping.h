/*
 *	$Id$
 */

#ifndef  __ML_SHAPING_H__
#define  __ML_SHAPING_H__


#include  <kiklib/kik_types.h>	/* u_int */

#include  "ml_char.h"
#include  "ml_iscii.h"


typedef struct  ml_shape
{
	u_int (*shape)( struct ml_shape * , ml_char_t * , u_int , ml_char_t * , u_int) ;
	int  (*delete)( struct ml_shape *) ;

} ml_shape_t ;


ml_shape_t *  ml_arabic_shape_new(void) ;

ml_shape_t *  ml_iscii_shape_new( ml_iscii_state_t  iscii_state) ;


#endif
