/*
 *	$Id$
 */

#include  "ml_logical_visual.h"

#include  <kiklib/kik_mem.h>		/* realloc/free */
#include  <kiklib/kik_util.h>		/* K_MIN */

#include  "ml_image_intern.h"


typedef struct  container_logical_visual
{
	ml_logical_visual_t  logvis ;

	/*
	 * visual : children[0] => children[1] => ... => children[n]
	 * logical: children[n] => ... => children[1] => children[0]
	 */
	ml_logical_visual_t *  children[2] ;
	u_int  num_of_children ;

} container_logical_visual_t ;

typedef struct  bidi_logical_visual
{
	ml_logical_visual_t  logvis ;
	
	int  cursor_logical_char_index ;
	int  cursor_logical_col ;	

} bidi_logical_visual_t ;

typedef struct  iscii_logical_visual
{
	ml_logical_visual_t  logvis ;
	
	ml_iscii_state_t  iscii_state ;

	ml_image_line_t *  logical_lines ;
	ml_image_line_t *  visual_lines ;
	
	u_int  logical_num_of_cols ;
	u_int  logical_num_of_rows ;

	int  cursor_logical_char_index ;
	int  cursor_logical_col ;

} iscii_logical_visual_t ;

typedef struct  vert_logical_visual
{
	ml_logical_visual_t  logvis ;
	
	ml_image_line_t *  logical_lines ;
	ml_image_line_t *  visual_lines ;

	int  logical_beg_row ;
	
	u_int  logical_num_of_cols ;
	u_int  logical_num_of_rows ;
	u_int  logical_num_of_filled_rows ;

	int  cursor_logical_char_index ;
	int  cursor_logical_col ;
	u_int  cursor_logical_row ;

} vert_logical_visual_t ;


/* --- static functions --- */

static int
container_delete(
	ml_logical_visual_t *  logvis
	)
{
	container_logical_visual_t *  container ;
	int  counter ;

	container = (container_logical_visual_t*) logvis ;

	if( container->num_of_children)
	{
		for( counter = container->num_of_children - 1 ; counter >= 0 ; counter --)
		{
			(*container->children[counter]->delete)( container->children[counter]) ;
		}
	}
	
	free( logvis) ;

	return  1 ;
}

static int
container_change_image(
	ml_logical_visual_t *  logvis ,
	ml_image_t *  image
	)
{
	container_logical_visual_t *  container ;
	int  counter ;

	logvis->image = image ;

	container = (container_logical_visual_t*) logvis ;

	for( counter = 0 ; counter < container->num_of_children ; counter ++)
	{
		(*container->children[counter]->change_image)( container->children[counter] , image) ;
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
		return  ml_image_get_cols( logvis->image) ;
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
		return  ml_image_get_rows( logvis->image) ;
	}
}

static int
container_render(
	ml_logical_visual_t *  logvis
	)
{
	container_logical_visual_t *  container ;
	int  counter ;

	container = (container_logical_visual_t*) logvis ;

	for( counter = 0 ; counter < container->num_of_children ; counter ++)
	{
		(*container->children[counter]->render)( container->children[counter]) ;
	}

	return  1 ;
}

static int
container_visual(
	ml_logical_visual_t *  logvis
	)
{
	container_logical_visual_t *  container ;
	int  counter ;

	if( logvis->is_visual)
	{
		return  0 ;
	}

	container = (container_logical_visual_t*) logvis ;

	for( counter = 0 ; counter < container->num_of_children ; counter ++)
	{
		(*container->children[counter]->visual)( container->children[counter]) ;
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
	int  counter ;

	if( ! logvis->is_visual)
	{
		return  0 ;
	}

	container = (container_logical_visual_t*) logvis ;

	if( container->num_of_children == 0)
	{
		return  1 ;
	}

	for( counter = container->num_of_children - 1 ; counter >= 0  ; counter --)
	{
		(*container->children[counter]->logical)( container->children[counter]) ;
	}

	logvis->is_visual = 0 ;

	return  1 ;
}

static int
container_visual_line(
	ml_logical_visual_t *  logvis ,
	ml_image_line_t *  line
	)
{
	container_logical_visual_t *  container ;
	int  counter ;

	container = (container_logical_visual_t*) logvis ;

	for( counter = 0 ; counter < container->num_of_children ; counter ++)
	{
		(*container->children[counter]->visual_line)( container->children[counter] , line) ;
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
	ml_image_t *  image ;
	int  row ;

	image = logvis->image ;
	
	for( row = 0 ; row < image->num_of_rows ; row ++)
	{
		ml_imgline_unuse_bidi( &image->lines[row]) ;
	}

	free( logvis) ;
	
	return  1 ;
}

static int
bidi_change_image(
	ml_logical_visual_t *  logvis ,
	ml_image_t *  new_image
	)
{
	ml_image_t *  image ;
	int  row ;

	image = logvis->image ;
	
	for( row = 0 ; row < image->num_of_rows ; row ++)
	{
		ml_imgline_unuse_bidi( &image->lines[row]) ;
	}

	logvis->image = new_image ;
	
	return  1 ;
}

static u_int
bidi_logical_cols(
	ml_logical_visual_t *  logvis
	)
{
	return  ml_image_get_cols( logvis->image) ;
}

static u_int
bidi_logical_rows(
	ml_logical_visual_t *  logvis
	)
{
	return  ml_image_get_rows( logvis->image) ;
}

static int
bidi_render(
	ml_logical_visual_t *  logvis
	)
{
	ml_image_t *  image ;
	ml_image_line_t *  line ;
	int  row ;

	image = logvis->image ;

	/*
	 * all lines(not only filled lines) should be rendered.
	 */
	for( row = 0 ; row < image->num_of_rows ; row ++)
	{
		line = &IMAGE_LINE(image,row) ;
		
		if( ml_imgline_is_modified( line))
		{
			if( ! ml_imgline_is_using_bidi( line))
			{
				ml_imgline_use_bidi( line) ;
			}

			ml_imgline_bidi_render( line) ;
		}
	}

	return  1 ;
}

static int
bidi_visual(
	ml_logical_visual_t *  logvis
	)
{
	ml_image_t *  image ;
	int  row ;
	u_int  cols_rest ;

	if( logvis->is_visual)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " is called continuously.\n") ;
	#endif
		
		return  0 ;
	}
	
	image = logvis->image ;

#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	ml_convert_col_to_char_index( &CURSOR_LINE(image) , &cols_rest , image->cursor.col , 0) ;

	for( row = 0 ; row < image->num_of_filled_rows ; row ++)
	{
		if( ! ml_imgline_bidi_visual( &IMAGE_LINE(image,row)))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " visualize row %d failed.\n" , row) ;
		#endif
		}
	}

	((bidi_logical_visual_t*)logvis)->cursor_logical_char_index = image->cursor.char_index ;
	((bidi_logical_visual_t*)logvis)->cursor_logical_col = image->cursor.col ;

	image->cursor.char_index = ml_bidi_convert_logical_char_index_to_visual(
					&CURSOR_LINE(image) , image->cursor.char_index) ;
	image->cursor.col = ml_convert_char_index_to_col( &CURSOR_LINE(image) ,
					image->cursor.char_index , 0) + cols_rest ;

#ifdef  CURSOR_DEBUG
	fprintf( stderr , "-> [cursor(index)%d (col)%d (row)%d]\n" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	logvis->is_visual = 1 ;

	return  1 ;
}

static int
bidi_logical(
	ml_logical_visual_t *  logvis
	)
{
	ml_image_t *  image ;
	int  row ;

	if( ! logvis->is_visual)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " is called continuously.\n") ;
	#endif
			
		return  0 ;
	}
	
	image = logvis->image ;
	
#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	for( row = 0 ; row < image->num_of_filled_rows ; row ++)
	{
		if( ! ml_imgline_bidi_logical( &IMAGE_LINE(image,row)))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " visualize row %d failed.\n" , row) ;
		#endif
		}
	}

	image->cursor.char_index = ((bidi_logical_visual_t*)logvis)->cursor_logical_char_index ;
	image->cursor.col = ((bidi_logical_visual_t*)logvis)->cursor_logical_col ;

#ifdef  CURSOR_DEBUG
	fprintf( stderr , "-> [cursor(index)%d (col)%d (row)%d]\n" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif
	
	logvis->is_visual = 0 ;
	
	return  1 ;
}

static int
bidi_visual_line(
	ml_logical_visual_t *  logvis ,
	ml_image_line_t *  line
	)
{
#if  0
	/* just to be sure */
	ml_imgline_bidi_render( line) ;
#endif

	ml_imgline_bidi_visual( line) ;

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
		ml_imgline_final( &iscii_logvis->visual_lines[row]) ;
		ml_imgline_final( &iscii_logvis->logical_lines[row]) ;
	}

	free( iscii_logvis->visual_lines) ;
	free( iscii_logvis->logical_lines) ;
	
	free( logvis) ;
	
	return  1 ;
}

static int
iscii_change_image(
	ml_logical_visual_t *  logvis ,
	ml_image_t *  image
	)
{
	logvis->image = image ;

	return  1 ;
}

static u_int
iscii_logical_cols(
	ml_logical_visual_t *  logvis
	)
{
	return  ml_image_get_cols( logvis->image) ;
}

static u_int
iscii_logical_rows(
	ml_logical_visual_t *  logvis
	)
{
	return  ml_image_get_rows( logvis->image) ;
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
	ml_image_line_t *  lines ,
	u_int  num_of_lines ,
	ml_image_line_t *  line
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
	ml_image_t *  image ;
	int  row ;
	int  cols_rest ;

	if( logvis->is_visual)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " is called continuously.\n") ;
	#endif
	
		return  0 ;
	}
	
	image = logvis->image ;

	iscii_logvis = (iscii_logical_visual_t*) logvis ;

	if( iscii_logvis->logical_num_of_rows != image->num_of_rows ||
		iscii_logvis->logical_num_of_cols != image->num_of_cols)
	{
		/* ml_image_t is resized */
		
		if( iscii_logvis->logical_num_of_rows != image->num_of_rows)
		{
			void *  p1 ;
			void *  p2 ;
			
			if( iscii_logvis->visual_lines)
			{
				for( row = 0 ; row < iscii_logvis->logical_num_of_rows ; row ++)
				{
					ml_imgline_final( &iscii_logvis->visual_lines[row]) ;
				}
			}

			if( iscii_logvis->logical_lines)
			{
				for( row = 0 ; row < iscii_logvis->logical_num_of_rows ; row ++)
				{
					ml_imgline_final( &iscii_logvis->logical_lines[row]) ;
				}
			}
			
			if( ( p1 = realloc( iscii_logvis->visual_lines ,
					sizeof( ml_image_line_t) * image->num_of_rows)) == NULL ||
				( p2 = realloc( iscii_logvis->logical_lines ,
					sizeof( ml_image_line_t) * image->num_of_rows)) == NULL)
			{
				free( iscii_logvis->visual_lines) ;
				iscii_logvis->visual_lines = NULL ;
				
				free( iscii_logvis->logical_lines) ;
				iscii_logvis->logical_lines = NULL ;
				
				return  0 ;
			}

			iscii_logvis->visual_lines = p1 ;
			iscii_logvis->logical_lines = p2 ;
			
			iscii_logvis->logical_num_of_rows = image->num_of_rows ;
		}
		
		for( row = 0 ; row < iscii_logvis->logical_num_of_rows ; row ++)
		{
			ml_imgline_init( &iscii_logvis->visual_lines[row] , image->num_of_cols) ;
			ml_imgline_init( &iscii_logvis->logical_lines[row] , image->num_of_cols) ;
		}

		iscii_logvis->logical_num_of_cols = image->num_of_cols ;
	}
	
	ml_convert_col_to_char_index( &CURSOR_LINE(image) , &cols_rest , image->cursor.col , 0) ;
	
	for( row = 0 ; row < image->num_of_filled_rows ; row ++)
	{
		int  is_cache_active ;
		
		if( ml_imgline_is_modified( &IMAGE_LINE(image,row)))
		{
			is_cache_active = 0 ;
		}
		else if( iscii_logvis->logical_lines[row].num_of_filled_chars !=
				IMAGE_LINE(image,row).num_of_filled_chars ||
			! ml_str_bytes_equal( iscii_logvis->logical_lines[row].chars ,
				IMAGE_LINE(image,row).chars ,
				iscii_logvis->logical_lines[row].num_of_filled_chars) )
		{
			int  hit_row ;
			
			if( ( hit_row = search_same_line( iscii_logvis->logical_lines ,
				iscii_logvis->logical_num_of_rows , &IMAGE_LINE(image,row))) == -1)
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
				ml_imgline_copy_line( &iscii_logvis->logical_lines[row] ,
					&iscii_logvis->logical_lines[hit_row]) ;
				ml_imgline_copy_line( &iscii_logvis->visual_lines[row] ,
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
			ml_imgline_copy_line( &IMAGE_LINE(image,row) , &iscii_logvis->visual_lines[row]) ;
		}
		else
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " iscii rendering.\n") ;
		#endif
		
			ml_imgline_copy_line( &iscii_logvis->logical_lines[row] , &IMAGE_LINE(image,row)) ;
			ml_imgline_is_updated( &iscii_logvis->logical_lines[row]) ;

			ml_imgline_iscii_visual( &IMAGE_LINE(image,row) , iscii_logvis->iscii_state) ;

			/* caching */
			ml_imgline_copy_line( &iscii_logvis->visual_lines[row] , &IMAGE_LINE(image,row)) ;
			ml_imgline_is_updated( &iscii_logvis->visual_lines[row]) ;
		}
	}

	iscii_logvis->cursor_logical_char_index = image->cursor.char_index ;
	iscii_logvis->cursor_logical_col = image->cursor.col ;
	
	image->cursor.char_index = ml_iscii_convert_logical_char_index_to_visual(
					&CURSOR_LINE(image) , image->cursor.char_index) ;
	image->cursor.col = ml_convert_char_index_to_col( &CURSOR_LINE(image) ,
				image->cursor.char_index , 0) + cols_rest ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [col %d index %d]\n" ,
		image->cursor.col , image->cursor.char_index) ;
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
	ml_image_t *  image ;
	int  row ;

	if( ! logvis->is_visual)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " is called continuously.\n") ;
	#endif
			
		return  0 ;
	}
	
	image = logvis->image ;

	iscii_logvis = (iscii_logical_visual_t*) logvis ;
	
	for( row = 0 ; row < image->num_of_filled_rows ; row ++)
	{
		ml_imgline_copy_line( &IMAGE_LINE(image,row) , &iscii_logvis->logical_lines[row]) ;
	}

	image->cursor.char_index = iscii_logvis->cursor_logical_char_index ;
	image->cursor.col = iscii_logvis->cursor_logical_col ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [col %d index %d]\n" ,
		image->cursor.col , image->cursor.char_index) ;
#endif

	logvis->is_visual = 0 ;
	
	return  1 ;
}

static int
iscii_visual_line(
	ml_logical_visual_t *  logvis ,
	ml_image_line_t *  line
	)
{
	ml_iscii_state_t  iscii_state ;

	iscii_state = ((iscii_logical_visual_t*) logvis)->iscii_state ;

	ml_imgline_iscii_visual( line , iscii_state) ;
	
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
	int  row ;

	vert_logvis = (vert_logical_visual_t*) logvis ;

	if( logvis->is_visual)
	{
		(*logvis->logical)( logvis) ;
	}
	
	for( row = 0 ; row < vert_logvis->logical_num_of_cols ; row ++)
	{
		ml_imgline_final( &vert_logvis->visual_lines[row]) ;
	}

	free( vert_logvis->visual_lines) ;
		
	free( logvis) ;
	
	return  1 ;
}

static int
vert_change_image(
	ml_logical_visual_t *  logvis ,
	ml_image_t *  image
	)
{
	logvis->image = image ;

	return  1 ;
}

static u_int
vert_logical_cols(
	ml_logical_visual_t *  logvis
	)
{
	if( logvis->is_visual)
	{
		return  ((vert_logical_visual_t*)logvis)->logical_num_of_cols ;
	}
	else
	{
		return  ml_image_get_cols( logvis->image) ;
	}
}

static u_int
vert_logical_rows(
	ml_logical_visual_t *  logvis
	)
{
	if( logvis->is_visual)
	{
		return  ((vert_logical_visual_t*)logvis)->logical_num_of_rows ;
	}
	else
	{
		return  ml_image_get_rows( logvis->image) ;
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
	ml_image_t *  image ;
	ml_image_line_t *  line ;
	int  row ;
	int  counter ;

	if( logvis->is_visual)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " is called continuously.\n") ;
	#endif
	
		return  0 ;
	}

	image = logvis->image ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [col %d index %d row %d]\n" ,
		image->cursor.col , image->cursor.char_index , image->cursor.row) ;
#endif

	vert_logvis = (vert_logical_visual_t*) logvis ;

	if( vert_logvis->logical_num_of_rows != image->num_of_rows ||
		vert_logvis->logical_num_of_cols != image->num_of_cols)
	{
		/* ml_image_t is resized */
		
		if( vert_logvis->logical_num_of_cols != image->num_of_cols)
		{
			void *  p ;
			
			if( vert_logvis->visual_lines)
			{
				for( row = 0 ; row < vert_logvis->logical_num_of_cols ; row ++)
				{
					ml_imgline_final( &vert_logvis->visual_lines[row]) ;
				}
			}

			if( ( p = realloc( vert_logvis->visual_lines ,
					sizeof( ml_image_line_t) * image->num_of_cols)) == NULL)
			{
				free( vert_logvis->visual_lines) ;
				vert_logvis->visual_lines = NULL ;
				
				return  0 ;
			}

			vert_logvis->visual_lines = p ;

			vert_logvis->logical_num_of_cols = image->num_of_cols ;
		}
		
		for( row = 0 ; row < vert_logvis->logical_num_of_cols ; row ++)
		{
			ml_imgline_init( &vert_logvis->visual_lines[row] , image->num_of_rows) ;
		}

		vert_logvis->logical_num_of_rows = image->num_of_rows ;
	}
	else
	{
		for( row = 0 ; row < vert_logvis->logical_num_of_cols ; row ++)
		{
			ml_imgline_reset( &vert_logvis->visual_lines[row]) ;
			ml_imgline_is_updated( &vert_logvis->visual_lines[row]) ;
		}
	}

	vert_logvis->logical_num_of_filled_rows = image->num_of_filled_rows ;

	vert_logvis->logical_beg_row = image->beg_row ;
	image->beg_row = 0 ;

	image->num_of_rows = vert_logvis->logical_num_of_cols ;
	image->num_of_filled_rows = vert_logvis->logical_num_of_cols ;
	image->num_of_cols = vert_logvis->logical_num_of_rows ;
	
	vert_logvis->logical_lines = image->lines ;
	image->lines = vert_logvis->visual_lines ;

	if( mode & VERT_LTR)
	{
		/* Mongol */
		
		counter = -1 ;
	}
	else
	{
		/* CJK */
		
		counter = vert_logvis->logical_num_of_rows ;
	}

	while( 1)
	{
		if( mode & VERT_LTR)
		{
			/* Mongol */
			
			if( ++ counter >= vert_logvis->logical_num_of_rows)
			{
				break ;
			}
		}
		else
		{
			/* CJK */
			
			if( -- counter < 0)
			{
				break ;
			}
		}
		
		/*
		 * the same processing as IMAGE_LINE macro.
		 */
		if( counter + vert_logvis->logical_beg_row >= vert_logvis->logical_num_of_rows)
		{
			line = &vert_logvis->logical_lines[
				vert_logvis->logical_beg_row + counter - vert_logvis->logical_num_of_rows] ;
		}
		else
		{
			line = &vert_logvis->logical_lines[ vert_logvis->logical_beg_row + counter] ;
		}

		for( row = 0 ; row < line->num_of_filled_chars ; row ++)
		{
			ml_char_t *  ch ;
			
			if( IMAGE_LINE(image,row).num_of_filled_chars + 1 >
				IMAGE_LINE(image,row).num_of_chars)
			{
				continue ;
			}

			ch = &IMAGE_LINE(image,row).chars[ IMAGE_LINE(image,row).num_of_filled_chars ++] ;

			ml_char_copy( ch , &line->chars[row]) ;
			
			if( ml_char_font_decor( ch) == FONT_UNDERLINE)
			{
				ml_char_set_font_decor( ch , FONT_LEFTLINE) ;
			}
			
			if( ml_imgline_is_modified( line) &&
				ml_imgline_get_beg_of_modified( line) <= row &&
				(ml_imgline_is_cleared_to_end( line) ||
				row < ml_imgline_get_beg_of_modified( line)
					+ ml_imgline_get_num_of_redrawn_chars( line)))
			{
				ml_imgline_set_modified( &IMAGE_LINE(image,row) ,
					IMAGE_LINE(image,row).num_of_filled_chars - 1 ,
					IMAGE_LINE(image,row).num_of_filled_chars - 1 , 0) ;
			}
		}

		for( ; row < image->num_of_rows ; row ++)
		{
			if( IMAGE_LINE(image,row).num_of_filled_chars + 1 >
				IMAGE_LINE(image,row).num_of_chars)
			{
				continue ;
			}
			
			ml_char_copy( &IMAGE_LINE(image,row).chars[
				IMAGE_LINE(image,row).num_of_filled_chars ++] , &image->sp_ch) ;
				
			if( ml_imgline_is_modified( line) &&
				ml_imgline_get_beg_of_modified( line) <= row &&
				(ml_imgline_is_cleared_to_end( line) ||
				row < ml_imgline_get_beg_of_modified( line)
					+ ml_imgline_get_num_of_redrawn_chars( line)))
			{
				ml_imgline_set_modified( &IMAGE_LINE(image,row) ,
					IMAGE_LINE(image,row).num_of_filled_chars - 1 ,
					IMAGE_LINE(image,row).num_of_filled_chars - 1 , 0) ;
			}
		}
		
		ml_imgline_is_updated( line) ;
	}

	vert_logvis->cursor_logical_char_index = image->cursor.char_index ;
	vert_logvis->cursor_logical_col = image->cursor.col ;
	vert_logvis->cursor_logical_row = image->cursor.row ;

	image->cursor.row = vert_logvis->cursor_logical_char_index ;
	image->cursor.char_index = image->cursor.col = 0 ;

	if( mode & VERT_LTR)
	{
		/* Mongol */
		
		image->cursor.col = image->cursor.char_index = vert_logvis->cursor_logical_row ;
	}
	else
	{
		/* CJK */
		
		image->cursor.col = image->cursor.char_index =
			vert_logvis->logical_num_of_rows - vert_logvis->cursor_logical_row - 1 ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [col %d index %d row %d]\n" ,
		image->cursor.col , image->cursor.char_index , image->cursor.row) ;
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
	ml_image_t *  image ;

	if( ! logvis->is_visual)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " is called continuously.\n") ;
	#endif
	
		return  0 ;
	}
	
	image = logvis->image ;
	
	vert_logvis = (vert_logical_visual_t*) logvis ;
	
	image->lines = vert_logvis->logical_lines ;

	image->beg_row = vert_logvis->logical_beg_row ;
		
	image->num_of_rows = vert_logvis->logical_num_of_rows ;
	image->num_of_filled_rows = vert_logvis->logical_num_of_filled_rows ;
	image->num_of_cols = vert_logvis->logical_num_of_cols ;

	image->cursor.char_index = vert_logvis->cursor_logical_char_index ;
	image->cursor.col = vert_logvis->cursor_logical_col ;
	image->cursor.row = vert_logvis->cursor_logical_row ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [col %d index %d row %d]\n" ,
		image->cursor.col , image->cursor.char_index , image->cursor.row) ;
#endif

	logvis->is_visual = 0 ;
	
	return  1 ;
}

static int
vert_visual_line(
	ml_logical_visual_t *  logvis ,
	ml_image_line_t *  line
	)
{
	return  1 ;
}


/* --- global functions --- */

ml_logical_visual_t *
ml_logvis_container_new(
	ml_image_t *  image
	)
{
	container_logical_visual_t *  container ;

	if( ( container = malloc( sizeof( container_logical_visual_t))) == NULL)
	{
		return  NULL ;
	}
	
	container->num_of_children = 0 ;
	
	container->logvis.image = image ;

	container->logvis.is_visual = 0 ;

	container->logvis.delete = container_delete ;
	container->logvis.change_image = container_change_image ;
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
	container_logical_visual_t *  container ;

	container = (container_logical_visual_t*) logvis ;

	if( container->num_of_children == 2)
	{
		return  0 ;
	}

	container->children[container->num_of_children ++] = child ;

	return  1 ;
}

ml_logical_visual_t *
ml_logvis_bidi_new(
	ml_image_t *  image
	)
{
	bidi_logical_visual_t *  bidi_logvis ;

	if( ( bidi_logvis = malloc( sizeof( bidi_logical_visual_t))) == NULL)
	{
		return  NULL ;
	}
	
	bidi_logvis->cursor_logical_char_index = 0 ;
	bidi_logvis->cursor_logical_col = 0 ;

	bidi_logvis->logvis.image = image ;
	bidi_logvis->logvis.is_visual = 0 ;
	
	bidi_logvis->logvis.delete = bidi_delete ;
	bidi_logvis->logvis.change_image = bidi_change_image ;
	bidi_logvis->logvis.logical_cols = bidi_logical_cols ;
	bidi_logvis->logvis.logical_rows = bidi_logical_rows ;
	bidi_logvis->logvis.render = bidi_render ;
	bidi_logvis->logvis.visual = bidi_visual ;
	bidi_logvis->logvis.logical = bidi_logical ;
	bidi_logvis->logvis.visual_line = bidi_visual_line ;

	return  (ml_logical_visual_t*) bidi_logvis ;
}

ml_logical_visual_t *
ml_logvis_iscii_new(
	ml_image_t *  image ,
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
	iscii_logvis->logvis.image = image ;
	iscii_logvis->logical_num_of_cols = 0 ;
	iscii_logvis->logical_num_of_rows = 0 ;
	
	iscii_logvis->logvis.delete = iscii_delete ;
	iscii_logvis->logvis.change_image = iscii_change_image ;
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
	ml_image_t *  image ,
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

	vert_logvis->visual_lines = NULL ;
	vert_logvis->logical_lines = NULL ;
	vert_logvis->logical_num_of_cols = 0 ;
	vert_logvis->logical_num_of_rows = 0 ;
	vert_logvis->logical_num_of_filled_rows = 0 ;
	vert_logvis->cursor_logical_char_index = 0 ;
	vert_logvis->cursor_logical_col = 0 ;
	vert_logvis->cursor_logical_row = 0 ;

	vert_logvis->logvis.is_visual = 0 ;
	vert_logvis->logvis.image = image ;

	vert_logvis->logvis.delete = vert_delete ;
	vert_logvis->logvis.change_image = vert_change_image ;
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
