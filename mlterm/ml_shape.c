/*
 *	$Id$
 */

#include  "ml_shape.h"

#include  "ml_ctl_loader.h"


/* --- global functions --- */

#ifndef  NO_DYNAMIC_LOAD_CTL

u_int
ml_shape_arabic(
	ml_char_t *  dst ,
	u_int  dst_len ,
	ml_char_t *  src ,
	u_int  src_len
	)
{
	u_int (*func)( ml_char_t *  dst , u_int  dst_len , ml_char_t *  src , u_int  src_len) ;

	if( ! (func = ml_load_ctl_bidi_func( ML_SHAPE_ARABIC)))
	{
		return  0 ;
	}

	return  (*func)( dst , dst_len , src , src_len) ;
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

u_int
ml_shape_iscii(
	ml_char_t *  dst ,
	u_int  dst_len ,
	ml_char_t *  src ,
	u_int  src_len
	)
{
	u_int (*func)( ml_char_t *  dst , u_int  dst_len , ml_char_t *  src , u_int  src_len) ;

	if( ! (func = ml_load_ctl_iscii_func( ML_SHAPE_ISCII)))
	{
		return  0 ;
	}

	return  (*func)( dst , dst_len , src , src_len) ;
}

#endif
