/*
 *	$Id$
 */

#include  "ml_backscroll.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>


#define  ROW_IN_LOGS(bs_image,row) \
	( ml_get_num_of_logged_lines((bs_image)->logs) + row)

#if  0
#define  __DEBUG
#endif


/* --- global functions --- */

int
ml_bs_init(
	ml_bs_image_t *  bs_image ,
	ml_image_t *  image ,
	ml_logs_t *  logs ,
	ml_bs_event_listener_t *  bs_listener
	)
{
	bs_image->image = image ;
	bs_image->logs = logs ;
	bs_image->bs_listener = bs_listener ;

	bs_image->backscroll_rows = 0 ;
	bs_image->is_backscroll_mode = 0 ;

	return  1 ;
}

int
ml_bs_final(
	ml_bs_image_t *  bs_image
	)
{
	return  1 ;
}

int
ml_bs_change_image(
	ml_bs_image_t *  bs_image ,
	ml_image_t *  image
	)
{
	bs_image->image = image ;

	return  1 ;
}

int
ml_is_backscroll_mode(
	ml_bs_image_t *  bs_image
	)
{
	return  bs_image->is_backscroll_mode ;
}

void
ml_set_backscroll_mode(
	ml_bs_image_t *  bs_image
	)
{
	bs_image->is_backscroll_mode = 1 ;
}

void
ml_unset_backscroll_mode(
	ml_bs_image_t *  bs_image
	)
{
	bs_image->is_backscroll_mode = 0 ;
	bs_image->backscroll_rows = 0 ;
}
	
ml_image_line_t *
ml_bs_get_image_line_in_all(
	ml_bs_image_t *  bs_image ,
	int  row
	)
{
	u_int  num_of_filled_lines ;

	num_of_filled_lines = ml_get_num_of_logged_lines( bs_image->logs) ;

	if( num_of_filled_lines < bs_image->backscroll_rows)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " backscroll is overflowed.\n") ;
	#endif

		return  NULL ;
	}
	else if( row == ml_image_get_rows( bs_image->image))
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " row %d is at the end of screen.\n" , row) ;
	#endif
		return  NULL ;
	}

	if( row < 0)
	{
		return  ml_log_get( bs_image->logs , ROW_IN_LOGS(bs_image,row)) ;
	}
	else
	{
		return  ml_image_get_line( bs_image->image , row) ;
	}
}

ml_image_line_t *
ml_bs_get_image_line_in_screen(
	ml_bs_image_t *  bs_image ,
	int  row_in_scr
	)
{
	if( bs_image->is_backscroll_mode && bs_image->backscroll_rows > 0)
	{
		u_int  num_of_filled_lines ;

		num_of_filled_lines = ml_get_num_of_logged_lines( bs_image->logs) ;

		if( num_of_filled_lines < bs_image->backscroll_rows)
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " backscroll is overflowed.\n") ;
		#endif
			
			return  NULL ;
		}
		else if( row_in_scr == ml_image_get_rows( bs_image->image))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " row %d is at the end of screen.\n") ;
		#endif
			return  NULL ;
		}

		if( row_in_scr < bs_image->backscroll_rows)
		{
			return  ml_log_get( bs_image->logs ,
				ROW_IN_LOGS(bs_image,row_in_scr - bs_image->backscroll_rows)) ;
		}
		else
		{
			return  ml_image_get_line( bs_image->image ,
				row_in_scr - bs_image->backscroll_rows) ;
		}
	}
	else
	{
		return  ml_image_get_line( bs_image->image , row_in_scr) ;
	}
}

int
ml_bs_scroll_to(
	ml_bs_image_t *  bs_image ,
	int  beg_row
	)
{
	int  counter ;
	ml_image_line_t *  line ;

	if( ! bs_image->is_backscroll_mode)
	{
		return  0 ;
	}

	if( beg_row > 0)
	{
		bs_image->backscroll_rows = 0 ;
	}
	else
	{
		bs_image->backscroll_rows = abs( beg_row) ;
	}

	for( counter = 0 ; counter < ml_image_get_rows( bs_image->image) ; counter ++)
	{
		if( ( line = ml_bs_get_image_line_in_screen( bs_image , counter)) == NULL)
		{
			return  0 ;
		}

		ml_imgline_set_modified( line) ;
	}
	
	return  1 ;
}

int
ml_bs_scroll_upward(
	ml_bs_image_t *  bs_image ,
	u_int  size
	)
{
	int  counter ;

	if( ! bs_image->is_backscroll_mode)
	{
		return  0 ;
	}

	if( bs_image->backscroll_rows < size)
	{
		size = bs_image->backscroll_rows ;
	}
	
	bs_image->backscroll_rows -= size ;
	
	if( ! bs_image->bs_listener->window_scroll_upward( bs_image->bs_listener->self , size))
	{
		for( counter = 0 ; counter < ml_image_get_rows( bs_image->image) - size ; counter++)
		{
			ml_imgline_set_modified( ml_bs_get_image_line_in_screen( bs_image , counter)) ;
		}
	}

	for( counter = ml_image_get_rows( bs_image->image) - size ;
		counter < ml_image_get_rows( bs_image->image) ; counter++)
	{
		ml_imgline_set_modified( ml_bs_get_image_line_in_screen( bs_image , counter)) ;
	}
	
	return  1 ;
}

int
ml_bs_scroll_downward(
	ml_bs_image_t *  bs_image ,
	u_int  size
	)
{
	int  counter ;
	
	if( ! bs_image->is_backscroll_mode)
	{
		return  0 ;
	}

	if( ml_get_num_of_logged_lines( bs_image->logs) < bs_image->backscroll_rows + size)
	{
		size = ml_get_num_of_logged_lines( bs_image->logs) - bs_image->backscroll_rows ;
	}
	
	bs_image->backscroll_rows += size ;

	if( ! bs_image->bs_listener->window_scroll_downward( bs_image->bs_listener->self , size))
	{
		for( counter = size ; counter < ml_image_get_rows( bs_image->image) ; counter++)
		{
			ml_imgline_set_modified( ml_bs_get_image_line_in_screen( bs_image , counter)) ;
		}
	}
	
	for( counter = 0 ; counter < size ; counter ++)
	{
		ml_imgline_set_modified( ml_bs_get_image_line_in_screen( bs_image , counter)) ;
	}
	
	return  1 ;
}

void
ml_bs_is_updated(
	ml_bs_image_t *   bs_image
	)
{
	ml_image_is_updated( bs_image->image) ;
	ml_log_is_updated( bs_image->logs) ;
}

u_int
ml_bs_copy_region(
	ml_bs_image_t *  bs_image ,
	ml_char_t *  chars ,
	u_int  num_of_chars ,
        int  beg_col ,
	int  beg_row ,
	int  end_col ,
	int  end_row
	)
{
	u_int  read_size ;

	read_size = 0 ;
	
	if( beg_row < 0)
	{
		int  log_beg_row ;
		int  log_end_row ;
		int  log_beg_col ;
		int  log_end_col ;

		log_beg_row = ROW_IN_LOGS(bs_image,beg_row) ;
		log_beg_col = beg_col ;

		if( end_row < 0)
		{
			log_end_row = ROW_IN_LOGS(bs_image,end_row) ;
			log_end_col = end_col ;
		}
		else
		{
			log_end_row = ml_get_num_of_logged_lines( bs_image->logs) - 1 ;
			log_end_col = 0 ;
		}

		read_size += ml_log_copy_region( bs_image->logs , &chars[read_size] ,
			num_of_chars - read_size , log_beg_col , log_beg_row , log_end_col ,
			log_end_row) ;
		
		beg_col = 0 ;
		beg_row = 0 ;
	}

	if( end_row >= 0)
	{
		read_size += ml_image_copy_region( bs_image->image , &chars[read_size] ,
			num_of_chars - read_size , beg_col , beg_row , end_col , end_row) ;
	}

	return  read_size ;
}

u_int
ml_bs_get_region_size(
	ml_bs_image_t *  bs_image ,
        int  beg_col ,
	int  beg_row ,
	int  end_col ,
	int  end_row
	)
{
	u_int  read_size ;

	read_size = 0 ;
	
	if( beg_row < 0)
	{
		int  log_beg_row ;
		int  log_end_row ;
		int  log_beg_col ;
		int  log_end_col ;

		log_beg_row = ROW_IN_LOGS(bs_image,beg_row) ;
		log_beg_col = beg_col ;

		if( end_row < 0)
		{
			log_end_row = ROW_IN_LOGS(bs_image,end_row) ;
			log_end_col = end_col ;
		}
		else
		{
			log_end_row = ml_get_num_of_logged_lines( bs_image->logs) - 1 ;
			log_end_col = 0 ;
		}

		read_size += ml_log_get_region_size( bs_image->logs , log_beg_col ,
			log_beg_row , log_end_col , log_end_row) ;
		
		beg_col = 0 ;
		beg_row = 0 ;
	}

	if( end_row >= 0)
	{
		read_size += ml_image_get_region_size( bs_image->image , beg_col ,
			beg_row , end_col , end_row) ;
	}

	return  read_size ;
}

int
ml_convert_row_to_bs_row(
	ml_bs_image_t *  bs_image ,
	int  row
	)
{
	return row - bs_image->backscroll_rows ;
}
