/*
 *	$Id$
 */

#include  "ml_image_model.h"

#include  <kiklib/kik_mem.h>		/* malloc/free */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>


/* --- global functions --- */

int
ml_imgmdl_init(
	ml_image_model_t *  model ,
	u_int  num_of_cols ,
	u_int  num_of_rows
	)
{
	int  counter ;
	
	model->num_of_rows = num_of_rows ;
	model->num_of_cols = num_of_cols ;
	
	if( ( model->lines = malloc( sizeof( ml_image_line_t) * model->num_of_rows)) == NULL)
	{
		return  0 ;
	}
	
	for( counter = 0 ; counter < model->num_of_rows ; counter ++)
	{
		if( ! ml_imgline_init( &model->lines[counter] , model->num_of_cols))
		{
			return  0 ;
		}
	}

	model->num_of_filled_rows = 0 ;
	model->beg_row = 0 ;

	return  1 ;
}

int
ml_imgmdl_final(
	ml_image_model_t *  model
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < model->num_of_rows ; counter ++)
	{
		ml_imgline_final( &model->lines[counter]) ;
	}
	
	free( model->lines) ;

	return  1 ;
}

int
ml_imgmdl_reset(
	ml_image_model_t *  model
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < model->num_of_rows ; counter ++)
	{
		ml_imgline_reset( &model->lines[counter]) ;
		ml_imgline_updated( &model->lines[counter]) ;
	}

	model->num_of_filled_rows = 0 ;

	return  1 ;
}

int
ml_imgmdl_resize(
	ml_image_model_t *  model ,
	u_int  num_of_cols ,
	u_int  num_of_rows
	)
{
	int  old_row ;
	int  new_row ;
	int  counter ;
	u_int  copy_rows ;
	u_int  copy_len ;
	ml_image_line_t *  lines_p ;

	if( num_of_cols == model->num_of_cols && num_of_rows == model->num_of_rows)
	{
		/* not resized */
		
		return  0 ;
	}

	if( ( lines_p = malloc( sizeof( ml_image_line_t) * num_of_rows)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif
	
		return  0 ;
	}
	
	copy_len = K_MIN(num_of_cols , model->num_of_cols) ;

	if( num_of_rows >= model->num_of_filled_rows)
	{
		old_row = 0 ;
		copy_rows = model->num_of_filled_rows ;
	}
	else
	{
		old_row = model->num_of_filled_rows - num_of_rows ;
		copy_rows = num_of_rows ;
	}

	/*
	 * updating each line.
	 */

	for( new_row = 0 ; new_row < copy_rows ; new_row ++)
	{
		ml_imgline_init( &lines_p[new_row] , num_of_cols) ;

		ml_imgline_copy_line( &lines_p[new_row] , ml_imgmdl_get_line( model , old_row)) ;
		old_row ++ ;
		
		ml_imgline_set_modified_all( &lines_p[new_row]) ;
	}

	model->num_of_filled_rows = new_row ;

	for( ; new_row < num_of_rows ; new_row ++)
	{
		ml_imgline_init( &lines_p[new_row] , num_of_cols) ;

		ml_imgline_set_modified_all( &lines_p[new_row]) ;
	}

	/*
	 * freeing old data.
	 */
	 
	for( counter = 0 ; counter < model->num_of_rows ; counter ++)
	{
		ml_imgline_final( &model->lines[counter]) ;
	}

	free( model->lines) ;
	
	model->lines = lines_p ;

	model->num_of_rows = num_of_rows ;
	model->num_of_cols = num_of_cols ;

	model->beg_row = 0 ;

	return  1 ;
}

int
ml_imgmdl_end_row(
	ml_image_model_t *  model
	)
{
	if( model->num_of_filled_rows == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " num_of_filled_rows is 0.\n") ;
	#endif
	
		return  0 ;
	}
	else
	{
		return  model->num_of_filled_rows - 1 ;
	}
}

ml_image_line_t *
ml_imgmdl_get_line(
	ml_image_model_t *  model ,
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

ml_image_line_t *
ml_imgmdl_get_end_line(
	ml_image_model_t *  model
	)
{
	return  ml_imgmdl_get_line( model , model->num_of_filled_rows - 1) ;
}

u_int
ml_imgmdl_reserve_boundary(
	ml_image_model_t *  model ,
	u_int  size
	)
{
	if( model->num_of_filled_rows + size > model->num_of_rows)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " breaking from line %d by size %d failed." ,
			model->num_of_filled_rows - 1 , size) ;
	#endif

		size = model->num_of_rows - model->num_of_filled_rows ;

	#ifdef  DEBUG
		kik_msg_printf( " ... size is modified -> %d\n" , size) ;
	#endif
	}

	model->num_of_filled_rows += size ;

	return  size ;
}

/*
 * return: actually broken rows.
 */
u_int
ml_imgmdl_break_boundary(
	ml_image_model_t *  model ,
	u_int  size ,
	ml_char_t *  sp_ch
	)
{
	int  counter ;

	if( model->num_of_filled_rows + size > model->num_of_rows)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " breaking from line %d by size %d failed." ,
			model->num_of_filled_rows - 1 , size) ;
	#endif

		size = model->num_of_rows - model->num_of_filled_rows ;

	#ifdef  DEBUG
		kik_msg_printf( " ... size is modified -> %d\n" , size) ;
	#endif
	}

	if( size == 0)
	{
		/* nothing is done */
		
		return  0 ;
	}

	for( counter = model->num_of_filled_rows ; counter < model->num_of_filled_rows + size ; counter ++)
	{
		ml_imgline_clear( ml_imgmdl_get_line( model , counter) , 0 , sp_ch) ;
	}

	model->num_of_filled_rows += size ;

	return  size ;
}

/*
 * return: actually broken rows.
 */
u_int
ml_imgmdl_shrink_boundary(
	ml_image_model_t *  model ,
	u_int  size
	)
{
	int  counter ;

	for( counter = 1 ; counter <= size ; counter ++)
	{
		ml_imgline_reset( ml_imgmdl_get_line( model , model->num_of_filled_rows - counter)) ;
	}

	model->num_of_filled_rows -= size ;

	return  size ;
}

int
ml_imgmdl_scroll_upward(
	ml_image_model_t *  model ,
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
ml_imgmdl_scroll_downward(
	ml_image_model_t *  model ,
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
