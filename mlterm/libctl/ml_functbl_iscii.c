/*
 *	$Id$
 */

#include  "../ml_ctl_loader.h"

#include  "../ml_logical_visual.h"
#include  "ml_line_iscii.h"


/* Declaration of functions defined in ml_line_iscii.c */
int  ml_line_set_use_iscii( ml_line_t *  line , int  flag) ;
int  ml_iscii_convert_visual_char_index_to_logical( ml_line_t *  line , int  char_index) ;

/* Declaration of functions defined in ml_iscii.c */
int  ml_iscii_copy( ml_iscii_state_t *  dst , ml_iscii_state_t *  src) ;
int  ml_iscii_reset( ml_iscii_state_t *  state) ;


/* --- global variables --- */

void *  ml_ctl_iscii_func_table[MAX_CTL_ISCII_FUNCS] =
{
	ml_isciikey_state_new ,
	ml_isciikey_state_delete ,
	ml_convert_ascii_to_iscii ,
	ml_line_set_use_iscii ,
	ml_iscii_convert_logical_char_index_to_visual ,
	ml_logvis_iscii_new ,
	ml_iscii_shape_new ,
	ml_iscii_copy ,
	ml_iscii_reset ,

} ;
