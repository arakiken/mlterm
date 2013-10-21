/*
 *	$Id$
 */

#ifndef  __ML_MODEL_H__
#define  __ML_MODEL_H__


#include  <kiklib/kik_types.h>

#include  "ml_str.h"
#include  "ml_line.h"


typedef struct  ml_model
{
	/* private */
	ml_line_t *  lines ;
	
	/* public(readonly) */
	u_int16_t  num_of_cols ;	/* 0 - 65536 */
	u_int16_t  num_of_rows ;	/* 0 - 65536 */
	
	/* private */
	int  beg_row ;			/* used for scrolling */

} ml_model_t ;


int  ml_model_init( ml_model_t *  model , u_int  num_of_cols , u_int  num_of_rows) ;

int  ml_model_final( ml_model_t *  model) ;

int  ml_model_reset( ml_model_t *  model) ;

int  ml_model_resize( ml_model_t *  model , u_int *  slide , u_int  num_of_cols , u_int  num_of_rows) ;

u_int  ml_model_get_num_of_filled_rows( ml_model_t *  model) ;

int  ml_model_end_row( ml_model_t *  model) ;

ml_line_t *  ml_model_get_line( ml_model_t *  model , int  row) ;

int  ml_model_scroll_upward( ml_model_t *  model , u_int  size) ;

int  ml_model_scroll_downward( ml_model_t *  model , u_int  size) ;

#ifdef  DEBUG

void  ml_model_dump( ml_model_t *  model) ;

#endif


#endif
