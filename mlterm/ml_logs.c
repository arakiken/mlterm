/*
 *	$Id$
 */

#include  "ml_logs.h"

#include  <string.h>		/* memmove/memset */
#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>


#if  0
#define  __DEBUG
#endif


/* --- global functions --- */

int
ml_log_init(
	ml_logs_t *  logs ,
	u_int  num_of_rows
	)
{
	logs->lines = NULL ;
	logs->index = NULL ;
	logs->num_of_rows = 0;
	
	if( num_of_rows == 0)
	{
		return  1 ;
	}
		
	if( ( logs->lines = calloc(  sizeof( ml_line_t), num_of_rows)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " calloc() failed.\n") ;
	#endif
		return  0 ;
	}

	if( ( logs->index = kik_cycle_index_new( num_of_rows)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " kik_cycle_index_new() failed.\n") ;
	#endif

		free( logs->lines) ;
		logs->lines= NULL ;

		return  0 ;
	}

	logs->num_of_rows = num_of_rows ;

	return  1 ;
}

int
ml_log_final(
	ml_logs_t *  logs
	)
{
	int  count ;

	if( logs->num_of_rows == 0)
	{
		return  1 ;
	}
	
	for( count = 0 ; count < logs->num_of_rows ; count ++)
	{
		ml_line_final( &logs->lines[count]) ;
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
	else if( new_num_of_rows == 0)
	{
		free( logs->lines) ;
		logs->lines = NULL ;

		kik_cycle_index_delete( logs->index) ;
		logs->index = NULL ;

		logs->num_of_rows = 0 ;

		return  1 ;
	}
	else if( new_num_of_rows > logs->num_of_rows)
	{
		ml_line_t *  new_lines ;
		if(  sizeof( ml_line_t) * new_num_of_rows <  sizeof( ml_line_t) * logs->num_of_rows){
			/* integer overflow */
			return  0 ;
		}
		if( ( new_lines = realloc( logs->lines , sizeof( ml_line_t) * new_num_of_rows))
			== NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " realloc() failed.\n") ;
		#endif

			return  0 ;
		}

		memset( &new_lines[logs->num_of_rows] , 0 ,
			sizeof( ml_line_t) * (new_num_of_rows - logs->num_of_rows)) ;
		
		logs->lines = new_lines ;
	}
	else if( new_num_of_rows < logs->num_of_rows)
	{
		ml_line_t *  new_lines ;
		ml_line_t *  line ;
		int  count ;
		int  start ;

		if( ( new_lines = calloc( sizeof( ml_line_t), new_num_of_rows)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " calloc() failed.\n") ;
		#endif

			return  0 ;
		}
		
		num_of_filled_rows = ml_get_num_of_logged_lines( logs) ;
		
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
		for( count = 0 ; count < start ; count ++)
		{
			if( ( line = ml_log_get( logs , count)) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " this is impossible.\n") ;
			#endif
			
				return  0 ;
			}

			ml_line_final( line) ;
		}

		/*
		 * copying to new lines.
		 */
		for( count = 0 ; count < new_num_of_rows ; count ++)
		{
			if( ( line = ml_log_get( logs , count + start)) == NULL)
			{
				break ;
			}

			ml_line_init( &new_lines[count] , line->num_of_filled_chars) ;
			ml_line_share( &new_lines[count] , line) ;
		}

		free( logs->lines) ;
		logs->lines = new_lines ;
	}

	if( logs->index)
	{
		if( ! kik_cycle_index_change_size( logs->index , new_num_of_rows))
		{
			return  0 ;
		}
	}
	else
	{
		if( ( logs->index = kik_cycle_index_new( new_num_of_rows)) == NULL)
		{
			return  0 ;
		}
	}

	logs->num_of_rows = new_num_of_rows ;
	
	return  1 ;
}

int
ml_log_add(
	ml_logs_t *  logs ,
	ml_line_t *  line
	)
{
	int  at ;

	if( logs->num_of_rows == 0)
	{
		return  1 ;
	}
	
	at = kik_next_cycle_index( logs->index) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %d len line logged to index %d.\n" , 
		line->num_of_filled_chars , at) ;
#endif

	ml_line_final( &logs->lines[at]) ;

	/* logs->lines[at] becomes completely the same one as line */
	ml_line_clone( &logs->lines[at] , line , line->num_of_filled_chars) ;

	ml_line_updated( &logs->lines[at]) ;

	return  1 ;
}

ml_line_t *
ml_log_get(
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

u_int
ml_get_num_of_logged_lines(
	ml_logs_t *  logs
	)
{
	if( logs->num_of_rows == 0)
	{
		return  0 ;
	}
	else
	{
		return  kik_get_filled_cycle_index( logs->index) ;
	}
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
	ml_line_t *  line ;

	if( ( line = ml_log_get( logs , row)) == NULL)
	{
		return  0 ;
	}
	
	ml_char_reverse_color( ml_char_at( line , char_index)) ;

	ml_line_set_modified( line , char_index , ml_line_end_char_index(line)) ;

	return  1 ;
}

int
ml_log_restore_color(
	ml_logs_t *  logs ,
	int  char_index ,
	int  row
	)
{
	ml_line_t *  line ;

	if( ( line = ml_log_get( logs , row)) == NULL)
	{
		return  0 ;
	}

	ml_char_restore_color( ml_char_at( line , char_index)) ;

	ml_line_set_modified( line , char_index , ml_line_end_char_index(line)) ;

	return  1 ;
}
