/*
 *	update: <2001/11/21(13:26:56)>
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

	if( char_index > END_CHAR_INDEX(IMAGE_LINE(image,row)))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" col %d is overflowed(char array of line %d is %d) , and nothing is cleared.\n" ,
			char_index , row , END_CHAR_INDEX(IMAGE_LINE(image,row))) ;
	#endif

		return  1 ;
	}

	ml_char_copy( &IMAGE_LINE(image,row).chars[char_index] , &image->sp_ch) ;
	IMAGE_LINE(image,row).num_of_filled_chars = char_index + 1 ;

	ml_imgline_update_change_char_index( &IMAGE_LINE(image,row) , char_index ,
		END_CHAR_INDEX(IMAGE_LINE(image,row)) , 1) ;

	if( row == image->cursor.row)
	{
		if( image->cursor.char_index > char_index)
		{
			image->cursor.char_index = char_index ;
			image->cursor.col = ml_convert_char_index_to_col( image , row , char_index , 0) ;
		}
	}

	return  1 ;
}

/*
 * this is used only internally(by ml_image and ml_image_scroll)
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

/*
 * this is only used internally(by ml_image/ml_image_scroll)
 */
int
ml_convert_char_index_to_col(
	ml_image_t *  image ,
	int  row ,
	int  char_index ,
	int  flag		/* BREAK_BOUNDARY */
	)
{
	int  counter ;
	int  col ;

#ifdef  DEBUG
	if( char_index >= image->num_of_chars)
	{
		/* this must never happens */
		
		kik_warn_printf( KIK_DEBUG_TAG " char index %d is larger than ml_image_t::num_of_chars(%d)\n" ,
			char_index , image->num_of_chars) ;

		abort() ;
	}
#endif
	
	if( row > END_ROW(image))
	{
		if( flag & BREAK_BOUNDARY)
		{
			/* return  ml_char_col( &image->sp_ch) * char_index ; */
			return  char_index ;
		}
		else
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " row %d is over END_ROW %d\n" ,
				row , END_ROW(image)) ;
		#endif

			return  0 ;
		}
	}

	col = 0 ;

	if( (flag & BREAK_BOUNDARY) && (IMAGE_LINE(image,row).num_of_filled_chars <= char_index))
	{
		for( counter = 0 ; counter < IMAGE_LINE(image,row).num_of_filled_chars ; counter ++)
		{
		#ifdef  DEBUG
			if( ml_char_cols( &IMAGE_LINE(image,row).chars[counter]) == 0)
			{
				kik_warn_printf( KIK_DEBUG_TAG " ml_char_cols returns 0.\n") ;
			
				continue ;
			}
		#endif
		
			col += ml_char_cols( &IMAGE_LINE(image,row).chars[counter]) ;
		}

		col += (char_index - counter) ;
	}
	else
	{
		if( char_index > END_CHAR_INDEX(IMAGE_LINE(image,row)))
		{
			char_index = END_CHAR_INDEX(IMAGE_LINE(image,row)) ;
		}
		
		/*
		 * excluding the width of the last char.
		 */
		for( counter = 0 ; counter < char_index ; counter ++)
		{
		#ifdef  DEBUG
			if( ml_char_cols( &IMAGE_LINE(image,row).chars[counter]) == 0)
			{
				ml_image_dump( image) ;
				kik_warn_printf( KIK_DEBUG_TAG " ml_char_cols returns 0.\n") ;
			
				continue ;
			}
		#endif
			
			col += ml_char_cols( &IMAGE_LINE(image,row).chars[counter]) ;
		}
	}

	return  col ;
}

/*
 * this is only used internally(by ml_image)
 */
int
ml_convert_col_to_char_index(
	ml_image_t *  image ,
	int *  cols_rest ,
	int  row ,
	int  col ,
	int  flag	/* BREAK_BOUNDARY */
	)
{
	int  char_index ;

#ifdef  DEBUG
	if( col >= image->num_of_chars)
	{
		/* this must never happen */
		
		kik_warn_printf( KIK_DEBUG_TAG " col %d is larger than ml_image_t::num_of_chars(%d)" ,
			col , image->num_of_chars) ;

		col = image->num_of_chars - 1 ;

		fprintf( stderr , " ... modified -> %d\n" , col) ;
	}
#endif

	if( row > END_ROW(image))
	{
		if( flag & BREAK_BOUNDARY)
		{
			/* return  col / ml_char_col( &image->sp_ch) ; */

			if( cols_rest)
			{
				*cols_rest = 0 ;
			}
			
			return  col ;
		}
		else
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " row %d is over END_ROW %d\n" ,
				row , END_ROW(image)) ;
		#endif

			if( cols_rest)
			{
				*cols_rest = col ;
			}

			return  0 ;
		}
	}
	
	char_index = 0 ;
	
	if( row <= END_ROW(image))
	{
		for( ; char_index < END_CHAR_INDEX(IMAGE_LINE(image,row)) ; char_index ++)
		{
			if( col < ml_char_cols( &IMAGE_LINE(image,row).chars[char_index]))
			{
				goto  end ;
			}

			col -= ml_char_cols( &IMAGE_LINE(image,row).chars[char_index]) ;
		}

		if( col < ml_char_cols( &IMAGE_LINE(image,row).chars[char_index]))
		{
			goto  end ;
		}
		else if( flag & BREAK_BOUNDARY)
		{
			col -= ml_char_cols( &IMAGE_LINE(image,row).chars[char_index++]) ;
		}
	}

	if( flag & BREAK_BOUNDARY)
	{
		while( col > 0)
		{
			col -- ;
			char_index ++ ;
		}
	}

end:
	if( cols_rest != NULL)
	{
		*cols_rest = col ;
	}

	return  char_index ;
}
