/*
 *	$Id$
 */

#include  "../ml_logical_visual.h"

#include  <kiklib/kik_mem.h>		/* realloc/free */
#include  <kiklib/kik_util.h>		/* K_MIN */
#include  <kiklib/kik_debug.h>		/* kik_msg_printf */
#include  <kiklib/kik_str.h>            /* strcmp */
#include  "ml_line_iscii.h"


#define  CURSOR_LINE(logvis)  (ml_model_get_line((logvis)->model,(logvis)->cursor->row))

#if  0
#define  __DEBUG
#endif

#if  0
#define  CURSOR_DEBUG
#endif


typedef struct  iscii_logical_visual
{
	ml_logical_visual_t  logvis ;
	
	int  cursor_logical_char_index ;
	int  cursor_logical_col ;

} iscii_logical_visual_t ;


/* --- static functions --- */

/*
 * ISCII logical <=> visual methods
 */

static int
iscii_delete(
	ml_logical_visual_t *  logvis
	)
{
	int  row ;

	if( logvis->model)
	{
		for( row = 0 ; row < logvis->model->num_of_rows ; row ++)
		{
			ml_line_set_use_iscii( &logvis->model->lines[row] , 0) ;
		}
	}

	free( logvis) ;
	
	return  1 ;
}

static int
iscii_init(
	ml_logical_visual_t *  logvis ,
	ml_model_t *  model ,
	ml_cursor_t *  cursor
	)
{
	logvis->model = model ;
	logvis->cursor = cursor ;

	return  1 ;
}

static u_int
iscii_logical_cols(
	ml_logical_visual_t *  logvis
	)
{
	return  logvis->model->num_of_cols ;
}

static u_int
iscii_logical_rows(
	ml_logical_visual_t *  logvis
	)
{
	return  logvis->model->num_of_rows ;
}

static void
iscii_render_line(
	ml_logical_visual_t *  logvis ,
	ml_line_t *  line
	)
{
	int  need_render ;

	if( ml_line_is_empty( line))
	{
		return ;
	}

	need_render = 0 ;

	if( ! ml_line_is_using_iscii( line))
	{
		ml_line_set_use_iscii( line , 1) ;

		need_render = 1 ;
	}

	if( ml_line_is_modified( line) || need_render)
	{
		if( ! ml_line_iscii_render( line))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_line_iscii_render failed.\n") ;
		#endif
		}
	}
}

static int
iscii_render(
	ml_logical_visual_t *  logvis
	)
{
	int  row ;

	for( row = 0 ; row < logvis->model->num_of_rows ; row ++)
	{
		iscii_render_line( logvis , ml_model_get_line( logvis->model , row)) ;
	}

	return  1 ;
}

static int
iscii_visual(
	ml_logical_visual_t *  logvis
	)
{
	iscii_logical_visual_t *  iscii_logvis ;
	int  row ;

	if( logvis->is_visual)
	{
		return  0 ;
	}

	iscii_logvis = (iscii_logical_visual_t*) logvis ;

	for( row = 0 ; row < logvis->model->num_of_rows ; row ++)
	{
		if( ! ml_line_iscii_visual( ml_model_get_line( logvis->model , row)))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " visualize row %d failed.\n" , row) ;
		#endif
		}
	}

	iscii_logvis->cursor_logical_char_index = logvis->cursor->char_index ;
	iscii_logvis->cursor_logical_col = logvis->cursor->col ;

	logvis->cursor->char_index = ml_line_iscii_convert_logical_char_index_to_visual(
					CURSOR_LINE(logvis) , logvis->cursor->char_index) ;
	logvis->cursor->col = ml_convert_char_index_to_col( CURSOR_LINE(logvis) ,
				logvis->cursor->char_index , 0) + logvis->cursor->col_in_char ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [col %d index %d]\n" ,
		logvis->cursor->col , logvis->cursor->char_index) ;
#endif

	logvis->is_visual = 1 ;
	
	return  1 ;
}

static int
iscii_logical(
	ml_logical_visual_t *  logvis
	)
{
	iscii_logical_visual_t *  iscii_logvis ;
	int  row ;

	if( ! logvis->is_visual)
	{
		return  0 ;
	}

	iscii_logvis = (iscii_logical_visual_t*) logvis ;

	for( row = 0 ; row < logvis->model->num_of_rows ; row ++)
	{
		ml_line_t *  line ;

		line = ml_model_get_line( logvis->model , row) ;

		if( ! ml_line_iscii_logical( line))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " logicalize row %d failed.\n" , row) ;
		#endif
		}

		/* XXX see ml_iscii_visual */
	#if  1
		if( line->num_of_chars > logvis->model->num_of_cols)
		{
			ml_str_final( line->chars + logvis->model->num_of_cols ,
				line->num_of_chars - logvis->model->num_of_cols) ;
			line->num_of_chars = logvis->model->num_of_cols ;
		}
	#endif
	}

	logvis->cursor->char_index = iscii_logvis->cursor_logical_char_index ;
	logvis->cursor->col = iscii_logvis->cursor_logical_col ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [col %d index %d]\n" ,
		logvis->cursor->col , logvis->cursor->char_index) ;
#endif

	logvis->is_visual = 0 ;

	return  1 ;
}

static int
iscii_visual_line(
	ml_logical_visual_t *  logvis ,
	ml_line_t *  line
	)
{
	iscii_render_line( logvis , line) ;
	ml_line_iscii_visual( line) ;

	return  1 ;
}


/* --- global functions --- */

ml_logical_visual_t *
ml_logvis_iscii_new(void)
{
	iscii_logical_visual_t *  iscii_logvis ;

	if( ( iscii_logvis = calloc( 1 , sizeof( iscii_logical_visual_t))) == NULL)
	{
		return  NULL ;
	}

	iscii_logvis->logvis.delete = iscii_delete ;
	iscii_logvis->logvis.init = iscii_init ;
	iscii_logvis->logvis.logical_cols = iscii_logical_cols ;
	iscii_logvis->logvis.logical_rows = iscii_logical_rows ;
	iscii_logvis->logvis.render = iscii_render ;
	iscii_logvis->logvis.visual = iscii_visual ;
	iscii_logvis->logvis.logical = iscii_logical ;
	iscii_logvis->logvis.visual_line = iscii_visual_line ;

	return  (ml_logical_visual_t*) iscii_logvis ;
}
