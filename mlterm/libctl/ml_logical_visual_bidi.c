/*
 *	$Id$
 */

#include  "../ml_logical_visual.h"

#include  <kiklib/kik_mem.h>		/* realloc/free */
#include  <kiklib/kik_debug.h>		/* kik_msg_printf */
#include  <kiklib/kik_str.h>            /* strcmp */
#include  "ml_line_bidi.h"


#define  CURSOR_LINE(logvis)  (ml_model_get_line((logvis)->model,(logvis)->cursor->row))

#if  0
#define  __DEBUG
#endif

#if  0
#define  CURSOR_DEBUG
#endif


typedef struct  bidi_logical_visual
{
	ml_logical_visual_t  logvis ;
	
	int  cursor_logical_char_index ;
	int  cursor_logical_col ;
	int  ltr_rtl_meet_pos ;
	ml_bidi_mode_t  bidi_mode ;

} bidi_logical_visual_t ;


/* --- static functions --- */

/*
 * Bidi logical <=> visual methods
 */

static int
bidi_delete(
	ml_logical_visual_t *  logvis
	)
{
	int  row ;

	if( logvis->model)
	{
		for( row = 0 ; row < logvis->model->num_of_rows ; row ++)
		{
			ml_line_set_use_bidi( &logvis->model->lines[row] , 0) ;
		}
	}

	free( logvis) ;

	return  1 ;
}

static int
bidi_init(
	ml_logical_visual_t *  logvis ,
	ml_model_t *  model ,
	ml_cursor_t *  cursor
	)
{
	int  row ;

	if( logvis->model)
	{
		for( row = 0 ; row < logvis->model->num_of_rows ; row ++)
		{
			ml_line_set_use_bidi( &logvis->model->lines[row] , 0) ;
		}
	}

	logvis->model = model ;
	logvis->cursor = cursor ;
	
	return  1 ;
}

static u_int
bidi_logical_cols(
	ml_logical_visual_t *  logvis
	)
{
	return  logvis->model->num_of_cols ;
}

static u_int
bidi_logical_rows(
	ml_logical_visual_t *  logvis
	)
{
	return  logvis->model->num_of_rows ;
}

static int
bidi_render(
	ml_logical_visual_t *  logvis
	)
{
	ml_line_t *  line ;
	int  row ;

	if( logvis->is_visual)
	{
		return  1 ;
	}

	/*
	 * all lines(not only filled lines) should be rendered.
	 */
	for( row = 0 ; row < logvis->model->num_of_rows ; row ++)
	{
		int  need_render ;

		line = ml_model_get_line( logvis->model , row) ;

		need_render = 0 ;
		
		if( ! ml_line_is_using_bidi( line))
		{
			ml_line_set_use_bidi( line , 1) ;

			need_render = 1 ;
		}

		if( ml_line_is_modified( line) || need_render)
		{
			if( ! ml_line_bidi_render( line ,
						((bidi_logical_visual_t*)logvis)->bidi_mode))
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " ml_line_bidi_render failed.\n") ;
			#endif
			}
		}
	}

	return  1 ;
}

static int
bidi_visual(
	ml_logical_visual_t *  logvis
	)
{
	int  row ;

	if( logvis->is_visual)
	{
		return  0 ;
	}

#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d (ltrmeet)%d] ->" ,
		logvis->cursor->char_index , logvis->cursor->col , logvis->cursor->row ,
		((bidi_logical_visual_t*)logvis)->ltr_rtl_meet_pos) ;
#endif

	for( row = 0 ; row < logvis->model->num_of_rows ; row ++)
	{
		if( ! ml_line_bidi_visual( ml_model_get_line( logvis->model , row)))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " visualize row %d failed.\n" , row) ;
		#endif
		}
	}

	((bidi_logical_visual_t*)logvis)->cursor_logical_char_index = logvis->cursor->char_index ;
	((bidi_logical_visual_t*)logvis)->cursor_logical_col = logvis->cursor->col ;

	logvis->cursor->char_index = ml_line_bidi_convert_logical_char_index_to_visual(
					CURSOR_LINE(logvis) , logvis->cursor->char_index ,
					&((bidi_logical_visual_t*)logvis)->ltr_rtl_meet_pos) ;
	/*
	 * XXX
	 * col_in_char should not be plused to col, because the character pointed by
	 * ml_line_bidi_convert_logical_char_index_to_visual() is not the same as the one
	 * in logical order.
	 */
	logvis->cursor->col = ml_convert_char_index_to_col( CURSOR_LINE(logvis) ,
					logvis->cursor->char_index , 0) + logvis->cursor->col_in_char ;

#ifdef  CURSOR_DEBUG
	kik_msg_printf( "-> [cursor(index)%d (col)%d (row)%d (ltrmeet)%d]\n" ,
		logvis->cursor->char_index , logvis->cursor->col , logvis->cursor->row ,
		((bidi_logical_visual_t*)logvis)->ltr_rtl_meet_pos) ;
#endif

	logvis->is_visual = 1 ;

	return  1 ;
}

static int
bidi_logical(
	ml_logical_visual_t *  logvis
	)
{
	int  row ;

	if( ! logvis->is_visual)
	{
		return  0 ;
	}
	
#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		logvis->cursor->char_index , logvis->cursor->col , logvis->cursor->row) ;
#endif

	for( row = 0 ; row < logvis->model->num_of_rows ; row ++)
	{
		if( ! ml_line_bidi_logical( ml_model_get_line( logvis->model , row)))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " visualize row %d failed.\n" , row) ;
		#endif
		}
	}

	logvis->cursor->char_index = ((bidi_logical_visual_t*)logvis)->cursor_logical_char_index ;
	logvis->cursor->col = ((bidi_logical_visual_t*)logvis)->cursor_logical_col ;

#ifdef  CURSOR_DEBUG
	kik_msg_printf( "-> [cursor(index)%d (col)%d (row)%d]\n" ,
		logvis->cursor->char_index , logvis->cursor->col , logvis->cursor->row) ;
#endif
	
	logvis->is_visual = 0 ;
	
	return  1 ;
}

static int
bidi_visual_line(
	ml_logical_visual_t *  logvis ,
	ml_line_t *  line
	)
{
	int  need_render ;

	need_render = 0 ;
	
	if( ! ml_line_is_using_bidi( line))
	{
		ml_line_set_use_bidi( line , 1) ;

		need_render = 1 ;
	}

	if( ml_line_is_modified( line) || need_render)
	{
		if( ! ml_line_bidi_render( line , ((bidi_logical_visual_t*)logvis)->bidi_mode))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_line_bidi_render failed.\n") ;
		#endif
		}
	}
	
	if( ! ml_line_bidi_visual( line))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_line_bidi_visual() failed.\n") ;
	#endif
	}

	return  1 ;
}


/* --- global functions --- */

ml_logical_visual_t *
ml_logvis_bidi_new(
	ml_bidi_mode_t  bidi_mode
	)
{
	bidi_logical_visual_t *  bidi_logvis ;

	if( ( bidi_logvis = malloc( sizeof( bidi_logical_visual_t))) == NULL)
	{
		return  NULL ;
	}
	
	bidi_logvis->cursor_logical_char_index = 0 ;
	bidi_logvis->cursor_logical_col = 0 ;
	bidi_logvis->ltr_rtl_meet_pos = 0 ;
	bidi_logvis->bidi_mode = bidi_mode ;

	bidi_logvis->logvis.model = NULL ;
	bidi_logvis->logvis.cursor = NULL ;
	bidi_logvis->logvis.is_visual = 0 ;
	
	bidi_logvis->logvis.delete = bidi_delete ;
	bidi_logvis->logvis.init = bidi_init ;
	bidi_logvis->logvis.logical_cols = bidi_logical_cols ;
	bidi_logvis->logvis.logical_rows = bidi_logical_rows ;
	bidi_logvis->logvis.render = bidi_render ;
	bidi_logvis->logvis.visual = bidi_visual ;
	bidi_logvis->logvis.logical = bidi_logical ;
	bidi_logvis->logvis.visual_line = bidi_visual_line ;

	bidi_logvis->logvis.is_reversible = 1 ;

	return  (ml_logical_visual_t*) bidi_logvis ;
}
