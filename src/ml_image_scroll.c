/*
 *	$Id$
 */

#include  "ml_image_scroll.h"

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
	ml_image_line_t *  src_line ;
	ml_image_line_t *  dst_line ;
	
	if( size == 0 || dst_row == src_row)
	{
		return  1 ;
	}

	if( src_row + size > image->model.num_of_rows)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" copying %d lines from %d row is over image->model.num_of_rows(%d)" ,
			size , src_row , image->model.num_of_rows) ;
	#endif

		size = image->model.num_of_rows - src_row ;

	#ifdef  DEBUG
		kik_msg_printf( " ... size modified -> %d.\n" , size) ;
	#endif
	}
	
	if( dst_row + size > image->model.num_of_rows)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" copying %d lines to %d row is over image->model.num_of_rows(%d)" ,
			size , dst_row , image->model.num_of_rows) ;
	#endif

		size = image->model.num_of_rows - dst_row ;

	#ifdef  DEBUG
		kik_msg_printf( " ... size modified -> %d.\n" , size) ;
	#endif
	}

	if( dst_row < src_row)
	{
		for( counter = 0 ; counter < size ; counter ++)
		{
			dst_line = ml_imgmdl_get_line( &image->model , dst_row + counter) ;
			src_line = ml_imgmdl_get_line( &image->model , src_row + counter) ;
			
			ml_imgline_copy_line( dst_line , src_line) ;
			if( mark_changed)
			{
				ml_imgline_set_modified( dst_line ,
					0 , ml_imgline_end_char_index( dst_line) , 1) ;
			}
		}
	}
	else
	{
		for( counter = size - 1 ; counter >= 0 ; counter --)
		{
			dst_line = ml_imgmdl_get_line( &image->model , dst_row + counter) ;
			src_line = ml_imgmdl_get_line( &image->model , src_row + counter) ;
			
			ml_imgline_copy_line( dst_line , src_line) ;
			if( mark_changed)
			{
				ml_imgline_set_modified( dst_line ,
					0 , ml_imgline_end_char_index( dst_line) , 1) ;
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
	int  window_is_scrolled ;

	if( ml_imgmdl_end_row( &image->model) < boundary_end)
	{
		boundary_end = ml_imgmdl_end_row( &image->model) ;
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
					image->scroll_listener->self ,
					ml_imgmdl_get_line( &image->model , counter)) ;
			}
		}

		return  ml_image_clear_lines( image , boundary_beg , boundary_end - boundary_beg + 1) ;
	}
	
	/*
	 * scrolling up in window.
	 *
	 * !! Notice !!
	 * This should be done before ml_image_t data structure is chanegd
	 * for the listener object to clear existing cache.
	 */
	window_is_scrolled = (*image->scroll_listener->window_scroll_upward_region)(
				image->scroll_listener->self , boundary_beg , boundary_end , size) ;
	 
	/*
	 * handing over scrolled out lines , and calculating scrolling beg/end y positions.
	 */

	if( image->is_logging && image->scroll_listener->receive_upward_scrolled_out_line)
	{
		for( counter = boundary_beg ; counter < boundary_beg + size ; counter ++)
		{
			(*image->scroll_listener->receive_upward_scrolled_out_line)(
				image->scroll_listener->self ,
				ml_imgmdl_get_line( &image->model , counter)) ;
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
	 
	if( boundary_beg == 0 && boundary_end == image->model.num_of_rows - 1)
	{
		ml_imgmdl_scroll_upward( &image->model , size) ;
	}
	else
	{
		copy_lines( image , boundary_beg , boundary_beg + size ,
			boundary_end - (boundary_beg + size) + 1 , 0) ;
	}
	
	ml_image_clear_lines( image , boundary_end - size + 1 , size) ;
	
	if( ! window_is_scrolled)
	{
		int  counter ;

		for( counter = boundary_beg ; counter <= boundary_end ; counter ++)
		{
			ml_imgline_set_modified_all( ml_imgmdl_get_line( &image->model , counter)) ;
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
	int  window_is_scrolled ;

	if( ml_imgmdl_end_row( &image->model) < boundary_end)
	{
		boundary_end = ml_imgmdl_end_row( &image->model) ;
	}
	
	if( boundary_beg + size > boundary_end)
	{
		/*
		 * all lines within boundary are scrolled out.
		 */

		return  ml_image_clear_lines( image , boundary_beg , boundary_end - boundary_beg + 1) ;
	}

	/*
	 * scrolling down in window.
	 *
	 * !! Notice !!
	 * This should be done before ml_image_t data structure is chanegd
	 * for the listener object to clear existing cache.
	 */
	window_is_scrolled = (*image->scroll_listener->window_scroll_downward_region)(
				image->scroll_listener->self , boundary_beg , boundary_end , size) ;
	
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
	if( boundary_beg == 0 && boundary_end == image->model.num_of_rows - 1)
	{
		ml_imgmdl_scroll_downward( &image->model , size) ;
	}
	else
	{
		copy_lines( image , boundary_beg + size , boundary_beg ,
			(boundary_end - size) - boundary_beg + 1 , 0) ;
	}
	
	ml_image_clear_lines( image , boundary_beg , size) ;
	
	if( ! window_is_scrolled)
	{	
		int  counter ;
		
		for( counter = boundary_beg ; counter <= boundary_end ; counter ++)
		{
			ml_imgline_set_modified_all( ml_imgmdl_get_line( &image->model , counter)) ;
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
#if  0
	/*
	 * XXX
	 * Can this cause unexpected result ?
	 */
	if( image->scroll_region_beg > image->cursor.row || image->cursor.row > image->scroll_region_end)
	{
		return  0 ;
	}
#endif

	return  scroll_upward_region( image , image->scroll_region_beg , image->scroll_region_end , size) ;
}

int
ml_imgscrl_scroll_downward(
	ml_image_t *  image ,
	u_int  size
	)
{
#if  0
	/*
	 * XXX
	 * Can this cause unexpected result ?
	 */
	if( image->scroll_region_beg > image->cursor.row || image->cursor.row > image->scroll_region_end)
	{
		return  0 ;
	}
#endif
	
	return  scroll_downward_region( image , image->scroll_region_beg , image->scroll_region_end , size) ;
}

/*
 * XXX
 * not used for now.
 */
#if  0
int
ml_imgscrl_scroll_upward_in_all(
	ml_image_t *  image ,
	u_int  size
	)
{
	return  scroll_upward_region( image , 0 , image->model.num_of_rows - 1 , size) ;
}

int
ml_imgscrl_scroll_downward_in_all(
	ml_image_t *  image ,
	u_int  size
	)
{
	return  scroll_downward_region( image , 0 , image->model.num_of_rows - 1 , size) ;
}
#endif

int
ml_is_scroll_upperlimit(
	ml_image_t *  image ,
	int  row
	)
{
	return  (row == image->scroll_region_beg) ;
}

int
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
	
	if( image->scroll_region_end < ml_imgmdl_end_row( &image->model))
	{
		end_row = image->scroll_region_end ;
	}
	else
	{
		end_row = ml_imgmdl_end_row( &image->model) ;
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

	if( image->scroll_region_end < ml_imgmdl_end_row( &image->model))
	{
		end_row = image->scroll_region_end ;
	}
	else
	{
		end_row = ml_imgmdl_end_row( &image->model) ;
	}
	
	copy_lines( image , start_row , start_row + 1 , end_row - start_row , 1) ;
	ml_image_clear_lines( image , end_row , 1) ;
	
	return  1 ;
}
