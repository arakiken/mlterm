/*
 *	$Id$
 */

#include  "ml_logical_visual.h"

#include  <kiklib/kik_mem.h>		/* realloc/free */
#include  <kiklib/kik_util.h>		/* K_MIN */
#include  <kiklib/kik_debug.h>		/* kik_msg_printf */
#include  <kiklib/kik_str.h>            /* strcmp */

#define  CURSOR_LINE(logvis)  (ml_model_get_line((logvis)->model,(logvis)->cursor->row))

#if  0
#define  __DEBUG
#endif

#if  0
#define  CURSOR_DEBUG
#endif


typedef struct  container_logical_visual
{
	ml_logical_visual_t  logvis ;

	/*
	 * visual : children[0] => children[1] => ... => children[n]
	 * logical: children[n] => ... => children[1] => children[0]
	 */
	ml_logical_visual_t **  children ;
	u_int  num_of_children ;

} container_logical_visual_t ;

typedef struct  bidi_logical_visual
{
	ml_logical_visual_t  logvis ;
	
	int  cursor_logical_char_index ;
	int  cursor_logical_col ;
	int  ltr_rtl_meet_pos ;
	ml_bidi_mode_t  bidi_mode ;

	int8_t  adhoc_right_align ;

} bidi_logical_visual_t ;

typedef struct  comb_logical_visual
{
	ml_logical_visual_t  logvis ;
	
	int  cursor_logical_char_index ;
	int  cursor_logical_col ;	

} comb_logical_visual_t ;

typedef struct  iscii_logical_visual
{
	ml_logical_visual_t  logvis ;
	
	ml_line_t *  logical_lines ;
	
	u_int  logical_num_of_cols ;
	u_int  logical_num_of_rows ;

	int  cursor_logical_char_index ;
	int  cursor_logical_col ;

} iscii_logical_visual_t ;

typedef struct  vert_logical_visual
{
	ml_logical_visual_t  logvis ;

	ml_model_t  logical_model ;
	ml_model_t  visual_model ;

	int  cursor_logical_char_index ;
	int  cursor_logical_col ;
	int  cursor_logical_row ;

	int8_t  is_init ;

} vert_logical_visual_t ;


/* --- static variables --- */

/* Order of this table must be same as x_vertical_mode_t. */
static char *   vertical_mode_name_table[] =
{
	"none" , "mongol" , "cjk" ,
} ;


/* --- static functions --- */

static int
container_delete(
	ml_logical_visual_t *  logvis
	)
{
	container_logical_visual_t *  container ;
	int  count ;

	container = (container_logical_visual_t*) logvis ;

	if( container->num_of_children)
	{
		for( count = container->num_of_children - 1 ; count >= 0 ; count --)
		{
			(*container->children[count]->delete)( container->children[count]) ;
		}
	}

	free( container->children) ;
	
	free( logvis) ;

	return  1 ;
}

static int
container_init(
	ml_logical_visual_t *  logvis ,
	ml_model_t *  model ,
	ml_cursor_t *  cursor
	)
{
	container_logical_visual_t *  container ;
	int  count ;

	logvis->model = model ;
	logvis->cursor = cursor ;

	container = (container_logical_visual_t*) logvis ;

	for( count = 0 ; count < container->num_of_children ; count ++)
	{
		(*container->children[count]->init)( container->children[count] , model , cursor) ;
	}

	return  1 ;
}

static u_int
container_logical_cols(
	ml_logical_visual_t *  logvis
	)
{
	container_logical_visual_t *  container ;

	container = (container_logical_visual_t*) logvis ;
	
	if( container->num_of_children > 0)
	{
		return  (*container->children[container->num_of_children - 1]->logical_cols)(
			container->children[container->num_of_children - 1]) ;
	}
	else
	{
		return  logvis->model->num_of_cols ;
	}
}

static u_int
container_logical_rows(
	ml_logical_visual_t *  logvis
	)
{
	container_logical_visual_t *  container ;

	container = (container_logical_visual_t*) logvis ;
	
	if( container->num_of_children > 0)
	{
		return  (*container->children[container->num_of_children - 1]->logical_rows)(
			container->children[container->num_of_children - 1]) ;
	}
	else
	{
		return  logvis->model->num_of_rows ;
	}
}

static int
container_render(
	ml_logical_visual_t *  logvis
	)
{
	container_logical_visual_t *  container ;
	int  count ;

	container = (container_logical_visual_t*) logvis ;

	/*
	 * XXX
	 * only the first children can render correctly.
	 */
	for( count = 0 ; count < container->num_of_children ; count ++)
	{
		(*container->children[count]->render)( container->children[count]) ;
	}

	return  1 ;
}

static int
container_visual(
	ml_logical_visual_t *  logvis
	)
{
	container_logical_visual_t *  container ;
	int  count ;

	if( logvis->is_visual)
	{
		return  0 ;
	}

	container = (container_logical_visual_t*) logvis ;

	for( count = 0 ; count < container->num_of_children ; count ++)
	{
		(*container->children[count]->visual)( container->children[count]) ;
	}

	logvis->is_visual = 1 ;

	return  1 ;
}

static int
container_logical(
	ml_logical_visual_t *  logvis
	)
{
	container_logical_visual_t *  container ;
	int  count ;

	if( ! logvis->is_visual)
	{
		return  0 ;
	}

	container = (container_logical_visual_t*) logvis ;

	if( container->num_of_children == 0)
	{
		return  1 ;
	}

	for( count = container->num_of_children - 1 ; count >= 0  ; count --)
	{
		(*container->children[count]->logical)( container->children[count]) ;
	}

	logvis->is_visual = 0 ;

	return  1 ;
}

static int
container_visual_line(
	ml_logical_visual_t *  logvis ,
	ml_line_t *  line
	)
{
	container_logical_visual_t *  container ;
	int  count ;

	container = (container_logical_visual_t*) logvis ;

	for( count = 0 ; count < container->num_of_children ; count ++)
	{
		(*container->children[count]->visual_line)( container->children[count] , line) ;
	}

	return  1 ;
}


#ifdef  USE_FRIBIDI

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
			ml_line_unuse_bidi( &logvis->model->lines[row]) ;
		}

		free( logvis) ;
	}
	
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
			ml_line_unuse_bidi( &logvis->model->lines[row]) ;
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
		
		if( ((bidi_logical_visual_t*)logvis)->adhoc_right_align &&
			line->num_of_filled_chars > 0)
		{
			ml_line_fill( line , ml_sp_ch() , line->num_of_filled_chars ,
				logvis->model->num_of_cols - ml_line_get_num_of_filled_cols( line)) ;
			need_render = 1 ;
		}

		if( ! ml_line_is_using_bidi( line))
		{
			ml_line_use_bidi( line) ;

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

	logvis->cursor->char_index = ml_bidi_convert_logical_char_index_to_visual(
					CURSOR_LINE(logvis) , logvis->cursor->char_index ,
					&((bidi_logical_visual_t*)logvis)->ltr_rtl_meet_pos) ;
	/*
	 * XXX
	 * col_in_char should not be plused to col, because the character pointed by
	 * ml_bidi_convert_logical_char_index_to_visual() is not the same as the one
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
	
	if( ((bidi_logical_visual_t*)logvis)->adhoc_right_align && line->num_of_filled_chars > 0)
	{
		ml_line_fill( line , ml_sp_ch() , line->num_of_filled_chars ,
			logvis->model->num_of_cols - ml_line_get_num_of_filled_cols( line)) ;
		need_render = 1 ;
	}

	if( ! ml_line_is_using_bidi( line))
	{
		ml_line_use_bidi( line) ;

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

#endif


/*
 * dynamic combining
 */
 
static int
comb_delete(
	ml_logical_visual_t *  logvis
	)
{
	free( logvis) ;
	
	return  1 ;
}

static int
comb_init(
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
comb_logical_cols(
	ml_logical_visual_t *  logvis
	)
{
	return  logvis->model->num_of_cols ;
}

static u_int
comb_logical_rows(
	ml_logical_visual_t *  logvis
	)
{
	return  logvis->model->num_of_rows ;
}

static int
comb_render(
	ml_logical_visual_t *  logvis
	)
{
	return  (*logvis->visual)( logvis) ;
}

static int
comb_visual(
	ml_logical_visual_t *  logvis
	)
{
	int  row ;

	if( logvis->is_visual)
	{
		return  0 ;
	}

	((comb_logical_visual_t*)logvis)->cursor_logical_char_index = logvis->cursor->char_index ;
	((comb_logical_visual_t*)logvis)->cursor_logical_col = logvis->cursor->col ;
	
	for( row = 0 ; row < logvis->model->num_of_rows ; row ++)
	{
		ml_line_t *  line ;
		int  dst_pos ;
		int  src_pos ;
		ml_char_t *  cur ;

		line = ml_model_get_line( logvis->model , row) ;
		
		dst_pos = 0 ;
		cur = line->chars ;
		for( src_pos = 0 ; src_pos < line->num_of_filled_chars ; src_pos ++)
		{
			if( dst_pos > 0 && (ml_char_is_comb( cur) ||
				ml_is_arabic_combining(
					dst_pos >= 2 ? ml_char_at( line , dst_pos - 2) : NULL ,
					ml_char_at( line , dst_pos - 1) , cur)))
			{
				ml_char_combine_simple( ml_char_at( line , dst_pos - 1) , cur) ;

			#if  0
				/*
				 * This doesn't work as expected, for example, when
				 * one of combined two characters are deleted.
				 */
				if( ml_line_is_modified( line))
				{
					int  beg ;
					int  end ;

					beg = ml_line_get_beg_of_modified( line) ;
					end = ml_line_get_end_of_modified( line) ;
					
					if( beg > dst_pos - 1)
					{
						beg -- ;
					}

					if( end > dst_pos - 1)
					{
						end -- ;
					}

					ml_line_updated( line) ;
					ml_line_set_modified( line , beg , end) ;
				}
			#endif
			}
			else
			{
				ml_char_copy( ml_char_at( line , dst_pos ++) , cur) ;
			}

			if( row == logvis->cursor->row && src_pos == logvis->cursor->char_index)
			{
				logvis->cursor->char_index = dst_pos - 1 ;
				logvis->cursor->col = ml_convert_char_index_to_col( CURSOR_LINE(logvis) ,
							logvis->cursor->char_index , 0) +
							logvis->cursor->col_in_char ;
			}
			
			cur ++ ;
		}

	#if  1
		if( ml_line_is_modified( line))
		{
			ml_line_set_modified_all( line) ;
		}
	#endif
	
		line->num_of_filled_chars = dst_pos ;
	}

	logvis->is_visual = 1 ;

	return  1 ;
}

static int
comb_logical(
	ml_logical_visual_t *  logvis
	)
{
	ml_char_t *  buf ;
	int  row ;

	if( ! logvis->is_visual)
	{
		return  0 ;
	}

	if( ( buf = ml_str_alloca( logvis->model->num_of_cols)) == NULL)
	{
		return  0 ;
	}

	for( row = 0 ; row < logvis->model->num_of_rows ; row ++)
	{
		ml_line_t *  line ;
		int  src_pos ;
		u_int  src_len ;
		ml_char_t *  c ;

		line = ml_model_get_line( logvis->model , row) ;

		ml_str_copy( buf , line->chars , line->num_of_filled_chars) ;

		src_len = line->num_of_filled_chars ;
		line->num_of_filled_chars = 0 ;
		c = buf ;
		for( src_pos = 0 ; src_pos < src_len && line->num_of_filled_chars < line->num_of_chars ;
			src_pos ++)
		{
			ml_char_t *  comb ;
			u_int  size ;

			if( ( comb = ml_get_combining_chars( c , &size)))
			{
				int  count ;

				ml_char_copy( ml_char_at( line , line->num_of_filled_chars ++) ,
					ml_get_base_char(c)) ;

				for( count = 0 ; count < size ; count ++)
				{
					if( line->num_of_filled_chars >= line->num_of_chars)
					{
						break ;
					}

				#if  0
					/*
					 * This doesn't work as expected, for example, when
					 * one of combined two characters are deleted.
					 */
					if( ml_line_is_modified( line))
					{
						int  beg ;
						int  end ;
						int  is_cleared_to_end ;

						beg = ml_line_get_beg_of_modified( line) ;
						end = ml_line_get_end_of_modified( line) ;
						
						if( beg > src_pos)
						{
							beg ++ ;
						}

						if( end > src_pos)
						{
							end ++ ;
						}

						ml_line_updated( line) ;
						ml_line_set_modified( line , beg , end) ;
					}
				#endif

					ml_char_copy( ml_char_at( line , line->num_of_filled_chars ++) ,
						comb) ;

					comb ++ ;
				}
			}
			else
			{
				ml_char_copy( ml_char_at( line , line->num_of_filled_chars ++) , c) ;
			}

			c ++ ;
		}
	}
	
	ml_str_final( buf , logvis->model->num_of_cols) ;

	logvis->cursor->char_index = ((bidi_logical_visual_t*)logvis)->cursor_logical_char_index ;
	logvis->cursor->col = ((bidi_logical_visual_t*)logvis)->cursor_logical_col ;
	
	logvis->is_visual = 0 ;

	return  1 ;
}

static int
comb_visual_line(
	ml_logical_visual_t *  logvis ,
	ml_line_t *  line
	)
{
	int  dst_pos ;
	int  src_pos ;
	ml_char_t *  cur ;

	dst_pos = 0 ;
	cur = line->chars ;
	for( src_pos = 0 ; src_pos < line->num_of_filled_chars ; src_pos ++)
	{
		if( dst_pos > 0 && (ml_char_is_comb( cur) ||
			ml_is_arabic_combining( dst_pos >= 2 ? ml_char_at( line , dst_pos - 2) : NULL ,
				ml_char_at( line , dst_pos - 1) , cur)))
		{
			ml_char_combine_simple( ml_char_at( line , dst_pos - 1) , cur) ;
		}
		else
		{
			ml_char_copy( ml_char_at( line , dst_pos ++) , cur) ;
		}

		cur ++ ;
	}

	line->num_of_filled_chars = dst_pos ;

	return  1 ;
}


#ifdef  USE_IND

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
	iscii_delete_cached_lines( (iscii_logical_visual_t*) logvis) ;
	
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
			ml_line_use_iscii( line) ;

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
	
	logvis->cursor->char_index = ml_iscii_convert_logical_char_index_to_visual(
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
			ml_line_updated( &iscii_logvis->logical_lines[row]) ;
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

#endif


/*
 * vertical view logical <=> visual methods
 */
 
static int
vert_delete(
	ml_logical_visual_t *  logvis
	)
{
	vert_logical_visual_t *  vert_logvis ;

	if( logvis->model)
	{
		vert_logvis = (vert_logical_visual_t*) logvis ;
		ml_model_final( &vert_logvis->visual_model) ;
	}
	
	free( logvis) ;
	
	return  1 ;
}

static int
vert_init(
	ml_logical_visual_t *  logvis ,
	ml_model_t *  model ,
	ml_cursor_t *  cursor
	)
{
	vert_logical_visual_t *  vert_logvis ;

	vert_logvis = (vert_logical_visual_t*) logvis ;
	
	if( vert_logvis->is_init)
	{
		ml_model_resize( &vert_logvis->visual_model , NULL ,
			model->num_of_rows , model->num_of_cols) ;
	}
	else
	{
		ml_model_init( &vert_logvis->visual_model ,
			model->num_of_rows , model->num_of_cols) ;
		vert_logvis->is_init = 1 ;
	}
	
	vert_logvis->logical_model = *model ;
	
	logvis->model = model ;
	logvis->cursor = cursor ;

	return  1 ;
}

static u_int
vert_logical_cols(
	ml_logical_visual_t *  logvis
	)
{
	if( logvis->is_visual)
	{
		return  ((vert_logical_visual_t*)logvis)->logical_model.num_of_cols ;
	}
	else
	{
		return  logvis->model->num_of_cols ;
	}
}

static u_int
vert_logical_rows(
	ml_logical_visual_t *  logvis
	)
{
	if( logvis->is_visual)
	{
		return  ((vert_logical_visual_t*)logvis)->logical_model.num_of_rows ;
	}
	else
	{
		return  logvis->model->num_of_rows ;
	}
}

static int
vert_render(
	ml_logical_visual_t *  logvis
	)
{
	return  1 ;
}

static int
vert_visual_intern(
	ml_logical_visual_t *  logvis ,
	ml_vertical_mode_t  mode
	)
{
	vert_logical_visual_t *  vert_logvis ;
	ml_line_t *  log_line ;
	ml_line_t *  vis_line ;
	int  row ;
	int  count ;

	if( logvis->is_visual)
	{
		return  0 ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " logical cursor [col %d index %d row %d]\n" ,
		logvis->cursor->col , logvis->cursor->char_index , logvis->cursor->row) ;
#endif

	vert_logvis = (vert_logical_visual_t*) logvis ;

	if( vert_logvis->logical_model.num_of_rows != logvis->model->num_of_rows ||
		vert_logvis->logical_model.num_of_cols != logvis->model->num_of_cols)
	{
		/* ml_model_t is resized */

		ml_model_resize( &vert_logvis->visual_model , NULL ,
			logvis->model->num_of_rows , logvis->model->num_of_cols) ;
	}

	ml_model_reset( &vert_logvis->visual_model) ;

	if( mode & VERT_LTR)
	{
		/* Mongol */
		
		count = -1 ;
	}
	else
	{
		/* CJK */
		
		count = logvis->model->num_of_rows ;
	}

	while( 1)
	{
		if( mode & VERT_LTR)
		{
			/* Mongol */
			
			if( ++ count >= logvis->model->num_of_rows)
			{
				break ;
			}
		}
		else
		{
			/* CJK */
			
			if( -- count < 0)
			{
				break ;
			}
		}

		log_line = ml_model_get_line( logvis->model , count) ;

		for( row = 0 ; row < log_line->num_of_filled_chars ; row ++)
		{
			vis_line = ml_model_get_line( &vert_logvis->visual_model , row) ;

			if( vis_line == NULL || vis_line->num_of_filled_chars >= vis_line->num_of_chars)
			{
				continue ;
			}

			ml_char_copy( ml_char_at( vis_line , vis_line->num_of_filled_chars ++) ,
				ml_char_at( log_line , row)) ;
			
			if( ml_line_is_modified( log_line) &&
				ml_line_get_beg_of_modified( log_line) <= row &&
				(ml_line_is_cleared_to_end( log_line) ||
				row <= ml_line_get_end_of_modified( log_line)))
			{
				ml_line_set_modified( vis_line ,
					vis_line->num_of_filled_chars - 1 ,
					vis_line->num_of_filled_chars - 1) ;
			}
		}

		for( ; row < vert_logvis->visual_model.num_of_rows ; row ++)
		{
			vis_line = ml_model_get_line( &vert_logvis->visual_model , row) ;
			
			if( vis_line == NULL || vis_line->num_of_filled_chars + 1 > vis_line->num_of_chars)
			{
				continue ;
			}
			
			ml_char_copy( ml_char_at( vis_line , vis_line->num_of_filled_chars ++) ,
				ml_sp_ch()) ;
			
			if( ml_line_is_modified( log_line) &&
				ml_line_get_beg_of_modified( log_line) <= row &&
				(ml_line_is_cleared_to_end( log_line) ||
				row <= ml_line_get_end_of_modified( log_line)))
			{
				ml_line_set_modified( vis_line ,
					vis_line->num_of_filled_chars - 1 ,
					vis_line->num_of_filled_chars - 1) ;
			}
		}
	}

	vert_logvis->logical_model = *logvis->model ;
	*logvis->model = vert_logvis->visual_model ;

	vert_logvis->cursor_logical_char_index = logvis->cursor->char_index ;
	vert_logvis->cursor_logical_col = logvis->cursor->col ;
	vert_logvis->cursor_logical_row = logvis->cursor->row ;

	logvis->cursor->row = vert_logvis->cursor_logical_char_index ;
	logvis->cursor->char_index = logvis->cursor->col = 0 ;

	if( mode & VERT_LTR)
	{
		/* Mongol */
		
		logvis->cursor->col = logvis->cursor->char_index = vert_logvis->cursor_logical_row ;
	}
	else
	{
		/* CJK */
		
		logvis->cursor->col = logvis->cursor->char_index =
			vert_logvis->logical_model.num_of_rows - vert_logvis->cursor_logical_row - 1 ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " visual cursor [col %d index %d row %d]\n" ,
		logvis->cursor->col , logvis->cursor->char_index , logvis->cursor->row) ;
#endif

	logvis->is_visual = 1 ;
	
	return  1 ;
}

static int
cjk_vert_visual(
	ml_logical_visual_t *  logvis
	)
{
	return  vert_visual_intern( logvis , VERT_RTL) ;
}

static int
mongol_vert_visual(
	ml_logical_visual_t *  logvis
	)
{
	return  vert_visual_intern( logvis , VERT_LTR) ;
}

static int
vert_logical(
	ml_logical_visual_t *  logvis
	)
{
	vert_logical_visual_t *  vert_logvis ;

	if( ! logvis->is_visual)
	{
		return  0 ;
	}
	
	vert_logvis = (vert_logical_visual_t*) logvis ;
	
	*logvis->model = vert_logvis->logical_model ;

	logvis->cursor->char_index = vert_logvis->cursor_logical_char_index ;
	logvis->cursor->col = vert_logvis->cursor_logical_col ;
	logvis->cursor->row = vert_logvis->cursor_logical_row ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " logical cursor [col %d index %d row %d]\n" ,
		logvis->cursor->col , logvis->cursor->char_index , logvis->cursor->row) ;
#endif

	logvis->is_visual = 0 ;
	
	return  1 ;
}

static int
vert_visual_line(
	ml_logical_visual_t *  logvis ,
	ml_line_t *  line
	)
{
	return  1 ;
}


/* --- global functions --- */

ml_logical_visual_t *
ml_logvis_container_new(void)
{
	container_logical_visual_t *  container ;

	if( ( container = malloc( sizeof( container_logical_visual_t))) == NULL)
	{
		return  NULL ;
	}

	container->children = NULL ;
	container->num_of_children = 0 ;
	
	container->logvis.model = NULL ;
	container->logvis.cursor = NULL ;
	container->logvis.is_visual = 0 ;

	container->logvis.delete = container_delete ;
	container->logvis.init = container_init ;
	container->logvis.logical_cols = container_logical_cols ;
	container->logvis.logical_rows = container_logical_rows ;
	container->logvis.render = container_render ;
	container->logvis.visual = container_visual ;
	container->logvis.logical = container_logical ;
	container->logvis.visual_line = container_visual_line ;

	return  (ml_logical_visual_t*) container ;
}

int
ml_logvis_container_add(
	ml_logical_visual_t *  logvis ,
	ml_logical_visual_t *  child
	)
{
	void *  p ;
	container_logical_visual_t *  container ;

	container = (container_logical_visual_t*) logvis ;

	if( ( p = realloc( container->children ,
		(container->num_of_children + 1) * sizeof( ml_logical_visual_t))) == NULL)
	{
		return  0 ;
	}

	container->children = p ;
	
	container->children[container->num_of_children ++] = child ;

	return  1 ;
}

#ifdef  USE_FRIBIDI
ml_logical_visual_t *
ml_logvis_bidi_new(
	int  adhoc_right_align ,
	ml_bidi_mode_t   bidi_mode
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
	bidi_logvis->adhoc_right_align = adhoc_right_align ;

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

	return  (ml_logical_visual_t*) bidi_logvis ;
}
#endif

ml_logical_visual_t *
ml_logvis_comb_new(void)
{
	comb_logical_visual_t *  comb_logvis ;

	if( ( comb_logvis = malloc( sizeof( comb_logical_visual_t))) == NULL)
	{
		return  NULL ;
	}
	
	comb_logvis->cursor_logical_char_index = 0 ;
	comb_logvis->cursor_logical_col = 0 ;

	comb_logvis->logvis.model = NULL ;
	comb_logvis->logvis.cursor = NULL ;
	comb_logvis->logvis.is_visual = 0 ;
	
	comb_logvis->logvis.delete = comb_delete ;
	comb_logvis->logvis.init = comb_init ;
	comb_logvis->logvis.logical_cols = comb_logical_cols ;
	comb_logvis->logvis.logical_rows = comb_logical_rows ;
	comb_logvis->logvis.render = comb_render ;
	comb_logvis->logvis.visual = comb_visual ;
	comb_logvis->logvis.logical = comb_logical ;
	comb_logvis->logvis.visual_line = comb_visual_line ;

	return  (ml_logical_visual_t*) comb_logvis ;
}

#ifdef  USE_IND
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

	return  (ml_logical_visual_t*) iscii_logvis ;
}
#endif

ml_logical_visual_t *
ml_logvis_vert_new(
	ml_vertical_mode_t  vertical_mode
	)
{
	vert_logical_visual_t *  vert_logvis ;

	if( vertical_mode != VERT_RTL && vertical_mode != VERT_LTR)
	{
		return  NULL ;
	}

	if( ( vert_logvis = malloc( sizeof( vert_logical_visual_t))) == NULL)
	{
		return  NULL ;
	}

	vert_logvis->is_init = 0 ;
	
	vert_logvis->cursor_logical_char_index = 0 ;
	vert_logvis->cursor_logical_col = 0 ;
	vert_logvis->cursor_logical_row = 0 ;

	vert_logvis->logvis.model = NULL ;
	vert_logvis->logvis.cursor = NULL ;
	vert_logvis->logvis.is_visual = 0 ;

	vert_logvis->logvis.delete = vert_delete ;
	vert_logvis->logvis.init = vert_init ;
	vert_logvis->logvis.logical_cols = vert_logical_cols ;
	vert_logvis->logvis.logical_rows = vert_logical_rows ;
	vert_logvis->logvis.render = vert_render ;
	vert_logvis->logvis.logical = vert_logical ;
	vert_logvis->logvis.visual_line = vert_visual_line ;

	if( vertical_mode == VERT_RTL)
	{
		/*
		 * CJK type vertical view
		 */

		vert_logvis->logvis.visual = cjk_vert_visual ;
	}
	else /* if( vertical_mode == VERT_LTR) */
	{
		/*
		 * mongol type vertical view
		 */
		 
		vert_logvis->logvis.visual = mongol_vert_visual ;
	}

	return  (ml_logical_visual_t*) vert_logvis ;
}


ml_vertical_mode_t
ml_get_vertical_mode(
	char *  name
	)
{
	ml_vertical_mode_t  mode ;

	for( mode = 0 ; mode < VERT_MODE_MAX ; mode++)
	{
		if( strcmp( vertical_mode_name_table[mode] , name) == 0)
		{
			return  mode ;
		}
	}
	
	/* default value */
	return  0 ;
}

char *
ml_get_vertical_mode_name(
	ml_vertical_mode_t  mode
	)
{
	if( mode < 0 || VERT_MODE_MAX <= mode)
	{
		/* default value */
		mode = 0 ;
	}

	return  vertical_mode_name_table[mode] ;
}
