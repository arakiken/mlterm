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
	u_int  num_of_rows
	)
{
	if( ( logs->lines = malloc( sizeof( ml_image_line_t) * num_of_rows)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  0 ;
	}
	memset( logs->lines , 0 , sizeof( ml_image_line_t) * num_of_rows) ;

	if( ( logs->index = kik_cycle_index_new( num_of_rows)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " kik_cycle_index_new() failed.\n") ;
	#endif
	
		free( logs->lines) ;
		
		return  0 ;
	}

	logs->num_of_chars = num_of_chars ;
	logs->num_of_rows = num_of_rows ;

	return  1 ;
}

int
ml_log_final(
	ml_logs_t *  logs
	)
{
	int  counter ;

	for( counter = 0 ; counter < logs->num_of_rows ; counter ++)
	{
		ml_imgline_final( &logs->lines[counter]) ;
	}

	kik_cycle_index_delete( logs->index) ;
	
	free( logs->lines) ;

	return  1 ;
}

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
		ml_image_line_t *  new_lines ;
		
		if( ( new_lines = realloc( logs->lines , sizeof( ml_image_line_t) * new_num_of_rows))
			== NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " realloc() failed.\n") ;
		#endif

			return  0 ;
		}

		memset( &new_lines[logs->num_of_rows] , 0 ,
			sizeof( ml_image_line_t) * (new_num_of_rows - logs->num_of_rows)) ;
		
		logs->lines = new_lines ;
	}
	else if( new_num_of_rows < logs->num_of_rows)
	{
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

		/*
		 * freeing excess lines.
		 */
		for( counter = 0 ; counter < start ; counter ++)
		{
			if( ( line = get_log_line( logs , counter)) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " this is impossible.\n") ;
			#endif
			
				return  0 ;
			}

			ml_imgline_final( line) ;
		}

		/*
		 * copying to new lines.
		 */
		for( counter = 0 ; counter < new_num_of_rows ; counter ++)
		{
			if( ( line = get_log_line( logs , counter + start)) == NULL)
			{
				break ;
			}

			ml_imgline_init( &new_lines[counter] , line->num_of_filled_chars) ;
			ml_imgline_share( &new_lines[counter] , line) ;
		}

		free( logs->lines) ;
		logs->lines = new_lines ;
	}
	
	logs->num_of_rows = new_num_of_rows ;
	
	if( ! kik_cycle_index_change_size( logs->index , new_num_of_rows))
	{
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

	ml_imgline_final( &logs->lines[at]) ;

	ml_imgline_clone( &logs->lines[at] , line , line->num_of_filled_chars) ;
	
	ml_imgline_is_updated( &logs->lines[at]) ;

	/*
	 * !! Notice !!
	 * backscroll log lines will not be modified any more.
	 * so they should be bidi visual order.
	 */
	ml_imgline_start_bidi( &logs->lines[at]) ;

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

u_int
ml_get_log_size(
	ml_logs_t *  logs
	)
{
	return  logs->num_of_rows ;
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

	ml_imgline_update_change_char_index( line , char_index , ml_imgline_end_char_index(line) , 0) ;

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

	ml_imgline_update_change_char_index( line , char_index , ml_imgline_end_char_index(line) , 0) ;

	return  1 ;
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
