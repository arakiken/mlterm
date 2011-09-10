/*
 *	$Id$
 */

#ifndef  __ML_LINE_ISCII_H__
#define  __ML_LINE_ISCII_H__


#include  "../ml_line.h"


#define  ml_line_is_using_iscii( line)  ((line)->ctl_info_type == VINFO_ISCII)

int  ml_line_set_use_iscii( ml_line_t *  line , int  flag) ;

int  ml_line_iscii_render( ml_line_t *  line) ;

int  ml_line_iscii_visual( ml_line_t *  line) ;

int  ml_iscii_convert_logical_char_index_to_visual( ml_line_t *  line , int  logical_char_index) ;


#endif
