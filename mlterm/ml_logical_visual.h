/*
 *	$Id$
 */

#ifndef  __ML_LOGICAL_VISUAL_H__
#define  __ML_LOGICAL_VISUAL_H__


#include  "ml_edit.h"
#include  "ml_iscii.h"


/*
 * LTR ... e.g. Mongolian
 * RTL ... e.g. CJK
 */
typedef enum  ml_vertical_mode
{
	VERT_LTR = 0x1 ,
	VERT_RTL = 0x2 ,

} ml_vertical_mode_t ;

typedef struct  ml_logical_visual
{
	int  is_visual ;
	
	ml_edit_t *  edit ;

	int  (*init)( struct ml_logical_visual * , ml_edit_t *) ;
	
	int  (*delete)( struct ml_logical_visual *) ;
	
	u_int  (*logical_cols)( struct ml_logical_visual *) ;
	u_int  (*logical_rows)( struct ml_logical_visual *) ;

	/*
	 * !! Notice !!
	 * ml_edit_t should not be modified from render/viaul until logical.
	 * Any modification is done from logical until render/visual.
	 */
	int  (*render)( struct ml_logical_visual *) ;
	int  (*visual)( struct ml_logical_visual *) ;
	int  (*logical)( struct ml_logical_visual *) ;
	
	int  (*visual_line)( struct ml_logical_visual * , ml_line_t *  line) ;
	
} ml_logical_visual_t ;


ml_logical_visual_t *  ml_logvis_container_new(void) ;

int  ml_logvis_container_add( ml_logical_visual_t *  logvis , ml_logical_visual_t *  child) ;

ml_logical_visual_t *  ml_logvis_bidi_new(void) ;

ml_logical_visual_t *  ml_logvis_comb_new(void) ;

ml_logical_visual_t *  ml_logvis_iscii_new( ml_iscii_state_t  iscii_state) ;

ml_logical_visual_t *  ml_logvis_vert_new( ml_vertical_mode_t  vertical_mode) ;

ml_vertical_mode_t  ml_get_vertical_mode( char *  name) ;

char *  ml_get_vertical_mode_name( ml_vertical_mode_t  mode) ;


#endif
