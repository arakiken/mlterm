/*
 *	$Id$
 */

#include  "ml_image_intern.h"

#include  <stdio.h>
#include  <kiklib/kik_debug.h>


/* --- global functions --- */

int
ml_image_clear_line(
	ml_image_t *  image ,
	int  row ,
	int  char_index
	)
{
	if( row > END_ROW(image))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " row %d is over END_ROW %d. nothing is cleared.\n" ,
			row , END_ROW(image)) ;
	#endif

		return  1 ;
	}

	ml_imgline_clear( &IMAGE_LINE(image,row) , char_index , &image->sp_ch) ;
	
	if( row == image->cursor.row)
	{
		if( image->cursor.char_index > char_index)
		{
			image->cursor.char_index = char_index ;
			image->cursor.col = ml_convert_char_index_to_col( &IMAGE_LINE(image,row) ,
						char_index , 0) ;
		}
	}

	return  1 ;
}

/*
 * used in ml_image/ml_image_scroll
 */
int
ml_image_clear_lines(
	ml_image_t *  image ,
	int  beg_row ,
	u_int  size
	)
{
	int  counter ;

	if( size == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " size %d should be larger than 0.\n" , size) ;
	#endif
	
		return  0 ;
	}

	if( beg_row > END_ROW(image))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " line %d is already empty.\n" , beg_row) ;
	#endif
		
		return  0 ;
	}

	ml_image_clear_line( image , beg_row , 0) ;

	if( beg_row + size < image->num_of_filled_rows)
	{
		/*
		 * there will still be some lines after the cleared lines.
		 */
		 
		for( counter = 1 ; counter < size ; counter ++)
		{
			ml_image_clear_line( image , beg_row + counter , 0) ;
		}

		if( beg_row <= image->cursor.row && image->cursor.row <= beg_row + size - 1)
		{
			image->cursor.char_index = 0 ;
			image->cursor.col = 0 ;
		}
	}
	else
	{
		/*
		 * all lines after beg_row will be cleared.
		 */
		 
		for( counter = 1 ; counter < size ; counter ++)
		{
			ml_imgline_reset( &IMAGE_LINE(image,beg_row + counter)) ;
		}

		image->num_of_filled_rows = beg_row + 1 ;
	
		if( image->cursor.row >= beg_row)
		{
			image->cursor.row = beg_row ;
			image->cursor.char_index = 0 ;
			image->cursor.col = 0 ;
		}
	}

	return  1 ;
}
