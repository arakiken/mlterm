/*
 *	$Id$
 */

#ifndef  __ML_IMAGE_MODEL_H__
#define  __ML_IMAGE_MODEL_H__


#include  <kiklib/kik_types.h>

#include  "ml_char.h"
#include  "ml_image_line.h"


typedef struct  ml_image_model
{
	/* private */
	ml_image_line_t *  lines ;
	
	/* public(readonly) */
	u_int  num_of_cols ;
	u_int  num_of_rows ;
	u_int  num_of_filled_rows ;
	
	/* private */
	int  beg_row ;			/* used for scrolling */

} ml_image_model_t ;


int  ml_imgmdl_init( ml_image_model_t *  model , u_int  num_of_cols , u_int  num_of_rows) ;

int  ml_imgmdl_final( ml_image_model_t *  model) ;

int  ml_imgmdl_reset( ml_image_model_t *  model) ;

int  ml_imgmdl_resize( ml_image_model_t *  model , u_int  num_of_cols , u_int  num_of_rows) ;

int  ml_imgmdl_end_row( ml_image_model_t *  model) ;

ml_image_line_t *  ml_imgmdl_get_line( ml_image_model_t *  model , int  row) ;

ml_image_line_t *  ml_imgmdl_get_end_line( ml_image_model_t *  model) ;

u_int  ml_imgmdl_reserve_boundary( ml_image_model_t *  model , u_int  size) ;

u_int  ml_imgmdl_break_boundary( ml_image_model_t *  model , u_int  size , ml_char_t *  sp_ch) ;

u_int  ml_imgmdl_shrink_boundary( ml_image_model_t *  model , u_int  size) ;

int  ml_imgmdl_scroll_upward( ml_image_model_t *  model , u_int  size) ;

int  ml_imgmdl_scroll_downward( ml_image_model_t *  model , u_int  size) ;


#endif
