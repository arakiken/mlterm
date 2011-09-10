/*
 *	$Id$
 */

#include  "ml_shape.h"

#include  "ml_ctl_loader.h"


/* --- global functions --- */

#ifndef  NO_DYNAMIC_LOAD_CTL

ml_shape_t *
ml_arabic_shape_new(void)
{
	ml_shape_t * (*func)(void) ;

	if( ! (func = ml_load_ctl_bidi_func( ML_ARABIC_SHAPE_NEW)))
	{
		return  NULL ;
	}

	return  (*func)() ;
}

u_int16_t
ml_is_arabic_combining(
	ml_char_t *  prev2 ,		/* can be NULL */
	ml_char_t *  prev ,		/* must be ISO10646_UCS4_1 character */
	ml_char_t *  ch			/* must be ISO10646_UCS4_1 character */
	)
{
	u_int16_t (*func)( ml_char_t * , ml_char_t * , ml_char_t *) ;

	if( ! (func = ml_load_ctl_bidi_func( ML_IS_ARABIC_COMBINING)))
	{
		return  0 ;
	}

	return  (*func)( prev2 , prev , ch) ;
}

ml_shape_t *
ml_iscii_shape_new(void)
{
	ml_shape_t * (*func)(void) ;

	if( ! (func = ml_load_ctl_iscii_func( ML_ISCII_SHAPE_NEW)))
	{
		return  NULL ;
	}

	return  (*func)() ;
}

#endif
