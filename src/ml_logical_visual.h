/*
 *	$Id$
 */

#ifndef  __ML_LOGICAL_VISUAL_H__
#define  __ML_LOGICAL_VISUAL_H__


#include  "ml_image.h"
#include  "ml_iscii.h"


/*
 * LTR | HALF_WIDTH ... e.g. Mongolian
 * RTL | FULL_WIDTH ... e.g. CJK
 */
typedef enum  ml_vertical_mode
{
	VERT_LTR = 0x1 ,
	VERT_RTL = 0x2 ,

	VERT_HALF_WIDTH = 0x4 ,		/* Half-width character is the basis */
	VERT_FULL_WIDTH = 0x8 ,		/* Full-width character is the basis */
	
} ml_vertical_mode_t ;

typedef struct  ml_logical_visual
{
	int  is_visual ;
	
	ml_image_t *  image ;

	int  (*delete)( struct ml_logical_visual *) ;
	
	int  (*change_image)( struct ml_logical_visual * , ml_image_t *) ;
	int  (*logical_cols)( struct ml_logical_visual *) ;
	int  (*logical_rows)( struct ml_logical_visual *) ;

	/*
	 * !! Notice !!
	 * ml_image_t should not be modified from render/viaul until logical.
	 * Any modification is done from logical until render/visual.
	 */
	int  (*render)( struct ml_logical_visual *) ;
	int  (*visual)( struct ml_logical_visual *) ;
	int  (*logical)( struct ml_logical_visual *) ;
	int  (*visual_line)( struct ml_logical_visual * , ml_image_line_t *  line) ;
	
} ml_logical_visual_t ;


ml_logical_visual_t *  ml_logvis_container_new( ml_image_t *  image) ;

int  ml_logvis_container_add( ml_logical_visual_t *  logvis , ml_logical_visual_t *  child) ;

ml_logical_visual_t *  ml_logvis_bidi_new( ml_image_t *  image) ;

ml_logical_visual_t *  ml_logvis_iscii_new( ml_image_t *  image , ml_iscii_state_t  iscii_state) ;

ml_logical_visual_t *  ml_logvis_vert_new( ml_image_t *  image , ml_vertical_mode_t  vertical_mode) ;

ml_vertical_mode_t  ml_get_vertical_mode( char *  name) ;


#endif
