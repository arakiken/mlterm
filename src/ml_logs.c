/*
 *	$Id$
 */

#include  "ml_logs.h"

#include  <string.h>		/* memmove/memset */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>


#if  0
#define  __DEBUG
#endif

/*
 * if malloc() of your system is poor , you may undef this macro and use fixed length log buffer.
 */
#if  1
#define  MALLOC_LOG_BY_LINE
#endif


/* --- static functions --- */

static ml_image_line_t *
get_log_line(
	ml_logs_t *  logs ,
	int  at
	)
{
	int  _at ;
	
	if( at < 0 || ml_get_num_of_logged_lines( logs) <= at)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " row %d is overflowed in logs.\n" , at) ;
	#endif
	
		return  NULL ;
	}

	if( ( _at = kik_cycle_index_of( logs->index , at)) == -1)
	{
		return  NULL ;
	}

	return  &logs->lines[_at] ;
}


/* --- global functions --- */

int
ml_log_init(
	ml_logs_t *  logs ,
	u_int  num_of_chars ,
	u_int  num_of_rows ,
	ml_char_t  nl_ch
	)
{
#ifndef  MALLOC_LOG_BY_LINE
	ml_char_t *  chars_p ;
	int  counter ;
#endif
	
	if( ( logs->lines = malloc( sizeof( ml_image_line_t) * num_of_rows)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  0 ;
	}
	memset( logs->lines , 0 , sizeof( ml_image_line_t) * num_of_rows) ;

#ifndef  MALLOC_LOG_BY_LINE
	if( ( chars_p = ml_str_new( num_of_chars * num_of_rows)) == NULL)
	{
		return  0 ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %d bytes are allocated.(ml_char_t size is %d)\n" ,
		sizeof( ml_char_t) * num_of_chars * num_of_rows , sizeof( ml_char_t)) ;
#endif

	for( counter = 0 ; counter < num_of_rows ; counter ++)
	{
		logs->lines[counter].chars = chars_p ;
		chars_p += num_of_chars ;
	}
#endif

	if( ( logs->index = kik_cycle_index_new( num_of_rows)) == NULL)
	{
		return  0 ;
	}

	logs->num_of_chars = num_of_chars ;
	logs->num_of_rows = num_of_rows ;

	ml_char_init( &logs->nl_ch) ;
	ml_char_copy( &logs->nl_ch , &nl_ch) ;
	
	return  1 ;
}

int
ml_log_final(
	ml_logs_t *  logs
	)
{
	int  counter ;

#ifdef  MALLOC_LOG_BY_LINE
	for( counter = 0 ; counter < logs->num_of_rows ; counter ++)
	{
		ml_str_delete( logs->lines[counter].chars , logs->lines[counter].num_of_filled_chars) ;
	}
#else
	ml_str_delete( logs->lines[0].chars) ;
#endif

	kik_cycle_index_delete( logs->index) ;
	
	free( logs->lines) ;

	return  1 ;	
}

int
ml_log_resized(
	ml_logs_t *  logs ,
	u_int  new_num_of_chars
	)
{
#ifndef  MALLOC_LOG_BY_LINE
	/*
	 * never resized smaller.
	 */

	if( new_num_of_chars > logs->num_of_chars)
	{
		u_int  old_num_of_chars ;
		int  counter ;
		ml_char_t *  chars_p ;
		ml_char_t *  old_chars_p ;

		old_num_of_chars = logs->num_of_chars ;
		old_chars_p = logs->lines[0].chars ;

		if( ( chars_p = ml_str_new( new_num_of_chars * logs->num_of_rows)) == NULL)
		{
			return  0 ;
		}

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " %d bytes are allocated.(ml_char_t size is %d)\n" ,
			sizeof( ml_char_t) * new_num_of_chars * logs->num_of_rows , sizeof( ml_char_t)) ;
	#endif
	
		for( counter = 0 ; counter < logs->num_of_rows ; counter ++)
		{
			memmove( chars_p , logs->lines[counter].chars ,
				sizeof( ml_char_t) * old_num_of_chars) ;

			logs->lines[counter].chars = chars_p ;
			chars_p += new_num_of_chars ;
		}

		logs->num_of_chars = new_num_of_chars ;

		free( old_chars_p) ;
	}
#endif

	return  1 ;
}

/*
 * XXX
 * this doesn't support the case MALLOC_LOG_BY_LINE is not defined.
 */
int
ml_change_log_size(
	ml_logs_t *  logs ,
	u_int  new_num_of_rows
	)
{
	u_int  num_of_filled_rows ;

	num_of_filled_rows = ml_get_num_of_logged_lines( logs) ;	

	if( new_num_of_rows == logs->num_of_rows)
	{
		return  1 ;
	}
	else if( new_num_of_rows > logs->num_of_rows)
	{
	#ifdef  MALLOC_LOG_BY_LINE

		ml_image_line_t *  new_lines ;
		
		if( ( new_lines = realloc( logs->lines , sizeof( ml_image_line_t) * new_num_of_rows))
			== NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
		#endif

			return  0 ;
		}

		/* memset 0 the enlarged area */
		memset( &new_lines[logs->num_of_rows] , 0 ,
			sizeof( ml_image_line_t) * (new_num_of_rows - logs->num_of_rows)) ;
		
		logs->lines = new_lines ;
		
	#else
	
		kik_msg_printf( "enlaring log size is not supported now:)\n") ;
		
	#endif
	}
	else if( new_num_of_rows < logs->num_of_rows)
	{
	#ifdef  MALLOC_LOG_BY_LINE
	
		ml_image_line_t *  new_lines ;
		ml_image_line_t *  line ;
		int  counter ;
		int  start ;

		if( ( new_lines = malloc( sizeof( ml_image_line_t) * new_num_of_rows)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
		#endif
		
			return  0 ;
		}
		memset( new_lines , 0 , sizeof( ml_image_line_t) * new_num_of_rows) ;

		if( new_num_of_rows >= num_of_filled_rows)
		{
			start = 0 ;
		}
		else
		{
			start = num_of_filled_rows - new_num_of_rows ;
		}

		for( counter = 0 ; counter < start ; counter ++)
		{
			if( ( line = get_log_line( logs , counter)) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " this is impossible.\n") ;
			#endif
			
				free( new_lines) ;
				
				return  0 ;
			}

			ml_str_delete( line->chars , line->num_of_filled_chars) ;
		}
		
		for( counter = 0 ; counter < new_num_of_rows ; counter ++)
		{
			if( ( line = get_log_line( logs , counter + start)) == NULL)
			{
				break ;
			}
			
			new_lines[counter].chars = line->chars ;
			new_lines[counter].num_of_filled_chars = line->num_of_filled_chars ;
			new_lines[counter].num_of_filled_cols = line->num_of_filled_cols ;
			new_lines[counter].is_continued_to_next = line->is_continued_to_next ;

			/* not used */
			new_lines[counter].is_modified = 0 ;
			new_lines[counter].change_beg_char_index = 0 ;
			new_lines[counter].change_end_char_index = 0 ;
		}

		free( logs->lines) ;
		logs->lines = new_lines ;
		
	#else
	
		kik_msg_printf( "shrinking log size is not supported now:)\n") ;
		
	#endif
	}
	
	logs->num_of_rows = new_num_of_rows ;
	
	if( ! kik_cycle_index_change_size( logs->index , new_num_of_rows))
	{
		/* unrecoverable error. this will cause unexpected behaviors... */

		return  0 ;
	}

	return  1 ;
}

int
ml_log_add(
	ml_logs_t *  logs ,
	ml_image_line_t *  line
	)
{
	int  at ;

	at = kik_next_cycle_index( logs->index) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %d len line logged to index %d.\n" , 
		line->num_of_filled_chars , at) ;
#endif

#ifdef  MALLOC_LOG_BY_LINE
	ml_str_delete( logs->lines[at].chars , logs->lines[at].num_of_filled_chars) ;
	
	if( ( logs->lines[at].chars = ml_str_new( line->num_of_filled_chars))
		== NULL)
	{
		return  0 ;
	}
#endif

	ml_str_copy( logs->lines[at].chars , line->chars , line->num_of_filled_chars) ;
	logs->lines[at].num_of_filled_chars = line->num_of_filled_chars ;
	logs->lines[at].num_of_filled_cols = line->num_of_filled_cols ;
	logs->lines[at].is_continued_to_next = line->is_continued_to_next ;

	/* not used */
	logs->lines[at].is_modified = 0 ;
	logs->lines[at].change_beg_char_index = 0 ;
	logs->lines[at].change_end_char_index = 0 ;
	
	return  1 ;
}

ml_image_line_t *
ml_log_get(
	ml_logs_t *  logs ,
	int  at
	)
{
	return  get_log_line( logs , at) ;
}

u_int
ml_get_num_of_logged_lines(
	ml_logs_t *  logs
	)
{
	return  kik_get_filled_cycle_index( logs->index) ;
}

int
ml_log_reverse_color(
	ml_logs_t *  logs ,
	int  char_index ,
	int  row
	)
{
	ml_image_line_t *  line ;

	line = ml_log_get( logs , row) ;
	
	ml_char_reverse_color( &line->chars[char_index]) ;

	ml_imgline_update_change_char_index( line , char_index , END_CHAR_INDEX(*line) , 0) ;

	return  1 ;
}

int
ml_log_restore_color(
	ml_logs_t *  logs ,
	int  char_index ,
	int  row
	)
{
	ml_image_line_t *  line ;

	line = ml_log_get( logs , row) ;

	ml_char_restore_color( &line->chars[char_index]) ;

	ml_imgline_update_change_char_index( line , char_index , END_CHAR_INDEX(*line) , 0) ;

	return  1 ;
}

/*
 * !! Notice !!
 * this is closely related to ml_image_copy_region()/ml_reverse_or_restore_color()
 */
u_int
ml_log_copy_region(
	ml_logs_t *  logs ,
	ml_char_t *  chars ,
	u_int  num_of_chars ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	ml_image_line_t *  line ;
	int  counter ;
	int  char_index ;
	u_int  size_except_end_space ;

	while( 1)
	{
		if( ( line = get_log_line( logs , beg_row)) == NULL)
		{
			return  0 ;
		}

		size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;

		if( beg_char_index < size_except_end_space)
		{
			break ;
		}
		
		beg_row ++ ;
		beg_char_index = 0 ;
	}
	
	counter = 0 ;
	
	if( beg_row == end_row)
	{
		size_except_end_space = K_MIN(end_char_index + 1,size_except_end_space) ;
		
		for( char_index = beg_char_index ;
			char_index < size_except_end_space ; char_index ++)
		{
			ml_char_copy( &chars[counter++] , &line->chars[char_index]) ;
		}
	}
	else if( beg_row < end_row)
	{
		int  row ;
		
		for( char_index = beg_char_index ; char_index < size_except_end_space ; char_index ++)
		{
			ml_char_copy( &chars[counter++] , &line->chars[char_index]) ;
		}

		if( ! line->is_continued_to_next)
		{
			ml_char_copy( &chars[counter++] , &logs->nl_ch) ;
		}

		for( row = beg_row + 1 ; row < end_row ; row ++)
		{
			if( ( line = get_log_line( logs , row)) == NULL)
			{
				return  counter ;
			}

			size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;

			for( char_index = 0 ; char_index < size_except_end_space ; char_index ++)
			{
				ml_char_copy( &chars[counter++] , &line->chars[char_index]) ;
			}

			if( ! line->is_continued_to_next)
			{
				ml_char_copy( &chars[counter++] , &logs->nl_ch) ;
			}
		}

		if( ( line = get_log_line( logs , row)) == NULL)
		{
			return  counter ;
		}

		size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;

		for( char_index = 0 ; char_index < K_MIN(end_char_index + 1,size_except_end_space) ;
			char_index ++)
		{
			ml_char_copy( &chars[counter++] , &line->chars[char_index]) ;
		}
	}
#ifdef  DEBUG
	else
	{
		kik_warn_printf( KIK_DEBUG_TAG " illegal region.\n") ;
	}
#endif

	return  counter ;
}

u_int
ml_log_get_region_size(
	ml_logs_t *  logs ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	ml_image_line_t *  line ;
	int  region_size ;
	u_int  size_except_end_space ;

	while( 1)
	{
		if( ( line = get_log_line( logs , beg_row)) == NULL)
		{
			return  0 ;
		}

		size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;

		if( beg_char_index < size_except_end_space)
		{
			break ;
		}
		
		beg_row ++ ;
		beg_char_index = 0 ;
	}

	if( beg_row == end_row)
	{
		region_size = K_MIN(end_char_index + 1,size_except_end_space) - beg_char_index ;
	}
	else if( beg_row < end_row)
	{
		int  row ;

		region_size = size_except_end_space - beg_char_index ;

		if( ! line->is_continued_to_next)
		{
			/* for NL */
			region_size ++ ;
		}

		for( row = beg_row + 1 ; row < end_row ; row ++)
		{
			if( ( line = get_log_line( logs , row)) == NULL)
			{
				return  region_size ;
			}

			region_size += ml_get_num_of_filled_chars_except_end_space( line) ;

			if( ! line->is_continued_to_next)
			{
				/* for NL */
				region_size ++ ;
			}
		}

		if( ( line = get_log_line( logs , row)) == NULL)
		{
			return  region_size ;
		}

		size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;
		region_size += (K_MIN(end_char_index + 1,size_except_end_space)) ;
	}
	else
	{
		region_size = 0 ;
	}

	return  region_size ;
}

void
ml_log_is_updated(
	ml_logs_t *  logs
	)
{
	int  counter ;
	u_int  num_of_rows ;

	num_of_rows = kik_get_filled_cycle_index( logs->index) ;

	for( counter = 0 ; counter < num_of_rows ; counter ++)
	{
		logs->lines[ kik_cycle_index_of( logs->index , counter)].is_modified = 0 ;
	}
}
