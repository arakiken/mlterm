/*
 *	$Id$
 */

#include  "ml_model.h"

#include  <kiklib/kik_mem.h>		/* malloc/free */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>


/* --- global functions --- */

int
ml_model_init(
	ml_model_t *  model ,
	u_int  num_of_cols ,
	u_int  num_of_rows
	)
{
	int  count ;

	if( num_of_rows == 0 || num_of_cols == 0)
	{
		return  0 ;
	}
		
	model->num_of_rows = num_of_rows ;
	model->num_of_cols = num_of_cols ;

	if( ( model->lines = malloc( sizeof( ml_line_t) * model->num_of_rows)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif
	
		return  0 ;
	}
	
	for( count = 0 ; count < model->num_of_rows ; count ++)
	{
		if( ! ml_line_init( &model->lines[count] , model->num_of_cols))
		{
			return  0 ;
		}
	}

	model->beg_row = 0 ;

	return  1 ;
}

int
ml_model_final(
	ml_model_t *  model
	)
{
	int  count ;
	
	for( count = 0 ; count < model->num_of_rows ; count ++)
	{
		ml_line_final( &model->lines[count]) ;
	}
	
	free( model->lines) ;

	return  1 ;
}

int
ml_model_reset(
	ml_model_t *  model
	)
{
	int  count ;
	
	for( count = 0 ; count < model->num_of_rows ; count ++)
	{
		ml_line_reset( &model->lines[count]) ;
		ml_line_updated( &model->lines[count]) ;
	}

	return  1 ;
}

int
ml_model_resize(
	ml_model_t *  model ,
	u_int *  slide ,
	u_int  num_of_cols ,
	u_int  num_of_rows
	)
{
	int  old_row ;
	int  new_row ;
	int  count ;
	u_int  copy_rows ;
	u_int  copy_len ;
	ml_line_t *  lines_p ;
	u_int  filled_rows ;

	if( num_of_cols == 0 || num_of_rows == 0)
	{
		return  0 ;
	}
	
	if( num_of_cols == model->num_of_cols && num_of_rows == model->num_of_rows)
	{
		/* not resized */
		
		return  1 ;
	}

	if( ( lines_p = malloc( sizeof( ml_line_t) * num_of_rows)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif
	
		return  0 ;
	}
	
	copy_len = K_MIN(num_of_cols , model->num_of_cols) ;

	count = model->num_of_rows - 1 ;
	while( 1)
	{
		if( ml_get_num_of_filled_chars_except_spaces( ml_model_get_line( model , count)) > 0)
		{
			filled_rows = count + 1 ;
			
			break ;
		}
		else if( -- count == 0)
		{
			/* Line num 0 must not be empty. */
			
			filled_rows = 0 ;
			
			break ;
		}
	}

	if( num_of_rows >= filled_rows)
	{
		old_row = 0 ;

		copy_rows = filled_rows ;
	}
	else
	{
		old_row = filled_rows - num_of_rows ;
		copy_rows = num_of_rows ;
	}

	if( slide)
	{
		*slide = old_row ;
	}

	/*
	 * updating each line.
	 */

	for( new_row = 0 ; new_row < copy_rows ; new_row ++)
	{
		ml_line_init( &lines_p[new_row] , num_of_cols) ;

		ml_line_copy_line( &lines_p[new_row] , ml_model_get_line( model , old_row)) ;
		old_row ++ ;
		
		ml_line_set_modified_all( &lines_p[new_row]) ;
	}

	for( ; new_row < num_of_rows ; new_row ++)
	{
		ml_line_init( &lines_p[new_row] , num_of_cols) ;

		ml_line_set_modified_all( &lines_p[new_row]) ;
	}

	/*
	 * freeing old data.
	 */
	 
	for( count = 0 ; count < model->num_of_rows ; count ++)
	{
		ml_line_final( &model->lines[count]) ;
	}

	free( model->lines) ;
	
	model->lines = lines_p ;

	model->num_of_rows = num_of_rows ;
	model->num_of_cols = num_of_cols ;

	model->beg_row = 0 ;

	return  1 ;
}

int
ml_model_end_row(
	ml_model_t *  model
	)
{
	return  model->num_of_rows - 1 ;
}

ml_line_t *
ml_model_get_line(
	ml_model_t *  model ,
	int  row
	)
{
	if( row < 0 || model->num_of_rows <= row)
	{
		return  NULL ;
	}

	if( model->beg_row + row < model->num_of_rows)
	{
		return  &model->lines[model->beg_row + row] ;
	}
	else
	{
		return  &model->lines[model->beg_row + row - model->num_of_rows] ;
	}
}

int
ml_model_scroll_upward(
	ml_model_t *  model ,
	u_int  size
	)
{
	if( size > model->num_of_rows)
	{
		size = model->num_of_rows ;
	}
	
	if( model->beg_row + size >= model->num_of_rows)
	{
		model->beg_row = model->beg_row + size - model->num_of_rows ;
	}
	else
	{
		model->beg_row += size ;
	}
	
	return  1 ;
}

int
ml_model_scroll_downward(
	ml_model_t *  model ,
	u_int  size
	)
{
	if( size > model->num_of_rows)
	{
		size = model->num_of_rows ;
	}
	
	if( model->beg_row < size)
	{
		model->beg_row = model->num_of_rows - (size - model->beg_row) ;
	}
	else
	{
		model->beg_row -= size ;
	}
	
	return  1 ;
}

#ifdef  DEBUG

void
ml_model_dump(
	ml_model_t *  model
	)
{
	int  row ;
	ml_line_t *  line ;
	
	for( row = 0 ; row < model->num_of_rows ; row++)
	{
		line = ml_model_get_line( model , row) ;

		if( ml_line_is_modified( line))
		{
			kik_msg_printf( "!%.2d-%.2d" ,
				ml_line_get_beg_of_modified( line) ,
				ml_line_get_end_of_modified( line)) ;
		}
		else
		{
			kik_msg_printf( "      ") ;
		}
		
		kik_msg_printf( "[%.2d %.2d]" , line->num_of_filled_chars ,
			ml_line_get_num_of_filled_cols( line)) ;

		ml_str_dump( line->chars , line->num_of_filled_chars) ;
	}
}

#endif
