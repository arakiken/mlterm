/*
 *	$Id$
 */

#include  "ml_image_scroll.h"

#include  <stdio.h>		/* stderr */
#include  <kiklib/kik_util.h>

#include  "ml_image_intern.h"


/* --- static functions --- */

/*
 * src and dst may overlap
 *
 * the caller side must be responsible for whether {dst|src} + size is within
 * image->num_of_filled_lines.
 *
 * !! Notice !!
 * this function doesn't concern about crossing over the boundary and num_of_filled_rows.
 */
static int
copy_lines(
	ml_image_t *  image ,
	int  dst_row ,
	int  src_row ,
	u_int  size ,
	int  mark_changed
	)
{
	int  counter ;
	
	if( size == 0 || dst_row == src_row)
	{
		return  1 ;
	}

	if( src_row + size > image->num_of_rows)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " lines are copyed over image->num_of_rows") ;
	#endif

		size = image->num_of_rows - src_row ;

	#ifdef  DEBUG
		fprintf( stderr , " ... size modified -> %d.\n" , size) ;
	#endif
	}
	
	if( dst_row + size > image->num_of_rows)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " lines are copyed over image->num_of_rows") ;
	#endif

		size = image->num_of_rows - dst_row ;

	#ifdef  DEBUG
		fprintf( stderr,  " ... size modified -> %d.\n" , size) ;
	#endif
	}

	if( dst_row < src_row)
	{
		for( counter = 0 ; counter < size ; counter ++)
		{
			ml_imgline_copy_line( &IMAGE_LINE(image,dst_row + counter) ,
				&IMAGE_LINE(image,src_row + counter)) ;
			if( mark_changed)
			{
				ml_imgline_update_change_char_index( &IMAGE_LINE(image,dst_row + counter) ,
					0 , END_CHAR_INDEX(IMAGE_LINE(image,dst_row + counter)) , 1) ;
			}
		}
	}
	else
	{
		for( counter = size - 1 ; counter >= 0 ; counter --)
		{
			ml_imgline_copy_line( &IMAGE_LINE(image,dst_row + counter) ,
				&IMAGE_LINE(image,src_row + counter)) ;
			if( mark_changed)
			{
				ml_imgline_update_change_char_index( &IMAGE_LINE(image,dst_row + counter) ,
					0 , END_CHAR_INDEX(IMAGE_LINE(image,dst_row + counter)) , 1) ;
			}
		}
	}

	return  1 ;
}

static int
scroll_upward_region(
	ml_image_t *  image ,
	int  boundary_beg ,
	int  boundary_end ,
	u_int  size
	)
{
	int  counter ;

	if( END_ROW(image) < boundary_end)
	{
		boundary_end = END_ROW(image) ;
	}

	if( boundary_beg + size > boundary_end)
	{
		/*
		 * all lines within boundary are scrolled out.
		 */

		if( image->is_logging && image->scroll_listener->receive_upward_scrolled_out_line)
		{
			for( counter = boundary_beg ; counter < boundary_end ; counter ++)
			{
				(*image->scroll_listener->receive_upward_scrolled_out_line)(
					image->scroll_listener->self , &IMAGE_LINE(image,counter)) ;
			}
		}

		return  ml_image_clear_lines( image , boundary_beg , boundary_end - boundary_beg + 1) ;
	}
	
	/*
	 * handing over scrolled out lines , and calculating scrolling beg/end y positions.
	 */

	if( image->is_logging && image->scroll_listener->receive_upward_scrolled_out_line)
	{
		for( counter = boundary_beg ; counter < boundary_beg + size ; counter ++)
		{
			(*image->scroll_listener->receive_upward_scrolled_out_line)(
				image->scroll_listener->self , &IMAGE_LINE(image,counter)) ;
		}
	}

	/*
	 * resetting cursor position.
	 */
	if( boundary_beg <= image->cursor.row && image->cursor.row <= boundary_end)
	{
		if( image->cursor.row < boundary_beg + size)
		{
			image->cursor.row = boundary_beg ;
			image->cursor.char_index = 0 ;
			image->cursor.col = 0 ;
		}
		else
		{
			image->cursor.row -= size ;
		}
	}

	/*
	 * scrolling up in image.
	 */
	 
	if( boundary_beg > 0)
	{
		/*
		 * this operation doesn't actually modify anything on screen ,
		 * so mark_changed flag is set 0
		 */
		copy_lines( image , size , 0 , boundary_beg , 0) ;
	}

	if( image->beg_line + size >= image->num_of_rows)
	{
		image->beg_line = (image->beg_line + size) - image->num_of_rows ;
	}
	else
	{
		image->beg_line += size ;
	}

	if( boundary_end < END_ROW(image))
	{
		/*
		 * this operation doesn't actually modify anything on screen ,
		 * so mark_changed flag is set 0
		 */
		copy_lines( image , boundary_end + 1 , boundary_end + 1 - size ,
			END_ROW(image) - boundary_end , 0) ;
	}

	for( counter = boundary_end - size + 1 ; counter <= boundary_end ; counter ++)
	{
		ml_imgline_clear( &IMAGE_LINE(image,counter) , &image->sp_ch) ;
	}

	/*
	 * scrolling up in window.
	 */

	if( ! (*image->scroll_listener->window_scroll_upward_region)( image->scroll_listener->self ,
		boundary_beg , boundary_end , size))
	{
		int  counter ;

		for( counter = boundary_beg ; counter <= boundary_end ; counter ++)
		{
			ml_imgline_set_modified( &IMAGE_LINE(image,counter)) ;
		}
	}

	return  1 ;
}

static int
scroll_downward_region(
	ml_image_t *  image ,
	int  boundary_beg ,
	int  boundary_end ,
	u_int  size
	)
{
	int  counter ;
	
	if( boundary_beg + size > boundary_end)
	{
		/*
		 * all lines within boundary are scrolled out.
		 */

		return  ml_image_clear_lines( image , boundary_beg , boundary_end - boundary_beg + 1) ;
	}

	/*
	 * resetting cursor position.
	 */
	if( boundary_beg <= image->cursor.row && image->cursor.row <= boundary_end)
	{
		if( image->cursor.row + size >= boundary_end + 1)
		{
			image->cursor.row = boundary_end ;
			image->cursor.char_index = END_CHAR_INDEX(IMAGE_LINE(image,boundary_end)) ;
			image->cursor.col = ml_convert_char_index_to_col( image , boundary_end ,
				image->cursor.char_index , 0) ;
		}
		else
		{
			image->cursor.row += size ;
		}
	}

	/*
	 * scrolling down in image.
	 */
	 
	if( boundary_end < END_ROW(image))
	{
		/*
		 * this operation doesn't actually modify anything on screen ,
		 * so mark_changed flag is set 0
		 */
		copy_lines( image , boundary_end + 1 - size ,
			boundary_end + 1 , END_ROW(image) - boundary_end , 0) ;
	}
	else if( boundary_end > END_ROW(image))
	{
		image->num_of_filled_rows =
			K_MIN(image->num_of_filled_rows + size , boundary_end + 1) ;
	}

	if( image->beg_line < size)
	{
		image->beg_line = image->num_of_rows - (size - image->beg_line) ;
	}
	else
	{
		image->beg_line -= size ;
	}

	if( boundary_beg > 0)
	{
		/*
		 * this operation doesn't actually modify anything on screen ,
		 * so mark_changed flag is set 0
		 */
		copy_lines( image , 0 , size , boundary_beg , 0) ;
	}

	for( counter = boundary_beg ; counter < boundary_beg + size ; counter ++)
	{
		ml_imgline_clear( &IMAGE_LINE(image,counter) , &image->sp_ch) ;
	}
	
	/*
	 * scrolling down in window.
	 */

	if( ! (*image->scroll_listener->window_scroll_downward_region)( image->scroll_listener->self ,
		boundary_beg , boundary_end , size))
	{
		int  counter ;
		
		for( counter = boundary_beg ; counter <= boundary_end ; counter ++)
		{
			ml_imgline_set_modified( &IMAGE_LINE(image,counter)) ;
		}
	}

	return  1 ;
}


/* --- global functions --- */

int
ml_image_set_scroll_region(
	ml_image_t *  image ,
	int  beg ,
	int  end
	)
{
	image->scroll_region_beg = beg ;
	image->scroll_region_end = end ;

	return  1 ;
}

int
ml_image_scroll_upward(
	ml_image_t *  image ,
	u_int  size
	)
{
	return  scroll_upward_region( image , image->scroll_region_beg , image->scroll_region_end , size) ;
}

int
ml_image_scroll_downward(
	ml_image_t *  image ,
	u_int  size
	)
{
	return  scroll_downward_region( image , image->scroll_region_beg , image->scroll_region_end , size) ;
}

/*
 * XXX
 * this shoule be ml_image_insert_new_lines(ml_image_t * , u_int).
 */
int
ml_image_insert_new_line(
	ml_image_t *  image
	)
{
	u_int  copy_rows ;

	if( image->cursor.row >= image->scroll_region_beg && image->scroll_region_end < END_ROW(image))
	{
		copy_rows = image->scroll_region_end + 1 - image->cursor.row ;

		if( copy_rows + image->cursor.row + 1 > image->scroll_region_end + 1)
		{
			copy_rows -- ;
		}
		
		copy_lines( image , image->cursor.row + 1 , image->cursor.row , copy_rows , 1) ;
	}
	else
	{
		copy_rows = image->num_of_filled_rows - image->cursor.row ;
		
		if( copy_rows + image->cursor.row + 1 > image->num_of_rows)
		{
			copy_rows -- ;
		}
		
		copy_lines( image , image->cursor.row + 1 , image->cursor.row , copy_rows , 1) ;

		if( image->num_of_filled_rows < image->num_of_rows)
		{
			image->num_of_filled_rows ++ ;
		}
	}
	
	ml_image_clear_lines( image , image->cursor.row , 1) ;

	return  1 ;
}

/*
 * XXX
 * this should be ml_image_insert_delete_lines(ml_image_t * , u_int).
 */
int
ml_image_delete_line(
	ml_image_t *  image
	)
{
	if( image->cursor.row >= image->scroll_region_beg && image->scroll_region_end < END_ROW(image))
	{
		copy_lines( image , image->cursor.row , image->cursor.row + 1 ,
			image->scroll_region_end - image->cursor.row , 1) ;
			
		ml_image_clear_lines( image , image->scroll_region_end , 1) ;
	}
	else if( image->cursor.row < END_ROW(image))
	{
		copy_lines( image , image->cursor.row , image->cursor.row + 1 ,
			END_ROW(image) - image->cursor.row , 1) ;

		ml_image_clear_lines( image , END_ROW(image) , 1) ;
	}
	else /* if( image->cursor.row == END_ROW(image)) */
	{
		ml_image_clear_lines( image , END_ROW(image) , 1) ;
	}
	
	return  1 ;
}

inline int
ml_is_scroll_upperlimit(
	ml_image_t *  image ,
	int  row
	)
{
	return  (row == image->scroll_region_beg) ;
}

inline int
ml_is_scroll_lowerlimit(
	ml_image_t *  image ,
	int  row
	)
{
	return  (row == image->scroll_region_end) ;
}
