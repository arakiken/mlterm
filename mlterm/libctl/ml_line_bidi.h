/*
 *	$Id$
 */

#ifndef  __ML_LINE_BIDI_H__
#define  __ML_LINE_BIDI_H__

#include  "../ml_line.h"
#include  "ml_bidi.h"		/* ml_bidi_mode_t */


#define  ml_line_is_using_bidi( line)  ((line)->ctl_info_type == VINFO_BIDI)

int  ml_line_set_use_bidi( ml_line_t *  line , int  flag) ;

int  ml_line_bidi_render( ml_line_t *  line , ml_bidi_mode_t  bidi_mode ,
	const char *  separators) ;

int  ml_line_bidi_visual( ml_line_t *  line) ;

int  ml_line_bidi_logical( ml_line_t *  line) ;

int  ml_line_bidi_convert_logical_char_index_to_visual( ml_line_t *  line , int  char_index ,
	int *  ltr_rtl_meet_pos) ;


#endif
