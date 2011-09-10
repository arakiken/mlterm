/*
 *	$Id$
 */

#include  "../ml_ctl_loader.h"

#include  "../ml_logical_visual.h"
#include  "ml_line_bidi.h"


/* Declaration of functions defined in ml_line_bidi.c */
int  ml_line_set_use_bidi( ml_line_t *  line , int  flag) ;
int  ml_bidi_convert_visual_char_index_to_logical( ml_line_t *  line , int  char_index) ;
int  ml_line_bidi_copy_logical_str( ml_line_t *  line , ml_char_t *  dst , int  beg , u_int  len) ;
int  ml_line_is_rtl( ml_line_t *  line) ;

/* Declaration of functions defined in ml_bidi.c */
int  ml_bidi_copy( ml_bidi_state_t *  dst , ml_bidi_state_t *  src) ;
int  ml_bidi_reset( ml_bidi_state_t *  state) ;


/* --- global variables --- */

void *  ml_ctl_bidi_func_table[MAX_CTL_BIDI_FUNCS] =
{
	ml_line_set_use_bidi ,
	ml_bidi_convert_logical_char_index_to_visual ,
	ml_bidi_convert_visual_char_index_to_logical ,
	ml_line_bidi_copy_logical_str ,
	ml_line_is_rtl ,
	ml_logvis_bidi_new ,
	ml_arabic_shape_new ,
	ml_is_arabic_combining ,
	ml_bidi_copy ,
	ml_bidi_reset ,

} ;
