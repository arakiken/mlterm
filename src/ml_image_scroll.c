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
				ml_imgline_set_modified(
					&IMAGE_LINE(image,dst_row + counter) , 0 ,
					ml_imgline_end_char_index( &IMAGE_LINE(image,dst_row + counter)) ,
					1) ;
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
				ml_imgline_set_modified(
					&IMAGE_LINE(image,dst_row + counter) , 0 ,
					ml_imgline_end_char_index( &IMAGE_LINE(image,dst_row + counter)) ,
					1) ;
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
	int  old_num_of_filled_rows ;

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
	 
	old_num_of_filled_rows = image->num_of_filled_rows ;

	if( boundary_beg > 0)
	{
		/*
		 * this operation doesn't actually modify anything on screen ,
		 * so mark_changed flag is set 0
		 */
		copy_lines( image , size , 0 , boundary_beg , 0) ;
	}

	/* sliding offset */
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

		/* num_of_filled_rows value doesn't change */
	}
	else if( boundary_end == END_ROW(image))
	{
		image->num_of_filled_rows -= size ;
	}

	/*
	 * clearing
	 */

	for( counter = boundary_end - size + 1 ; counter <= boundary_end  ; counter ++)
	{
		ml_imgline_clear( &IMAGE_LINE(image,counter) , 0 , &image->sp_ch) ;
	}
	 
	for( counter = image->num_of_filled_rows ; counter < old_num_of_filled_rows ; counter ++)
	{
		ml_imgline_reset( &IMAGE_LINE(image,counter)) ;

		/*
		 * this is necessary because lines between old_num_of_filled_rows and
		 * num_of_filled_rows are cleared above but not cleared in window.
		 */
		ml_imgline_set_modified_all( &IMAGE_LINE(image,counter)) ;
	}

	for( ; counter < image->num_of_rows ; counter ++)
	{
		ml_imgline_reset( &IMAGE_LINE(image,counter)) ;
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
			ml_imgline_set_modified_all( &IMAGE_LINE(image,counter)) ;
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
	
	if( END_ROW(image) < boundary_end)
	{
		boundary_end = END_ROW(image) ;
	}
	
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
			image->cursor.char_index = 0 ;
			image->cursor.col = 0 ;
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

		/* num_of_filled_rows value doesn't change */
	}
	else if( boundary_end == END_ROW(image))
	{
		image->num_of_filled_rows = K_MIN(image->num_of_filled_rows + size,image->num_of_rows) ;
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

	/*
	 * clearing
	 */
	
	for( counter = boundary_beg ; counter < boundary_beg + size ; counter ++)
	{
		ml_imgline_clear( &IMAGE_LINE(image,counter) , 0 , &image->sp_ch) ;
	}

	for( counter = image->num_of_filled_rows ; counter < image->num_of_rows ; counter ++)
	{
		ml_imgline_reset( &IMAGE_LINE(image,counter)) ;
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
			ml_imgline_set_modified_all( &IMAGE_LINE(image,counter)) ;
		}
	}

	return  1 ;
}


/* --- global functions --- */

int
ml_imgscrl_scroll_upward(
	ml_image_t *  image ,
	u_int  size
	)
{
	return  scroll_upward_region( image , image->scroll_region_beg , image->scroll_region_end , size) ;
}

int
ml_imgscrl_scroll_downward(
	ml_image_t *  image ,
	u_int  size
	)
{
	return  scroll_downward_region( image , image->scroll_region_beg , image->scroll_region_end , size) ;
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

int
ml_imgscrl_insert_new_line(
	ml_image_t *  image
	)
{
	u_int  copy_rows ;
	int  start_row ;
	int  end_row ;

	if( image->cursor.row < image->scroll_region_beg)
	{
		start_row = image->scroll_region_beg ;
	}
	else
	{
		start_row = image->cursor.row ;
	}
	
	if( image->scroll_region_end < END_ROW(image))
	{
		end_row = image->scroll_region_end ;
	}
	else
	{
		end_row = END_ROW(image) ;
	}

	copy_rows = end_row - start_row + 1 ;

	if( copy_rows + start_row > end_row)
	{
		copy_rows -- ;
	}

	copy_lines( image , start_row + 1 , start_row , copy_rows , 1) ;
	ml_image_clear_lines( image , start_row , 1) ;

	return  1 ;
}

int
ml_imgscrl_delete_line(
	ml_image_t *  image
	)
{
	int  start_row ;
	int  end_row ;

	if( image->cursor.row < image->scroll_region_beg)
	{
		start_row = image->scroll_region_beg ;
	}
	else
	{
		start_row = image->cursor.row ;
	}

	if( image->scroll_region_end < END_ROW(image))
	{
		end_row = image->scroll_region_end ;
	}
	else
	{
		end_row = END_ROW(image) ;
	}
	
	copy_lines( image , start_row , start_row + 1 , end_row - start_row , 1) ;
	ml_image_clear_lines( image , end_row , 1) ;
	
	return  1 ;
}
