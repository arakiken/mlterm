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
	
	ml_line_t *  logical_lines ;
	
	u_int  logical_num_of_cols ;
	u_int  logical_num_of_rows ;

	int  cursor_logical_char_index ;
	int  cursor_logical_col ;

} iscii_logical_visual_t ;


/* --- static functions --- */

/*
 * ISCII logical <=> visual methods
 */

static int
iscii_delete_cached_lines(
	iscii_logical_visual_t *  iscii_logvis
	)
{
	int  row ;

	for( row = 0 ; row < iscii_logvis->logical_num_of_rows ; row ++)
	{
		ml_line_final( &iscii_logvis->logical_lines[row]) ;
	}

	free( iscii_logvis->logical_lines) ;
	iscii_logvis->logical_lines = NULL ;
	iscii_logvis->logical_num_of_rows = 0 ;
	iscii_logvis->logical_num_of_cols = 0 ;

	return  1 ;
}

static int
iscii_delete(
	ml_logical_visual_t *  logvis
	)
{
	int  row ;

	iscii_delete_cached_lines( (iscii_logical_visual_t*) logvis) ;
	
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

	return  iscii_delete_cached_lines( (iscii_logical_visual_t*) logvis) ;
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

static int
iscii_render(
	ml_logical_visual_t *  logvis
	)
{
	ml_line_t *  line ;
	int  row ;

	for( row = 0 ; row < logvis->model->num_of_rows ; row ++)
	{
		int  need_render ;

		line = ml_model_get_line( logvis->model , row) ;

		if( ml_line_is_empty( line))
		{
			continue ;
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

	return  1 ;
}

static int
copy_color_reversed_flag(
	ml_line_t *  dst ,
	ml_line_t *  src
	)
{
	int  col ;
	u_int  copy_len ;
	
	copy_len = K_MIN(src->num_of_filled_chars,dst->num_of_filled_chars) ;

	for( col = 0 ; col < copy_len ; col ++)
	{
		ml_char_copy_color_reversed_flag( dst->chars + col , src->chars + col) ;
	}

	return  1 ;
}

static int
iscii_visual(
	ml_logical_visual_t *  logvis
	)
{
	iscii_logical_visual_t *  iscii_logvis ;
	ml_line_t *  line ;
	int  row ;

	if( logvis->is_visual)
	{
		return  0 ;
	}

	iscii_logvis = (iscii_logical_visual_t*) logvis ;

	if( iscii_logvis->logical_num_of_rows != logvis->model->num_of_rows ||
		iscii_logvis->logical_num_of_cols != logvis->model->num_of_cols)
	{
		/* ml_model_t resized */
		
		void *  p ;

		if( iscii_logvis->logical_lines)
		{
			for( row = 0 ; row < iscii_logvis->logical_num_of_rows ; row ++)
			{
				ml_line_final( &iscii_logvis->logical_lines[row]) ;
			}
		}

		if( iscii_logvis->logical_num_of_rows != logvis->model->num_of_rows)
		{
			if( ( p = realloc( iscii_logvis->logical_lines ,
					sizeof( ml_line_t) * logvis->model->num_of_rows)) == NULL)
			{
				free( iscii_logvis->logical_lines) ;
				iscii_logvis->logical_lines = NULL ;
				
				return  0 ;
			}

			iscii_logvis->logical_lines = p ;
			
			iscii_logvis->logical_num_of_rows = logvis->model->num_of_rows ;
		}
		
		for( row = 0 ; row < iscii_logvis->logical_num_of_rows ; row ++)
		{
			ml_line_init( &iscii_logvis->logical_lines[row] ,
					logvis->model->num_of_cols) ;
		}

		iscii_logvis->logical_num_of_cols = logvis->model->num_of_cols ;
	}

	for( row = 0 ; row < logvis->model->num_of_rows ; row ++)
	{
		line = ml_model_get_line( logvis->model , row) ;

		if( ml_line_is_empty( line))
		{
			continue ;
		}

		/* caching */
		ml_line_copy_line( &iscii_logvis->logical_lines[row] , line) ;

		ml_line_iscii_visual( line) ;

		/* XXX Adhoc implementation for shrinking the amount of memory usage XXX */
		copy_color_reversed_flag( line , &iscii_logvis->logical_lines[row]) ;
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
		ml_line_t *  vis_line ;

		vis_line = ml_model_get_line( logvis->model , row) ;

		/*
		 * XXX Adhoc implementation for shrinking the amount of memory usage XXX
		 *
		 * If logical lines are changed by vt sequences,
		 * reversed flag will be all cleared.
		 */
		copy_color_reversed_flag( &iscii_logvis->logical_lines[row] , vis_line) ;

		/*
		 * If line is drawin in visual mode, cache lines are also updated.
		 */
		if( ! ml_line_is_modified( vis_line))
		{
			ml_line_set_updated( &iscii_logvis->logical_lines[row]) ;
		}
		
		ml_line_copy_line( vis_line , &iscii_logvis->logical_lines[row]) ;
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
	ml_line_iscii_visual( line) ;
	
	return  1 ;
}


/* --- global functions --- */

ml_logical_visual_t *
ml_logvis_iscii_new(void)
{
	iscii_logical_visual_t *  iscii_logvis ;

	if( ( iscii_logvis = malloc( sizeof( iscii_logical_visual_t))) == NULL)
	{
		return  NULL ;
	}

	iscii_logvis->logical_lines = NULL ;
	iscii_logvis->cursor_logical_char_index = 0 ;
	iscii_logvis->cursor_logical_col = 0 ;

	iscii_logvis->logvis.model = NULL ;
	iscii_logvis->logvis.cursor = NULL ;
	iscii_logvis->logvis.is_visual = 0 ;
	
	iscii_logvis->logical_num_of_cols = 0 ;
	iscii_logvis->logical_num_of_rows = 0 ;
	
	iscii_logvis->logvis.delete = iscii_delete ;
	iscii_logvis->logvis.init = iscii_init ;
	iscii_logvis->logvis.logical_cols = iscii_logical_cols ;
	iscii_logvis->logvis.logical_rows = iscii_logical_rows ;
	iscii_logvis->logvis.render = iscii_render ;
	iscii_logvis->logvis.visual = iscii_visual ;
	iscii_logvis->logvis.logical = iscii_logical ;
	iscii_logvis->logvis.visual_line = iscii_visual_line ;

	iscii_logvis->logvis.is_reversible = 0 ;

	return  (ml_logical_visual_t*) iscii_logvis ;
}
