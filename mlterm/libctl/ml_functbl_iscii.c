/*
 *	$Id$
 */

#include  "../ml_ctl_loader.h"


/* Dummy declaration */
void  ml_line_set_use_iscii(void) ;
void  ml_line_iscii_convert_logical_char_index_to_visual(void) ;
void  ml_iscii_copy(void) ;
void  ml_iscii_reset(void) ;
void  ml_line_iscii_need_shape(void) ;
void  ml_line_iscii_render(void) ;
void  ml_line_iscii_visual(void) ;
void  ml_line_iscii_logical(void) ;


/* --- global variables --- */

void *  ml_ctl_iscii_func_table[MAX_CTL_ISCII_FUNCS] =
{
	(void*)CTL_API_COMPAT_CHECK_MAGIC ,
	ml_isciikey_state_new ,
	ml_isciikey_state_delete ,
	ml_convert_ascii_to_iscii ,
	ml_line_set_use_iscii ,
	ml_line_iscii_convert_logical_char_index_to_visual ,
	ml_shape_iscii ,
	ml_iscii_copy ,
	ml_iscii_reset ,
	ml_line_iscii_need_shape ,
	ml_line_iscii_render ,
	ml_line_iscii_visual ,
	ml_line_iscii_logical ,

} ;
