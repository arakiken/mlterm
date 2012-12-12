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


#if  ! defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI)

ml_shape_t *  ml_arabic_shape_new(void) ;

u_int16_t  ml_is_arabic_combining( ml_char_t *  prev2 , ml_char_t *  prev , ml_char_t *  ch) ;

#else

#define  ml_arabic_shape_new()  (NULL)
#define  ml_is_arabic_combining( a , b , c)  (NULL)

#endif

#if  ! defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND)

ml_shape_t *  ml_iscii_shape_new( void) ;

#else

#define  ml_iscii_shape_new()  (NULL)

#endif


#endif	/* __ML_SHAPING_H__ */
