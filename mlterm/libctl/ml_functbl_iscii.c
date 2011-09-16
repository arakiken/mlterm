/*
 *	$Id$
 */

#include  "../ml_ctl_loader.h"


/* Dummy declaration */
void  ml_line_set_use_iscii(void) ;
void  ml_line_iscii_convert_logical_char_index_to_visual(void) ;
void  ml_iscii_copy(void) ;
void  ml_iscii_reset(void) ;


/* --- global variables --- */

void *  ml_ctl_iscii_func_table[MAX_CTL_ISCII_FUNCS] =
{
	(void*)CTL_API_COMPAT_CHECK_MAGIC ,
	ml_isciikey_state_new ,
	ml_isciikey_state_delete ,
	ml_convert_ascii_to_iscii ,
	ml_line_set_use_iscii ,
	ml_line_iscii_convert_logical_char_index_to_visual ,
	ml_logvis_iscii_new ,
	ml_iscii_shape_new ,
	ml_iscii_copy ,
	ml_iscii_reset ,

} ;
