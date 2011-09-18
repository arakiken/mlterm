/*
 *	$Id$
 */

#ifndef  __ML_LOGICAL_VISUAL_H__
#define  __ML_LOGICAL_VISUAL_H__


#include  "ml_model.h"
#include  "ml_cursor.h"
#include  "ml_bidi.h"	/* ml_bidi_mode_t */


/*
 * LTR ... e.g. Mongolian
 * RTL ... e.g. CJK
 */
typedef enum  ml_vertical_mode
{
	VERT_LTR = 0x1 ,
	VERT_RTL = 0x2 ,

	VERT_MODE_MAX

} ml_vertical_mode_t ;

typedef struct  ml_logical_visual
{
	/* Private */
	
	ml_model_t *  model ;
	ml_cursor_t *  cursor ;

	int8_t  is_visual ;


	/* Public */
	
	/* Whether logical <=> visual is reversible. */
	int8_t  is_reversible ;
	
	int  (*init)( struct ml_logical_visual * , ml_model_t * , ml_cursor_t *) ;
	
	int  (*delete)( struct ml_logical_visual *) ;
	
	u_int  (*logical_cols)( struct ml_logical_visual *) ;
	u_int  (*logical_rows)( struct ml_logical_visual *) ;

	/*
	 * !! Notice !!
	 * ml_model_t should not be modified from render/viaul until logical.
	 * Any modification is done from logical until render/visual.
	 */
	int  (*render)( struct ml_logical_visual *) ;
	int  (*visual)( struct ml_logical_visual *) ;
	int  (*logical)( struct ml_logical_visual *) ;
	
	int  (*visual_line)( struct ml_logical_visual * , ml_line_t *  line) ;

} ml_logical_visual_t ;


ml_logical_visual_t *  ml_logvis_container_new(void) ;

int  ml_logvis_container_add( ml_logical_visual_t *  logvis , ml_logical_visual_t *  child) ;

ml_logical_visual_t *  ml_logvis_comb_new(void) ;

ml_logical_visual_t *  ml_logvis_vert_new( ml_vertical_mode_t  vertical_mode) ;

ml_vertical_mode_t  ml_get_vertical_mode( char *  name) ;

char *  ml_get_vertical_mode_name( ml_vertical_mode_t  mode) ;

#ifndef  NO_DYNAMIC_LOAD_CTL

ml_logical_visual_t *  ml_logvis_bidi_new( ml_bidi_mode_t  mode) ;

ml_logical_visual_t *  ml_logvis_iscii_new(void) ;

#else

#ifndef  USE_FRIBIDI
#define  ml_logvis_bidi_new( a)  (NULL)
#endif

#ifndef  USE_IND
#define  ml_logvis_iscii_new()  (NULL)
#endif


#endif

#endif
