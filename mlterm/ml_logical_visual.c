/*
 *	$Id$
 */

#include  "ml_logical_visual.h"

#include  <kiklib/kik_mem.h>		/* realloc/free */
#include  <kiklib/kik_util.h>		/* K_MIN */
#include  <kiklib/kik_debug.h>		/* kik_msg_printf */

#include  "ml_edit_intern.h"


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
	
	ml_iscii_state_t  iscii_state ;

	ml_line_t *  logical_lines ;
	ml_line_t *  visual_lines ;
	
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

} vert_logical_visual_t ;


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
	ml_edit_t *  edit
	)
{
	container_logical_visual_t *  container ;
	int  count ;

	logvis->edit = edit ;

	container = (container_logical_visual_t*) logvis ;

	for( count = 0 ; count < container->num_of_children ; count ++)
	{
		(*container->children[count]->init)( container->children[count] , edit) ;
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
		return  ml_edit_get_cols( logvis->edit) ;
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
		return  ml_edit_get_rows( logvis->edit) ;
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


/*
 * Bidi logical <=> visual methods
 */
 
static int
bidi_delete(
	ml_logical_visual_t *  logvis
	)
{
	int  row ;

	if( logvis->edit)
	{
		for( row = 0 ; row < logvis->edit->model.num_of_rows ; row ++)
		{
			ml_line_unuse_bidi( &logvis->edit->model.lines[row]) ;
		}

		free( logvis) ;
	}
	
	return  1 ;
}

static int
bidi_init(
	ml_logical_visual_t *  logvis ,
	ml_edit_t *  new_edit
	)
{
	int  row ;

	if( logvis->edit)
	{
		for( row = 0 ; row < logvis->edit->model.num_of_rows ; row ++)
		{
			ml_line_unuse_bidi( &logvis->edit->model.lines[row]) ;
		}
	}

	logvis->edit = new_edit ;
	
	return  1 ;
}

static u_int
bidi_logical_cols(
	ml_logical_visual_t *  logvis
	)
{
	return  ml_edit_get_cols( logvis->edit) ;
}

static u_int
bidi_logical_rows(
	ml_logical_visual_t *  logvis
	)
{
	return  ml_edit_get_rows( logvis->edit) ;
}

static int
bidi_render(
	ml_logical_visual_t *  logvis
	)
{
	ml_edit_t *  edit ;
	ml_line_t *  line ;
	int  row ;

	if( logvis->is_visual)
	{
		return  1 ;
	}

	edit = logvis->edit ;

	/*
	 * all lines(not only filled lines) should be rendered.
	 */
	for( row = 0 ; row < edit->model.num_of_rows ; row ++)
	{
		int  need_render ;

		line = ml_model_get_line( &edit->model , row) ;

		need_render = 0 ;
		
		if( ((bidi_logical_visual_t*)logvis)->adhoc_right_align && line->num_of_filled_chars > 0)
		{
			ml_line_fill( line , ml_sp_ch() , line->num_of_filled_chars ,
				edit->model.num_of_cols - ml_line_get_num_of_filled_cols( line)) ;
			need_render = 1 ;
		}

		if( ! ml_line_is_using_bidi( line))
		{
			ml_line_use_bidi( line) ;

			need_render = 1 ;
		}

		if( ml_line_is_modified( line) || need_render)
		{
			if( ! ml_line_bidi_render( line))
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
	ml_edit_t *  edit ;
	int  row ;
	u_int  cols_rest ;

	if( logvis->is_visual)
	{
		return  0 ;
	}
	
	edit = logvis->edit ;

#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

	ml_convert_col_to_char_index( CURSOR_LINE(edit) , &cols_rest , edit->cursor.col , 0) ;

	for( row = 0 ; row < edit->model.num_of_filled_rows ; row ++)
	{
		if( ! ml_line_bidi_visual( ml_model_get_line( &edit->model , row)))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " visualize row %d failed.\n" , row) ;
		#endif
		}
	}

	((bidi_logical_visual_t*)logvis)->cursor_logical_char_index = edit->cursor.char_index ;
	((bidi_logical_visual_t*)logvis)->cursor_logical_col = edit->cursor.col ;

	edit->cursor.char_index = ml_bidi_convert_logical_char_index_to_visual(
					CURSOR_LINE(edit) , edit->cursor.char_index ,
					&((bidi_logical_visual_t*)logvis)->ltr_rtl_meet_pos) ;
	edit->cursor.col = ml_convert_char_index_to_col( CURSOR_LINE(edit) ,
					edit->cursor.char_index , 0) + cols_rest ;

#ifdef  CURSOR_DEBUG
	kik_msg_printf( "-> [cursor(index)%d (col)%d (row)%d]\n" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

	logvis->is_visual = 1 ;

	return  1 ;
}

static int
bidi_logical(
	ml_logical_visual_t *  logvis
	)
{
	ml_edit_t *  edit ;
	int  row ;

	if( ! logvis->is_visual)
	{
		return  0 ;
	}
	
	edit = logvis->edit ;
	
#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

	for( row = 0 ; row < edit->model.num_of_filled_rows ; row ++)
	{
		if( ! ml_line_bidi_logical( ml_model_get_line( &edit->model , row)))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " visualize row %d failed.\n" , row) ;
		#endif
		}
	}

	edit->cursor.char_index = ((bidi_logical_visual_t*)logvis)->cursor_logical_char_index ;
	edit->cursor.col = ((bidi_logical_visual_t*)logvis)->cursor_logical_col ;

#ifdef  CURSOR_DEBUG
	kik_msg_printf( "-> [cursor(index)%d (col)%d (row)%d]\n" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
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
			logvis->edit->model.num_of_cols - ml_line_get_num_of_filled_cols( line)) ;
		need_render = 1 ;
	}

	if( ! ml_line_is_using_bidi( line))
	{
		ml_line_use_bidi( line) ;

		need_render = 1 ;
	}

	if( ml_line_is_modified( line) || need_render)
	{
		if( ! ml_line_bidi_render( line))
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
	ml_edit_t *  edit
	)
{
	logvis->edit = edit ;
	
	return  1 ;
}

static u_int
comb_logical_cols(
	ml_logical_visual_t *  logvis
	)
{
	return  ml_edit_get_cols( logvis->edit) ;
}

static u_int
comb_logical_rows(
	ml_logical_visual_t *  logvis
	)
{
	return  ml_edit_get_rows( logvis->edit) ;
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
	ml_edit_t *  edit ;
	int  row ;
	u_int  cols_rest ;

	if( logvis->is_visual)
	{
		return  0 ;
	}

	edit = logvis->edit ;

	((comb_logical_visual_t*)logvis)->cursor_logical_char_index = edit->cursor.char_index ;
	((comb_logical_visual_t*)logvis)->cursor_logical_col = edit->cursor.col ;
	
	ml_convert_col_to_char_index( CURSOR_LINE(edit) , &cols_rest , edit->cursor.col , 0) ;

	for( row = 0 ; row < edit->model.num_of_filled_rows ; row ++)
	{
		ml_line_t *  line ;
		int  dst_pos ;
		int  src_pos ;
		ml_char_t *  cur ;
		ml_char_t *  prev ;
		ml_char_t *  prev2 ;

		line = ml_model_get_line( &edit->model , row) ;
		
		dst_pos = 0 ;
		prev = NULL ;
		prev2 = NULL ;
		cur = line->chars ;
		for( src_pos = 0 ; src_pos < line->num_of_filled_chars ; src_pos ++)
		{
			if( prev && (ml_char_is_comb( cur) || ml_is_arabic_combining( prev2 , prev , cur)))
			{
				/* dst_pos must be over 0 since 'prev' is not NULL */
				ml_combine_chars( &line->chars[dst_pos - 1] , cur) ;

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
					ml_line_set_modified( line , beg , end , 1) ;
				}
			#endif
			}
			else
			{
				ml_char_copy( &line->chars[dst_pos ++] , cur) ;
			}

			if( row == edit->cursor.row && src_pos == edit->cursor.char_index)
			{
				edit->cursor.char_index = dst_pos - 1 ;
				edit->cursor.col = ml_convert_char_index_to_col( CURSOR_LINE(edit) ,
							edit->cursor.char_index , 0) + cols_rest ;
			}

			prev2 = prev ;
			prev = cur ;
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
	ml_edit_t *  edit ;
	ml_char_t *  buf ;
	int  row ;

	if( ! logvis->is_visual)
	{
		return  0 ;
	}
	
	edit = logvis->edit ;

	if( ( buf = ml_str_alloca( edit->model.num_of_cols)) == NULL)
	{
		return  0 ;
	}

	for( row = 0 ; row < edit->model.num_of_filled_rows ; row ++)
	{
		ml_line_t *  line ;
		int  dst_pos ;
		int  src_pos ;
		ml_char_t *  c ;

		line = ml_model_get_line( &edit->model , row) ;

		ml_str_copy( buf , line->chars , line->num_of_filled_chars) ;

		dst_pos = 0 ;
		c = buf ;
		for( src_pos = 0 ;
			src_pos < line->num_of_filled_chars && dst_pos < line->num_of_chars ;
			src_pos ++)
		{
			ml_char_t *  comb ;
			u_int  size ;

			if( ( comb = ml_get_combining_chars( c , &size)))
			{
				int  count ;

				ml_char_copy( &line->chars[dst_pos ++] , ml_get_base_char(c)) ;

				for( count = 0 ; count < size ; count ++)
				{
					if( dst_pos >= line->num_of_chars)
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
						is_cleared_to_end = ml_line_is_cleared_to_end( line) ;
						
						if( beg > src_pos)
						{
							beg ++ ;
						}

						if( end > src_pos)
						{
							end ++ ;
						}

						ml_line_updated( line) ;
						ml_line_set_modified( line , beg , end ,
							is_cleared_to_end) ;
					}
				#endif

					ml_char_copy( &line->chars[dst_pos ++] , comb) ;

					comb ++ ;
				}
			}
			else
			{
				ml_char_copy( &line->chars[dst_pos ++] , c) ;
			}

			c ++ ;
		}

		line->num_of_filled_chars = dst_pos ;
	}
	
	ml_str_final( buf , edit->model.num_of_cols) ;

	edit->cursor.char_index = ((bidi_logical_visual_t*)logvis)->cursor_logical_char_index ;
	edit->cursor.col = ((bidi_logical_visual_t*)logvis)->cursor_logical_col ;
	
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
	ml_char_t *  prev ;
	ml_char_t *  prev2 ;

	dst_pos = 0 ;
	prev = NULL ;
	prev2 = NULL ;
	cur = line->chars ;
	for( src_pos = 0 ; src_pos < line->num_of_filled_chars ; src_pos ++)
	{
		if( prev && (ml_char_is_comb( cur) || ml_is_arabic_combining( prev2 , prev , cur)))
		{
			/* dst_pos must be over 0 since 'prev' is not NULL */
			ml_combine_chars( &line->chars[dst_pos - 1] , cur) ;
		}
		else
		{
			ml_char_copy( &line->chars[dst_pos ++] , cur) ;
		}

		prev2 = prev ;
		prev = cur ;
		cur ++ ;
	}

	line->num_of_filled_chars = dst_pos ;

	return  1 ;
}


/*
 * ISCII logical <=> visual methods
 */
 
static int
iscii_delete(
	ml_logical_visual_t *  logvis
	)
{
	iscii_logical_visual_t *  iscii_logvis ;
	int  row ;

	iscii_logvis = (iscii_logical_visual_t*) logvis ;
	
	for( row = 0 ; row < iscii_logvis->logical_num_of_rows ; row ++)
	{
		ml_line_final( &iscii_logvis->visual_lines[row]) ;
		ml_line_final( &iscii_logvis->logical_lines[row]) ;
	}

	free( iscii_logvis->visual_lines) ;
	free( iscii_logvis->logical_lines) ;
	
	free( logvis) ;
	
	return  1 ;
}

static int
iscii_init(
	ml_logical_visual_t *  logvis ,
	ml_edit_t *  edit
	)
{
	logvis->edit = edit ;

	return  1 ;
}

static u_int
iscii_logical_cols(
	ml_logical_visual_t *  logvis
	)
{
	return  ml_edit_get_cols( logvis->edit) ;
}

static u_int
iscii_logical_rows(
	ml_logical_visual_t *  logvis
	)
{
	return  ml_edit_get_rows( logvis->edit) ;
}

static int
iscii_render(
	ml_logical_visual_t *  logvis
	)
{
	return  1 ;
}

static int
search_same_line(
	ml_line_t *  lines ,
	u_int  num_of_lines ,
	ml_line_t *  line
	)
{
	int  row ;

	for( row = 0 ; row < num_of_lines ; row ++)
	{
		if( lines[row].num_of_filled_chars == line->num_of_filled_chars &&
			ml_str_bytes_equal( lines[row].chars , line->chars , line->num_of_filled_chars))
		{
			return  row ;
		}
	}

	/* not found */
	return  -1 ;
}

static int
iscii_visual(
	ml_logical_visual_t *  logvis
	)
{
	iscii_logical_visual_t *  iscii_logvis ;
	ml_edit_t *  edit ;
	ml_line_t *  line ;
	int  row ;
	int  cols_rest ;

	if( logvis->is_visual)
	{
		return  0 ;
	}
	
	edit = logvis->edit ;

	iscii_logvis = (iscii_logical_visual_t*) logvis ;

	if( iscii_logvis->logical_num_of_rows != edit->model.num_of_rows ||
		iscii_logvis->logical_num_of_cols != edit->model.num_of_cols)
	{
		/* ml_edit_t is resized */
		
		if( iscii_logvis->logical_num_of_rows != edit->model.num_of_rows)
		{
			void *  p1 ;
			void *  p2 ;
			
			if( iscii_logvis->visual_lines)
			{
				for( row = 0 ; row < iscii_logvis->logical_num_of_rows ; row ++)
				{
					ml_line_final( &iscii_logvis->visual_lines[row]) ;
				}
			}

			if( iscii_logvis->logical_lines)
			{
				for( row = 0 ; row < iscii_logvis->logical_num_of_rows ; row ++)
				{
					ml_line_final( &iscii_logvis->logical_lines[row]) ;
				}
			}
			
			if( ( p1 = realloc( iscii_logvis->visual_lines ,
					sizeof( ml_line_t) * edit->model.num_of_rows)) == NULL ||
				( p2 = realloc( iscii_logvis->logical_lines ,
					sizeof( ml_line_t) * edit->model.num_of_rows)) == NULL)
			{
				free( iscii_logvis->visual_lines) ;
				iscii_logvis->visual_lines = NULL ;
				
				free( iscii_logvis->logical_lines) ;
				iscii_logvis->logical_lines = NULL ;
				
				return  0 ;
			}

			iscii_logvis->visual_lines = p1 ;
			iscii_logvis->logical_lines = p2 ;
			
			iscii_logvis->logical_num_of_rows = edit->model.num_of_rows ;
		}
		
		for( row = 0 ; row < iscii_logvis->logical_num_of_rows ; row ++)
		{
			ml_line_init( &iscii_logvis->visual_lines[row] , edit->model.num_of_cols) ;
			ml_line_init( &iscii_logvis->logical_lines[row] , edit->model.num_of_cols) ;
		}

		iscii_logvis->logical_num_of_cols = edit->model.num_of_cols ;
	}
	
	ml_convert_col_to_char_index( CURSOR_LINE(edit) , &cols_rest , edit->cursor.col , 0) ;
	
	for( row = 0 ; row < edit->model.num_of_filled_rows ; row ++)
	{
		int  is_cache_active ;

		line = ml_model_get_line( &edit->model , row) ;
		
		if( ml_line_is_modified( line))
		{
			is_cache_active = 0 ;
		}
		else if( iscii_logvis->logical_lines[row].num_of_filled_chars != line->num_of_filled_chars ||
			! ml_str_bytes_equal( iscii_logvis->logical_lines[row].chars ,
				line->chars , iscii_logvis->logical_lines[row].num_of_filled_chars))
		{
			int  hit_row ;
			
			if( ( hit_row = search_same_line( iscii_logvis->logical_lines ,
				iscii_logvis->logical_num_of_rows , line)) == -1)
			{
				is_cache_active = 0 ;
			}
			else
			{
				/*
				 * XXX
				 * this may break active cache for another line in "row" line.
				 * it is preferable to swap iscii_logvis->{logical|visual}_lines[row]
				 * and iscii_logvis->{logical|visual}_lines[hit_row] , but
				 * this also works anyway:)
				 */
				ml_line_copy_line( &iscii_logvis->logical_lines[row] ,
					&iscii_logvis->logical_lines[hit_row]) ;
				ml_line_copy_line( &iscii_logvis->visual_lines[row] ,
					&iscii_logvis->visual_lines[hit_row]) ;
				
				is_cache_active = 1 ;
			}
		}
		else
		{
			is_cache_active = 1 ;
		}

		if( is_cache_active)
		{
			/* using cached line */
			ml_line_copy_line( line , &iscii_logvis->visual_lines[row]) ;
		}
		else
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " iscii rendering.\n") ;
		#endif
		
			ml_line_copy_line( &iscii_logvis->logical_lines[row] , line) ;
			ml_line_updated( &iscii_logvis->logical_lines[row]) ;

			ml_line_iscii_visual( line , iscii_logvis->iscii_state) ;

			/* caching */
			ml_line_copy_line( &iscii_logvis->visual_lines[row] , line) ;
			ml_line_updated( &iscii_logvis->visual_lines[row]) ;
		}
	}

	iscii_logvis->cursor_logical_char_index = edit->cursor.char_index ;
	iscii_logvis->cursor_logical_col = edit->cursor.col ;
	
	edit->cursor.char_index = ml_iscii_convert_logical_char_index_to_visual(
					CURSOR_LINE(edit) , edit->cursor.char_index) ;
	edit->cursor.col = ml_convert_char_index_to_col( CURSOR_LINE(edit) ,
				edit->cursor.char_index , 0) + cols_rest ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [col %d index %d]\n" ,
		edit->cursor.col , edit->cursor.char_index) ;
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
	ml_edit_t *  edit ;
	int  row ;

	if( ! logvis->is_visual)
	{
		return  0 ;
	}
	
	edit = logvis->edit ;

	iscii_logvis = (iscii_logical_visual_t*) logvis ;
	
	for( row = 0 ; row < edit->model.num_of_filled_rows ; row ++)
	{
		ml_line_copy_line( ml_model_get_line( &edit->model , row) ,
			&iscii_logvis->logical_lines[row]) ;
	}

	edit->cursor.char_index = iscii_logvis->cursor_logical_char_index ;
	edit->cursor.col = iscii_logvis->cursor_logical_col ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [col %d index %d]\n" ,
		edit->cursor.col , edit->cursor.char_index) ;
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
	ml_iscii_state_t  iscii_state ;

	iscii_state = ((iscii_logical_visual_t*) logvis)->iscii_state ;

	ml_line_iscii_visual( line , iscii_state) ;
	
	return  1 ;
}


/*
 * vertical view logical <=> visual methods
 */
 
static int
vert_delete(
	ml_logical_visual_t *  logvis
	)
{
	vert_logical_visual_t *  vert_logvis ;

	if( logvis->edit)
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
	ml_edit_t *  edit
	)
{
	vert_logical_visual_t *  vert_logvis ;

	vert_logvis = (vert_logical_visual_t*) logvis ;

	logvis->edit = edit ;
	
	vert_logvis->logical_model = logvis->edit->model ;
	ml_model_resize( &vert_logvis->visual_model ,
		logvis->edit->model.num_of_rows , logvis->edit->model.num_of_cols) ;
	
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
		return  ml_edit_get_cols( logvis->edit) ;
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
		return  ml_edit_get_rows( logvis->edit) ;
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
	ml_edit_t *  edit ;
	ml_line_t *  log_line ;
	ml_line_t *  vis_line ;
	int  row ;
	int  count ;

	if( logvis->is_visual)
	{
		return  0 ;
	}

	edit = logvis->edit ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [col %d index %d row %d]\n" ,
		edit->cursor.col , edit->cursor.char_index , edit->cursor.row) ;
#endif

	vert_logvis = (vert_logical_visual_t*) logvis ;

	ml_model_reset( &vert_logvis->visual_model) ;
	ml_model_reserve_boundary( &vert_logvis->visual_model , vert_logvis->visual_model.num_of_rows) ;
	
	if( vert_logvis->logical_model.num_of_rows != edit->model.num_of_rows ||
		vert_logvis->logical_model.num_of_cols != edit->model.num_of_cols)
	{
		/* ml_edit_t is resized */

		ml_model_resize( &vert_logvis->visual_model ,
			edit->model.num_of_rows , edit->model.num_of_cols) ;
	}
	
	vert_logvis->logical_model = edit->model ;
	edit->model = vert_logvis->visual_model ;

	if( mode & VERT_LTR)
	{
		/* Mongol */
		
		count = -1 ;
	}
	else
	{
		/* CJK */
		
		count = vert_logvis->logical_model.num_of_rows ;
	}

	while( 1)
	{
		if( mode & VERT_LTR)
		{
			/* Mongol */
			
			if( ++ count >= vert_logvis->logical_model.num_of_rows)
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

		log_line = ml_model_get_line( &vert_logvis->logical_model , count) ;

		for( row = 0 ; row < log_line->num_of_filled_chars ; row ++)
		{
			ml_char_t *  ch ;

			vis_line = ml_model_get_line( &edit->model , row) ;
			
			if( vis_line->num_of_filled_chars >= vis_line->num_of_chars)
			{
				continue ;
			}

			ch = &vis_line->chars[ vis_line->num_of_filled_chars ++] ;

			ml_char_copy( ch , &log_line->chars[row]) ;
			
			if( ml_line_is_modified( log_line) &&
				ml_line_get_beg_of_modified( log_line) <= row &&
				(ml_line_is_cleared_to_end( log_line) ||
				row < ml_line_get_beg_of_modified( log_line)
					+ ml_line_get_num_of_redrawn_chars( log_line)))
			{
				ml_line_set_modified( vis_line ,
					vis_line->num_of_filled_chars - 1 ,
					vis_line->num_of_filled_chars - 1 , 0) ;
			}
		}

		for( ; row < edit->model.num_of_rows ; row ++)
		{
			vis_line = ml_model_get_line( &edit->model , row) ;
			
			if( vis_line->num_of_filled_chars + 1 > vis_line->num_of_chars)
			{
				continue ;
			}
			
			ml_char_copy( &vis_line->chars[ vis_line->num_of_filled_chars ++] , ml_sp_ch()) ;
						
			if( ml_line_is_modified( log_line) &&
				ml_line_get_beg_of_modified( log_line) <= row &&
				(ml_line_is_cleared_to_end( log_line) ||
				row < ml_line_get_beg_of_modified( log_line)
					+ ml_line_get_num_of_redrawn_chars( log_line)))
			{
				ml_line_set_modified( vis_line ,
					vis_line->num_of_filled_chars - 1 ,
					vis_line->num_of_filled_chars - 1 , 0) ;
			}
		}

		ml_line_updated( log_line) ;
	}

	vert_logvis->cursor_logical_char_index = edit->cursor.char_index ;
	vert_logvis->cursor_logical_col = edit->cursor.col ;
	vert_logvis->cursor_logical_row = edit->cursor.row ;

	edit->cursor.row = vert_logvis->cursor_logical_char_index ;
	edit->cursor.char_index = edit->cursor.col = 0 ;

	if( mode & VERT_LTR)
	{
		/* Mongol */
		
		edit->cursor.col = edit->cursor.char_index = vert_logvis->cursor_logical_row ;
	}
	else
	{
		/* CJK */
		
		edit->cursor.col = edit->cursor.char_index =
			vert_logvis->logical_model.num_of_rows - vert_logvis->cursor_logical_row - 1 ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [col %d index %d row %d]\n" ,
		edit->cursor.col , edit->cursor.char_index , edit->cursor.row) ;
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
	ml_edit_t *  edit ;

	if( ! logvis->is_visual)
	{
		return  0 ;
	}
	
	edit = logvis->edit ;
	
	vert_logvis = (vert_logical_visual_t*) logvis ;
	
	edit->model = vert_logvis->logical_model ;

	edit->cursor.char_index = vert_logvis->cursor_logical_char_index ;
	edit->cursor.col = vert_logvis->cursor_logical_col ;
	edit->cursor.row = vert_logvis->cursor_logical_row ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [col %d index %d row %d]\n" ,
		edit->cursor.col , edit->cursor.char_index , edit->cursor.row) ;
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
	
	container->logvis.edit = NULL ;
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

ml_logical_visual_t *
ml_logvis_bidi_new(
	int  adhoc_right_align
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
	bidi_logvis->adhoc_right_align = adhoc_right_align ;

	bidi_logvis->logvis.edit = NULL ;
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

	comb_logvis->logvis.edit = NULL ;
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

ml_logical_visual_t *
ml_logvis_iscii_new(
	ml_iscii_state_t  iscii_state
	)
{
	iscii_logical_visual_t *  iscii_logvis ;

	if( ( iscii_logvis = malloc( sizeof( iscii_logical_visual_t))) == NULL)
	{
		return  NULL ;
	}

	iscii_logvis->iscii_state = iscii_state ;
	iscii_logvis->visual_lines = NULL ;
	iscii_logvis->logical_lines = NULL ;
	iscii_logvis->cursor_logical_char_index = 0 ;
	iscii_logvis->cursor_logical_col = 0 ;

	iscii_logvis->logvis.is_visual = 0 ;
	iscii_logvis->logvis.edit = NULL ;
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

	memset( &vert_logvis->logical_model , 0 , sizeof( vert_logvis->logical_model)) ;
	memset( &vert_logvis->visual_model , 0 , sizeof( vert_logvis->visual_model)) ;
	
	vert_logvis->cursor_logical_char_index = 0 ;
	vert_logvis->cursor_logical_col = 0 ;
	vert_logvis->cursor_logical_row = 0 ;

	vert_logvis->logvis.is_visual = 0 ;
	vert_logvis->logvis.edit = NULL ;

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
	if( strcmp( name , "cjk") == 0)
	{
		return  VERT_RTL ;
	}
	else if( strcmp( name , "mongol") == 0)
	{
		return  VERT_LTR ;
	}
	else /* if( strcmp( name , "none") == 0 */
	{
		return  0 ;
	}
}

char *
ml_get_vertical_mode_name(
	ml_vertical_mode_t  mode
	)
{
	if( mode == VERT_RTL)
	{
		return  "cjk" ;
	}
	else if( mode == VERT_LTR)
	{
		return  "mongol" ;
	}
	else
	{
		return  "none" ;
	}
}
