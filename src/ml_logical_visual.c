/*
 *	$Id$
 */

#include  "ml_logical_visual.h"

#include  "ml_image_intern.h"


typedef struct  iscii_logical_visual
{
	ml_logical_visual_t  logvis ;
	ml_iscii_state_t  iscii_state ;

} iscii_logical_visual_t ;


/* --- static functions --- */

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
		
		if( line->is_modified)
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

	image->cursor.char_index = ml_bidi_convert_logical_char_index_to_visual(
					&CURSOR_LINE(image) , image->cursor.char_index) ;
	image->cursor.col = ml_convert_char_index_to_col( &CURSOR_LINE(image) ,
					image->cursor.char_index , 0) + cols_rest ;

#ifdef  CURSOR_DEBUG
	fprintf( stderr , "-> [cursor(index)%d (col)%d (row)%d]\n" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	return  1 ;
}

static int
bidi_logical(
	ml_logical_visual_t *  logvis
	)
{
	ml_image_t *  image ;
	int  row ;
	u_int  cols_rest ;

	image = logvis->image ;
	
#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	ml_convert_col_to_char_index( &CURSOR_LINE(image) , &cols_rest , image->cursor.col , 0) ;
	
	for( row = 0 ; row < image->num_of_filled_rows ; row ++)
	{
		if( ! ml_imgline_bidi_logical( &IMAGE_LINE(image,row)))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " visualize row %d failed.\n" , row) ;
		#endif
		}
	}

	image->cursor.char_index = ml_bidi_convert_visual_char_index_to_logical(
					&CURSOR_LINE(image) , image->cursor.char_index) ;
	image->cursor.col = ml_convert_char_index_to_col( &CURSOR_LINE(image) ,
					image->cursor.char_index , 0) + cols_rest ;

#ifdef  CURSOR_DEBUG
	fprintf( stderr , "-> [cursor(index)%d (col)%d (row)%d]\n" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif
	
	return  1 ;
}

static int
bidi_visual_line(
	ml_logical_visual_t *  logvis ,
	ml_image_line_t *  line
	)
{
	/* just to be sure */
	ml_imgline_bidi_render( line) ;
	
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

static int
iscii_render(
	ml_logical_visual_t *  logvis
	)
{
	return  1 ;
}

static int
iscii_visual(
	ml_logical_visual_t *  logvis
	)
{
	ml_image_t *  image ;
	ml_iscii_state_t  iscii_state ;
	int  row ;
	int  cols_rest ;

	image = logvis->image ;
	iscii_state = ((iscii_logical_visual_t*) logvis)->iscii_state ;

	ml_convert_col_to_char_index( &CURSOR_LINE(image) , &cols_rest , image->cursor.col , 0) ;
	
	for( row = 0 ; row < image->num_of_filled_rows ; row ++)
	{
		ml_imgline_iscii_visual( &IMAGE_LINE(image,row) , iscii_state) ;
	}

	image->cursor.char_index = ml_iscii_convert_logical_char_index_to_visual(
					&CURSOR_LINE(image) , image->cursor.char_index) ;

	image->cursor.col = ml_convert_char_index_to_col( &CURSOR_LINE(image) ,
				image->cursor.char_index , 0) + cols_rest ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [col %d index %d]\n" ,
		image->cursor.col , image->cursor.char_index) ;
#endif

	return  1 ;
}

static int
iscii_logical(
	ml_logical_visual_t *  logvis
	)
{
	ml_image_t *  image ;
	int  row ;
	int  cols_rest ;

	image = logvis->image ;
	
	ml_convert_col_to_char_index( &CURSOR_LINE(image) , &cols_rest , image->cursor.col , 0) ;
	
	image->cursor.char_index = ml_iscii_convert_visual_char_index_to_logical(
					&CURSOR_LINE(image) , image->cursor.char_index) ;
	
	for( row = 0 ; row < image->num_of_filled_rows ; row ++)
	{
		ml_imgline_iscii_logical( &IMAGE_LINE(image,row)) ;
	}

	image->cursor.col = ml_convert_char_index_to_col( &CURSOR_LINE(image) ,
				image->cursor.char_index , 0) + cols_rest ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [col %d index %d]\n" ,
		image->cursor.col , image->cursor.char_index) ;
#endif

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


/* --- global functions --- */

ml_logical_visual_t *
ml_logvis_bidi_new(
	ml_image_t *  image
	)
{
	ml_logical_visual_t *  logvis ;

	if( ( logvis = malloc( sizeof( ml_image_t))) == NULL)
	{
		return  NULL ;
	}
	
	logvis->image = image ;

	logvis->delete = bidi_delete ;
	logvis->change_image = bidi_change_image ;
	logvis->render = bidi_render ;
	logvis->visual = bidi_visual ;
	logvis->logical = bidi_logical ;
	logvis->visual_line = bidi_visual_line ;

	return  logvis ;
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
	
	iscii_logvis->logvis.image = image ;
	
	iscii_logvis->logvis.delete = iscii_delete ;
	iscii_logvis->logvis.change_image = iscii_change_image ;
	iscii_logvis->logvis.render = iscii_render ;
	iscii_logvis->logvis.visual = iscii_visual ;
	iscii_logvis->logvis.logical = iscii_logical ;
	iscii_logvis->logvis.visual_line = iscii_visual_line ;

	return  (ml_logical_visual_t*) iscii_logvis ;
}
