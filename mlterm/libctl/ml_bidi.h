/*
 *	$Id$
 */

#ifndef  __CTL_ML_BIDI_H__
#define  __CTL_ML_BIDI_H__


#include  "../ml_bidi.h"
#include  "../ml_char.h"


/* Only used by ml_bidi.c, ml_line_bidi.c */
#define  BASE_IS_RTL(state)	((((state)->rtl_state) >> 1) & 0x1)
#define  SET_BASE_RTL(state)	(((state)->rtl_state) |= (0x1 << 1))
#define  UNSET_BASE_RTL(state) (((state)->rtl_state) &= ~(0x1 << 1))
#define  HAS_RTL(state)		(((state)->rtl_state) & 0x1)
#define  SET_HAS_RTL(state)	(((state)->rtl_state) |= 0x1)
#define  UNSET_HAS_RTL(state)	(((state)->rtl_state) &= ~0x1)


struct  ml_bidi_state
{
	u_int16_t *  visual_order ;
	u_int16_t  size ;
	
	int8_t  bidi_mode ;	/* Cache how visual_order is rendered. */

	/*
	 * 6bit: Not used for now.
	 * 1bit: base_is_rtl
	 * 1bit: has_rtl
	 */
	int8_t  rtl_state ;

} ;


ml_bidi_state_t  ml_bidi_new(void) ;

int  ml_bidi_delete( ml_bidi_state_t  state) ;

int  ml_bidi( ml_bidi_state_t  state , ml_char_t *  src , u_int  size , ml_bidi_mode_t  mode) ;



#endif
