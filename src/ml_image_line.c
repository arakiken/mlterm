/*
 *	$Id$
 */

#include  "ml_image_line.h"

#include  <string.h>		/* memset */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>

#include  "ml_iscii.h"


#ifdef  DEBUG
#define  END_CHAR_INDEX(line) \
	( (line)->num_of_filled_chars == 0 && \
		kik_warn_printf( "END_CHAR_INDEX()[" __FUNCTION__  "] num_of_filled_chars is 0.\n") ? \
		0 : (line)->num_of_filled_chars - 1 )
#else
#define  END_CHAR_INDEX(line)  ((line)->num_of_filled_chars - 1)
#endif

#define  IS_CLEARED_TO_END(flag) ((flag) & 0x1)
#define  SET_CLEARED_TO_END(flag)  ((flag) |= 0x1)
#define  UNSET_CLEARED_TO_END(flag)  ((flag) &= ~0x1)

#define  IS_MODIFIED(flag)  (((flag) >> 1) & 0x1)
#define  SET_MODIFIED(flag)  ((flag) |= 0x2)
#define  UNSET_MODIFIED(flag)  ((flag) &= ~0x2)

#define  IS_CONTINUED_TO_NEXT(flag)  (((flag) >> 2) & 0x1)
#define  SET_CONTINUED_TO_NEXT(flag)  ((flag) |= 0x4)
#define  UNSET_CONTINUED_TO_NEXT(flag)  ((flag) &= ~0x4)


/* --- global functions --- */

/*
 * functions which doesn't have to care about visual order.
 */
 
int
ml_imgline_init(
	ml_image_line_t *  line ,
	u_int  num_of_chars
	)
{
	memset( line , 0 , sizeof( ml_image_line_t)) ;

	if( ( line->chars = ml_str_new( num_of_chars)) == NULL)
	{
		return  0 ;
	}

	line->num_of_chars = num_of_chars ;

	return  1 ;
}

int
ml_imgline_clone(
	ml_image_line_t *  clone ,
	ml_image_line_t *  orig ,
	u_int  num_of_chars
	)
{
	ml_imgline_init( clone , num_of_chars) ;
	
	ml_imgline_copy_line( clone , orig) ;

	return  1 ;
}

int
ml_imgline_final(
	ml_image_line_t *  line
	)
{
	if( line->chars)
	{
		ml_str_delete( line->chars , line->num_of_chars) ;
	}

	/*
	 * just to be sure...
	 */
	if( line->visual_order)
	{
		free( line->visual_order) ;
	}

	return  1 ;
}

/*
 * return: actually broken rows.
 */
u_int
ml_imgline_break_boundary(
	ml_image_line_t *  line ,
	u_int  size ,
	ml_char_t *  sp_ch
	)
{
	int  counter ;

	if( line->num_of_filled_chars + size > line->num_of_chars)
	{
		/* over line length */

	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " breakin from col %d by size %d failed." ,
			END_CHAR_INDEX(line) , size) ;
	#endif

		size = line->num_of_chars - line->num_of_filled_chars ;

	#ifdef  DEBUG
		kik_msg_printf( " ... size modified -> %d\n" , size) ;
	#endif
	}

	if( size == 0)
	{
		/* nothing is done */
		
		return  0 ;
	}

	/* padding spaces */
	for( counter = line->num_of_filled_chars ; counter < line->num_of_filled_chars + size ;
		counter ++)
	{
		ml_char_copy( &line->chars[counter] , sp_ch) ;
	}

	line->num_of_filled_chars += size ;

	/*
	 * change char index is not updated , because space has no glyph.
	 */

	return  size ;
}

int
ml_imgline_reset(
	ml_image_line_t *  line
	)
{
	if( line->num_of_filled_chars > 0)
	{
		ml_imgline_set_modified( line , 0 , 0 , 1) ;
	}

	line->num_of_filled_chars = 0 ;
	line->num_of_filled_visual_order = 0 ;
	UNSET_CONTINUED_TO_NEXT(line->flag) ;

	return 1 ;
}

int
ml_imgline_clear(
	ml_image_line_t *  line ,
	int  char_index ,
	ml_char_t *  sp_ch
	)
{
	if( char_index == 0)
	{
		ml_imgline_reset( line) ;

		ml_char_copy( &line->chars[0] , sp_ch) ;
		line->num_of_filled_chars = 1 ;
	}
	else
	{
		if( char_index >= line->num_of_filled_chars)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " nothing is cleared.\n") ;
		#endif

			return  1 ;
		}

		ml_char_copy( &line->chars[char_index] , sp_ch) ;
		line->num_of_filled_chars = char_index + 1 ;

		ml_imgline_set_modified( line , char_index , END_CHAR_INDEX(line) , 1) ;
	}
	
	return  1 ;
}

int
ml_imgline_overwrite_chars(
	ml_image_line_t *  line ,
	int  change_char_index ,
	ml_char_t *  chars ,
	u_int  len ,
	u_int  cols ,
	ml_char_t *  sp_ch
	)
{
	int  counter ;
	int  char_index ;
	u_int  orig_filled_cols ;
	u_int  cols_rest ;
	u_int  padding ;
	u_int  new_len ;
	u_int  copy_len ;

	orig_filled_cols = ml_imgline_get_num_of_filled_cols( line) ;

	if( ml_imgline_is_empty( line) || cols >= orig_filled_cols)
	{
		return  ml_imgline_overwrite_all( line , change_char_index , chars , len , cols) ;
	}

	/* trying to delete from pos 0 to the pos just before this char_index */
	char_index = ml_convert_col_to_char_index( line , &cols_rest , cols , 0) ;

	if( char_index == END_CHAR_INDEX(line) && cols_rest >= 1)
	{
		return  ml_imgline_overwrite_all( line , change_char_index , chars , len , cols) ;
	}

	if( 1 <= cols_rest && cols_rest < ml_char_cols( &line->chars[char_index]))
	{
		padding = ml_char_cols( &line->chars[char_index]) - cols_rest ;
		char_index ++ ;
	}
	else
	{
		padding = 0 ;
	}

	copy_len = line->num_of_filled_chars - char_index ;
	new_len = len + padding + copy_len ;
	if( new_len > line->num_of_chars)
	{
	#ifdef DEBUG
		kik_warn_printf(
			KIK_DEBUG_TAG " line length %d(ow %d copy %d) after overwriting is overflowed\n" ,
			new_len , len , copy_len) ;

		if( line->num_of_chars < padding + copy_len)
		{
			kik_warn_printf( KIK_DEBUG_TAG
				" padding(%d) + copy_len(%d) is over max chars(%d).\n" ,
				padding , copy_len , line->num_of_chars) ;

			abort() ;
		}
	#endif
		
		new_len = line->num_of_chars ;

		len = new_len - padding - copy_len ;

	#ifdef  DEBUG
		kik_msg_printf( " ... modified -> new_len %d , ow len %d\n" , new_len , len) ;
	#endif
	}

	ml_str_copy( &line->chars[len + padding] , &line->chars[char_index] , copy_len) ;

	for( counter = 0 ; counter < padding ; counter ++)
	{
		ml_char_copy( &line->chars[len + counter] , sp_ch) ;
	}

	ml_str_copy( line->chars , chars , len) ;

	line->num_of_filled_chars = new_len ;

	if( orig_filled_cols > ml_imgline_get_num_of_filled_cols( line))
	{
		ml_imgline_set_modified( line , change_char_index ,
			K_MAX(change_char_index,END_CHAR_INDEX(line)) , 1) ;
	}
	else
	{
		ml_imgline_set_modified( line , change_char_index ,
			K_MAX(change_char_index,len - 1) , 0) ;
	}

	return  1 ;
}

int
ml_imgline_overwrite_all(
	ml_image_line_t *  line ,
	int  change_char_index ,
	ml_char_t *  chars ,
	int  len ,
	u_int  cols
	)
{
	u_int  orig_filled_cols ;

	orig_filled_cols = ml_imgline_get_num_of_filled_cols( line) ;
	
	ml_str_copy( line->chars , chars , len) ;
	line->num_of_filled_chars = len ;

	if( orig_filled_cols > cols)
	{
		ml_imgline_set_modified( line , change_char_index , END_CHAR_INDEX(line) , 1) ;
	}
	else
	{
		ml_imgline_set_modified( line , change_char_index , END_CHAR_INDEX(line) , 0) ;
	}

	return  1 ;
}

int
ml_imgline_fill_all(
	ml_image_line_t *  line ,
	ml_char_t *  ch ,
	u_int  num
	)
{
	int  char_index ;
	int  cols ;

	num = K_MIN(num,line->num_of_chars) ;

	cols = 0 ;
	for( char_index = 0 ; char_index < num ; char_index ++)
	{
		ml_char_copy( &line->chars[char_index] , ch) ;
		cols += ml_char_cols( ch) ;
	}

	line->num_of_filled_chars = char_index ;

	ml_imgline_set_modified_all( line) ;

	return  1 ;
}

void
ml_imgline_set_modified(
	ml_image_line_t *  line ,
	int  beg_char_index ,
	int  end_char_index ,
	int  is_cleared_to_end
	)
{
	if( beg_char_index > end_char_index)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " beg_char_index %d > end_char_index %d\n" ,
			beg_char_index , end_char_index) ;
	#endif
	
		return ;
	}

	if( IS_MODIFIED(line->flag))
	{
		if( beg_char_index < line->change_beg_char_index)
		{
			line->change_beg_char_index = beg_char_index ;
		}

		if( end_char_index > line->change_end_char_index)
		{
			line->change_end_char_index = end_char_index ;
		}

		if( is_cleared_to_end)
		{
			SET_CLEARED_TO_END(line->flag) ;
		}
	}
	else
	{
		line->change_beg_char_index = beg_char_index ;
		line->change_end_char_index = end_char_index ;

		SET_MODIFIED(line->flag) ;
		if( is_cleared_to_end)
		{
			SET_CLEARED_TO_END(line->flag) ;
		}
		else
		{
			UNSET_CLEARED_TO_END(line->flag) ;
		}
	}
}

void
ml_imgline_set_modified_all(
	ml_image_line_t *  line
	)
{
	if( ml_imgline_is_empty( line))
	{
		ml_imgline_set_modified( line , 0 , 0 , 1) ;
	}
	else
	{
		ml_imgline_set_modified( line , 0 , END_CHAR_INDEX(line) , 1) ;
	}
}

int
ml_imgline_is_cleared_to_end(
	ml_image_line_t *  line
	)
{
	return  IS_CLEARED_TO_END(line->flag) ;
}

int
ml_imgline_is_modified(
	ml_image_line_t *  line
	)
{
	return  IS_MODIFIED(line->flag) ;
}

void
ml_imgline_is_updated(
	ml_image_line_t *  line
	)
{
	UNSET_MODIFIED(line->flag) ;
	UNSET_CLEARED_TO_END(line->flag) ;
}

int
ml_imgline_is_continued_to_next(
	ml_image_line_t *  line
	)
{
	return  IS_CONTINUED_TO_NEXT(line->flag) ;
}

void
ml_imgline_set_continued_to_next(
	ml_image_line_t *  line
	)
{
	SET_CONTINUED_TO_NEXT(line->flag) ;
}

void
ml_imgline_unset_continued_to_next(
	ml_image_line_t *  line
	)
{
	UNSET_CONTINUED_TO_NEXT(line->flag) ;
}

u_int
ml_imgline_get_num_of_filled_cols(
	ml_image_line_t *  line
	)
{
	return  ml_str_cols( line->chars , line->num_of_filled_chars) ;
}

int
ml_imgline_get_beg_of_modified(
	ml_image_line_t *  line
	)
{
	if( ml_imgline_is_empty( line))
	{
		return  0 ;
	}
	else
	{
		return  K_MIN(line->change_beg_char_index,END_CHAR_INDEX(line)) ;
	}
}

u_int
ml_imgline_get_num_of_redrawn_chars(
	ml_image_line_t *  line
	)
{
	if( ml_imgline_is_empty( line))
	{
		return  0 ;
	}
	else
	{
		return  K_MIN(line->change_end_char_index,END_CHAR_INDEX(line)) -
			K_MIN(line->change_beg_char_index,END_CHAR_INDEX(line)) + 1 ;
	}
}

int
ml_convert_char_index_to_col(
	ml_image_line_t *  line ,
	int  char_index ,
	int  flag		/* BREAK_BOUNDARY */
	)
{
	int  counter ;
	int  col ;

	if( line->num_of_filled_chars == 0)
	{
		return  0 ;
	}

#ifdef  DEBUG
	if( char_index >= line->num_of_chars)
	{
		/* this must never happen */
		
		kik_warn_printf( KIK_DEBUG_TAG
			" char index %d is larger than num_of_chars(%d)\n" ,
			char_index , line->num_of_chars) ;
			
		abort() ;
	}
#endif
	
	col = 0 ;

	if( (flag & BREAK_BOUNDARY) && line->num_of_filled_chars <= char_index)
	{
		for( counter = 0 ; counter < line->num_of_filled_chars ; counter ++)
		{
		#ifdef  DEBUG
			if( ml_char_cols( &line->chars[counter]) == 0)
			{
				kik_warn_printf( KIK_DEBUG_TAG " ml_char_cols returns 0.\n") ;
			
				continue ;
			}
		#endif
		
			col += ml_char_cols( &line->chars[counter]) ;
		}

		col += (char_index - counter) ;
	}
	else
	{
		/*
		 * excluding the width of the last char.
		 */
		for( counter = 0 ; counter < K_MIN(char_index,END_CHAR_INDEX(line)) ; counter ++)
		{
		#ifdef  DEBUG
			if( ml_char_cols( &line->chars[counter]) == 0)
			{
				kik_warn_printf( KIK_DEBUG_TAG " ml_char_cols returns 0.\n") ;
				
				continue ;
			}
		#endif

			col += ml_char_cols( &line->chars[counter]) ;
		}
	}

	return  col ;
}

int
ml_convert_col_to_char_index(
	ml_image_line_t *  line ,
	u_int *  cols_rest ,
	int  col ,
	int  flag	/* BREAK_BOUNDARY */
	)
{
	int  char_index ;

	if( line->num_of_filled_chars == 0)
	{
		return  0 ;
	}

#ifdef  DEBUG
	if( col >= line->num_of_chars)
	{
		/* this must never happen */
		
		kik_warn_printf( KIK_DEBUG_TAG " col %d is larger than num_of_chars(%d)" ,
			col , line->num_of_chars) ;

		abort() ;
	}
#endif

	char_index = 0 ;
	
	for( ; char_index < END_CHAR_INDEX(line) ; char_index ++)
	{
		if( col < ml_char_cols( &line->chars[char_index]))
		{
			goto  end ;
		}

		col -= ml_char_cols( &line->chars[char_index]) ;
	}
	
	if( col < ml_char_cols( &line->chars[char_index]))
	{
		goto  end ;
	}
	else if( flag & BREAK_BOUNDARY)
	{
		col -= ml_char_cols( &line->chars[char_index++]) ;
		
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

int
ml_convert_char_index_to_x(
	ml_image_line_t *  line ,
	int  char_index ,
	ml_shape_t *  shape
	)
{
	int  counter ;
	int  x ;
	ml_image_line_t *  orig ;

	if( char_index > END_CHAR_INDEX(line))
	{
		char_index = END_CHAR_INDEX(line) ;
	}

	if( shape)
	{
		if( ( orig = ml_imgline_shape( line , shape)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_imgline_shape() failed.\n") ;
		#endif
		}
	}
	else
	{
		orig = NULL ;
	}

	/*
	 * excluding the last char width.
	 */
	x = 0 ;
	for( counter = 0 ; counter < char_index ; counter ++)
	{
		x += ml_char_width( &line->chars[counter]) ;
	}
	
	if( orig)
	{
		ml_imgline_unshape( line , orig) ;
	}

	return  x ;
}

int
ml_convert_x_to_char_index(
	ml_image_line_t *  line ,
	u_int *  x_rest ,
	int  x ,
	ml_shape_t *  shape
	)
{
	int  counter ;
	ml_image_line_t *  orig ;
		
	if( shape)
	{
		if( ( orig = ml_imgline_shape( line , shape)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_imgline_shape() failed.\n") ;
		#endif
		}
	}
	else
	{
		orig = NULL ;
	}
	
	for( counter = 0 ; counter < END_CHAR_INDEX(line) ; counter ++)
	{
		if( x < ml_char_width( &line->chars[counter]))
		{
			break ;
		}

		x -= ml_char_width( &line->chars[counter]) ;
	}

	if( x_rest != NULL)
	{
		*x_rest = x ;
	}

	if( orig)
	{
		ml_imgline_unshape( line , orig) ;
	}
	
	return  counter ;
}

int
ml_imgline_reverse_color(
	ml_image_line_t *  line ,
	int  char_index
	)
{
	if( char_index > END_CHAR_INDEX(line))
	{
		return  0 ;
	}

	ml_char_reverse_color( &line->chars[char_index]) ;

	ml_imgline_set_modified( line , char_index , char_index , 0) ;

	return  1 ;
}

int
ml_imgline_restore_color(
	ml_image_line_t *  line ,
	int  char_index
	)
{
	if( char_index > END_CHAR_INDEX(line))
	{
		return  0 ;
	}

	ml_char_restore_color( &line->chars[char_index]) ;

	ml_imgline_set_modified( line , char_index , char_index , 0) ;

	return  1 ;
}

/*
 * This copys a line as it is and doesn't care about visual order.
 * But bidi parameters are also copyed as it is.
 */
int
ml_imgline_copy_line(
	ml_image_line_t *  dst ,
	ml_image_line_t *  src
	)
{
	u_int  copy_len ;

	copy_len = K_MIN(src->num_of_filled_chars,dst->num_of_chars) ;

	ml_str_copy( dst->chars , src->chars , copy_len) ;
	dst->num_of_filled_chars = copy_len ;

	dst->change_beg_char_index = K_MIN(src->change_beg_char_index,dst->num_of_chars) ;
	dst->change_end_char_index = K_MIN(src->change_end_char_index,dst->num_of_chars) ;
	
	dst->flag = src->flag ;

	/*
	 * bidi parameters.
	 */
	if( ml_imgline_is_using_bidi( src))
	{
		ml_imgline_use_bidi( dst) ;

		if( src->num_of_filled_visual_order > 0)
		{
			if( src->num_of_filled_chars == dst->num_of_filled_chars)
			{
				memcpy( dst->visual_order , src->visual_order ,
					sizeof( u_int16_t) * src->num_of_filled_visual_order) ;
				dst->num_of_filled_visual_order = src->num_of_filled_visual_order ;
			}
			else
			{
				SET_MODIFIED(dst->flag) ;
			}
		}
	}
	else
	{
		dst->num_of_filled_visual_order = 0 ;
	}

	return  1 ;
}

int
ml_imgline_share(
	ml_image_line_t *  dst ,
	ml_image_line_t *  src
	)
{
	dst->chars = src->chars ;
	dst->num_of_filled_chars = src->num_of_filled_chars ;
	
	dst->visual_order = src->visual_order ;
	dst->num_of_filled_visual_order = src->num_of_filled_visual_order ;

	dst->change_beg_char_index = src->change_beg_char_index ;
	dst->change_end_char_index = src->change_end_char_index ;
	dst->flag = src->flag ;

	return  1 ;
}

int
ml_imgline_end_char_index(
	ml_image_line_t *  line
	)
{
	if( line->num_of_filled_chars == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " num_of_filled_chars is 0.\n") ;
	#endif
	
		return  0 ;
	}
	else
	{
		return  line->num_of_filled_chars - 1 ;
	}
}

int
ml_imgline_is_empty(
	ml_image_line_t *  line
	)
{
	return  (line->num_of_filled_chars == 0) ;
}

u_int
ml_get_num_of_filled_chars_except_end_space(
	ml_image_line_t *  line
	)
{
	ml_char_t *  ch ;
	int  char_index ;

	for( char_index = END_CHAR_INDEX(line) ; char_index >= 0 ; char_index --)
	{
		ch = &line->chars[char_index] ;
		
		if( ml_char_size( ch) != 1 || ml_char_bytes( ch)[0] != ' ')
		{
			return  char_index + 1 ;
		}
	}

	return  0 ;
}


/*
 * visual <=> logical functions , which must care about visual order.
 */

int
ml_imgline_is_using_bidi(
	ml_image_line_t *  line
	)
{
	if( line->visual_order)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
ml_imgline_use_bidi(
	ml_image_line_t *  line
	)
{
	if( line->visual_order)
	{
		return  1 ;
	}

	if( ( line->visual_order = malloc( sizeof( u_int16_t) * line->num_of_chars)) == NULL)
	{
		return  0 ;
	}

	line->num_of_filled_visual_order = 0 ;

	return  1 ;
}

int
ml_imgline_unuse_bidi(
        ml_image_line_t *  line
        )
{
	free( line->visual_order) ;
	line->visual_order = NULL ;
	line->num_of_filled_visual_order = 0 ;

	return  1 ;
}

int
ml_imgline_bidi_render(
	ml_image_line_t *  line ,
	int  base_dir_is_rtl
	)
{
	u_int  len ;

	if( ! line->visual_order)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " render failed. visual_order is NULL\n") ;
	#endif

		return  0 ;
	}
#if  0
	else if( line->num_of_filled_chars == 0 ||
		(len = ml_get_num_of_filled_chars_except_end_space( line)) == 0)
	{
		return  1 ;
	}
#else
	else if( ( len = line->num_of_filled_chars) == 0)
	{
		return  1 ;
	}
#endif

	if( ! ml_bidi( line->visual_order , line->chars , len , base_dir_is_rtl))
	{
		line->num_of_filled_visual_order = 0 ;
		
		return  0 ;
	}

	line->num_of_filled_visual_order = len ;
	
	return  1 ;
}

int
ml_imgline_bidi_visual(
	ml_image_line_t *  line
	)
{
	int  counter ;
	ml_char_t *  src ;
	
	if( ! line->visual_order)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " visual_order is NULL\n") ;
	#endif

		return  0 ;
	}

	if( line->num_of_filled_visual_order == 0)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " not rendered yet.\n") ;
	#endif
	
		return  0 ;
	}

	if( ( src = ml_str_alloca( line->num_of_filled_visual_order)) == NULL)
	{
		return  0 ;
	}

	ml_str_copy( src , line->chars , line->num_of_filled_visual_order) ;

	for( counter = 0 ; counter < line->num_of_filled_visual_order ; counter ++)
	{
	#ifdef  DEBUG
		if( line->visual_order[counter] >= line->num_of_filled_visual_order)
		{
			kik_warn_printf( KIK_DEBUG_TAG
				" visual order(%d) of %d is illegal.\n" ,
				line->visual_order[counter] , counter) ;
				
			abort() ;
		}
	#endif

		ml_char_copy( &line->chars[line->visual_order[counter]] , &src[counter]) ;
	}

	ml_str_final( src , line->num_of_filled_visual_order) ;

	if( IS_MODIFIED(line->flag))
	{
		ml_imgline_set_modified( line , 0 , END_CHAR_INDEX(line) , IS_CLEARED_TO_END(line->flag)) ;
	}

	return  1 ;
}

int
ml_imgline_bidi_logical(
	ml_image_line_t *  line
	)
{
	int  counter ;
	ml_char_t *  src ;
	
	if( ! line->visual_order)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " visual_order is NULL\n") ;
	#endif

		return  0 ;
	}

	if( line->num_of_filled_visual_order == 0)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " not rendered yet.\n") ;
	#endif
	
		return  0 ;
	}

	if( ( src = ml_str_alloca( line->num_of_filled_visual_order)) == NULL)
	{
		return  0 ;
	}
	
	ml_str_copy( src , line->chars , line->num_of_filled_visual_order) ;

	for( counter = 0 ; counter < line->num_of_filled_visual_order ; counter ++)
	{
	#ifdef  DEBUG
		if( line->visual_order[counter] >= line->num_of_filled_visual_order)
		{
			kik_warn_printf( KIK_DEBUG_TAG
				" visual order(%d) of %d is illegal.\n" ,
				line->visual_order[counter] , counter) ;

			abort() ;
		}
	#endif
		
		ml_char_copy( &line->chars[counter] , &src[line->visual_order[counter]]) ;
	}

	ml_str_final( src , line->num_of_filled_visual_order) ;

	/*
	 * !! Notice !!
	 * is_modified is as it is , which should not be touched here.
	 */

	return  1 ;
}

int
ml_bidi_convert_logical_char_index_to_visual(
	ml_image_line_t *  line ,
	int  char_index
	)
{
	if( ! line->visual_order)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " visual_order is NULL\n") ;
	#endif
	
		return  char_index ;
	}
	
	if( 0 <= char_index && char_index < line->num_of_filled_visual_order)
	{
		return  line->visual_order[char_index] ;
	}
	else
	{
		return  char_index ;
	}
}

int
ml_bidi_convert_visual_char_index_to_logical(
	ml_image_line_t *  line ,
	int  char_index
	)
{
	u_int  counter ;

	if( ! line->visual_order)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " visual_order is NULL\n") ;
	#endif
	
		return  char_index ;
	}
	
	for( counter = 0 ; counter < line->num_of_filled_visual_order ; counter ++)
	{
		if( char_index == line->visual_order[counter])
		{
			return  counter ;
		}
	}

	return  char_index ;
}

int
ml_imgline_copy_str(
	ml_image_line_t *  line ,
	ml_char_t *  dst ,
	int  beg ,
	u_int  len
	)
{
	if( line->num_of_filled_visual_order == 0)
	{
		return  ml_str_copy( dst , &line->chars[beg] , len) ;
	}
	else
	{
		/*
		 * XXX  adhoc !
		 */
		 
		int *  flags ;
		int  bidi_pos ;
		int  norm_pos ;
		int  dst_pos ;

		if( ( flags = alloca( sizeof( int) * line->num_of_filled_visual_order)) == NULL)
		{
			return  0 ;
		}

		memset( flags , 0 , sizeof( int) * line->num_of_filled_visual_order) ;

		for( bidi_pos = beg ; bidi_pos < beg + len ; bidi_pos ++)
		{
			for( norm_pos = 0 ; norm_pos < line->num_of_filled_visual_order ; norm_pos ++)
			{
				if( line->visual_order[norm_pos] == bidi_pos)
				{
					flags[norm_pos] = 1 ;
				}
			}
		}

		for( dst_pos = norm_pos = 0 ; norm_pos < line->num_of_filled_visual_order ; norm_pos ++)
		{
			if( flags[norm_pos])
			{
				ml_char_copy( &dst[dst_pos ++] ,
					&line->chars[line->visual_order[norm_pos]]) ;
			}
		}

		return  1 ;
	}
}


int
ml_imgline_iscii_visual(
	ml_image_line_t *  line ,
	ml_iscii_state_t  iscii_state
	)
{
	ml_char_t *  src ;
	u_int  src_len ;
	ml_char_t *  dst ;
	u_int  dst_len ;
	int  dst_pos ;
	int  src_pos ;
	ml_char_t *  ch ;
	u_char *  iscii_buf ;
	u_int  iscii_buf_len ;
	u_char *  font_buf ;
	u_int  font_buf_len ;
	u_int  prev_font_filled ;
	u_int  iscii_filled ;
	int  counter ;

	/*
	 * char combining is necessary for rendering ISCII glyphs
	 */
	if( ! ml_is_char_combining())
	{
		return  0 ;
	}

	iscii_buf_len = line->num_of_filled_chars * 4 + 1 ;
	if( ( iscii_buf = alloca( iscii_buf_len)) == NULL)
	{
		return  0 ;
	}

	font_buf_len = line->num_of_chars * 4 + 1 ;
	if( ( font_buf = alloca( font_buf_len)) == NULL)
	{
		return  0 ;
	}

	src_len = line->num_of_filled_chars ;
	if( ( src = ml_str_alloca( src_len)) == NULL)
	{
		return  0 ;
	}
	
	ml_str_copy( src , line->chars , line->num_of_filled_chars) ;

	dst = line->chars ;
	dst_len = line->num_of_chars ;
	
	dst_pos = -1 ;
	prev_font_filled = 0 ;
	iscii_filled = 0 ;
	for( src_pos = 0 ; src_pos < src_len ; src_pos ++)
	{
		ch = &src[src_pos] ;
		
		if( ml_font_cs( ml_char_font( ch)) == ISCII)
		{
			u_int  font_filled ;
			
			iscii_buf[iscii_filled ++] = ml_char_bytes( ch)[0] ;
			iscii_buf[iscii_filled] = '\0' ;
			font_filled = ml_iscii_shape( iscii_state , font_buf , font_buf_len , iscii_buf) ;

			if( font_filled < prev_font_filled)
			{
				ml_char_t *  c ;
				ml_char_t *  comb ;
				u_int  comb_size ;

				if( prev_font_filled - font_filled > dst_pos)
				{
					font_filled = prev_font_filled - dst_pos ;
				}
				
				dst_pos -= (prev_font_filled - font_filled) ;
				
				for( counter = 1 ; counter <= prev_font_filled - font_filled ; counter ++)
				{
					int  comb_pos ;
					
					c = &dst[dst_pos + counter] ;

					if( ml_char_is_null( c))
					{
						continue ;
					}
					
					comb = ml_get_combining_chars( c , &comb_size) ;

					comb_pos = 0 ;
					while( 1)
					{
						if( ml_char_is_null( &dst[dst_pos]))
						{
							/*
							 * combining is forbidden if base character is null
							 */
							ml_char_copy( &dst[dst_pos] , c) ;
						}
						else if( ! ml_char_combine( &dst[dst_pos] ,
							ml_char_bytes( c) , ml_char_size( c) ,
							ml_char_font( c) , ml_char_font_decor( c) ,
							ml_char_fg_color( c) , ml_char_bg_color( c)))
						{
						#ifdef  DEBUG
							kik_warn_printf( KIK_DEBUG_TAG
								" combining failed.\n") ;
						#endif
						
							break ;
						}

						if( comb_pos >= comb_size)
						{
							break ;
						}

						c = &comb[comb_pos++] ;
					}
				}

				prev_font_filled = font_filled ;
			}

			if( dst_pos >= 0 && font_filled == prev_font_filled)
			{
				if( ml_char_is_null( &dst[dst_pos]))
				{
					/*
					 * combining is forbidden if base character is null
					 */
					ml_char_copy( &dst[dst_pos] , ch) ;
				}
				else if( ! ml_char_combine( &dst[dst_pos] ,
					ml_char_bytes( ch) , ml_char_size( ch) ,
					ml_char_font( ch) , ml_char_font_decor( ch) ,
					ml_char_fg_color( ch) , ml_char_bg_color( ch)))
				{
				#ifdef  DEBUG
					kik_warn_printf( KIK_DEBUG_TAG
						" combining failed.\n") ;
				#endif
				}
			}
			else
			{
				if( ++ dst_pos >= dst_len)
				{
					goto  end ;
				}

				ml_char_copy( &dst[dst_pos] , ch) ;
				
				if( font_filled > prev_font_filled + 1)
				{
					for( counter = 0 ; counter < font_filled - prev_font_filled - 1 ;
						counter ++)
					{
						if( ++ dst_pos >= dst_len)
						{
							goto  end ;
						}

						ml_char_copy( &dst[dst_pos] , ch) ;
						
						/* NULL */
						ml_char_set_bytes( &dst[dst_pos] , "\x0" , 1) ;
					}
				}
			}

			prev_font_filled = font_filled ;
		}
		else
		{
			if( ++ dst_pos >= dst_len)
			{
				goto  end ;
			}
			
			ml_char_copy( &dst[dst_pos] , ch) ;

			prev_font_filled = 0 ;
			iscii_filled = 0 ;
		}
	}

	dst_pos ++ ;

end:
	ml_str_final( src , src_len) ;

	line->num_of_filled_chars = dst_pos ;

	if( IS_MODIFIED(line->flag))
	{
		ml_imgline_set_modified_all( line) ;
	}

	return  1 ;
}

/*
 * this should be called after ml_imgline_iscii_visaul()
 */
int
ml_iscii_convert_logical_char_index_to_visual(
	ml_image_line_t *  line ,
	int  logical_char_index
	)
{
	int  counter ;
	int  visual_char_index ;
	u_int  comb_size ;

	/*
	 * char combining is necessary for rendering ISCII glyphs
	 */
	if( ! ml_is_char_combining())
	{
		return  logical_char_index ;
	}
	
	visual_char_index = 0 ;

	for( counter = 0 ; counter < logical_char_index && counter < END_CHAR_INDEX(line) ; counter ++)
	{
		if( ml_font_cs( ml_char_font( &line->chars[counter])) == ISCII)
		{
			/*
			 * skipping trailing null characters of previous character.
			 */
			while( ml_char_is_null( &line->chars[counter]))
			{
				if( ++ counter >= line->num_of_filled_chars)
				{
					goto  end ;
				}

				logical_char_index ++ ;
				visual_char_index ++ ;
			}

			if( ml_get_combining_chars( &line->chars[counter] , &comb_size))
			{
				logical_char_index -= comb_size ;
			}
		}
		
		visual_char_index ++ ;
	}

	while( ml_char_is_null( &line->chars[counter]))
	{
		if( ++ counter >= line->num_of_filled_chars)
		{
			break ;
		}
		
		visual_char_index ++ ;
	}

end:
	if( visual_char_index >= line->num_of_filled_chars)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" visual_char_index %d (of logical %d) is over num_of_filled_chars %d\n" ,
			visual_char_index , logical_char_index , line->num_of_filled_chars) ;
	#endif

	#ifdef  __DEBUG
		/*
		 * XXX
		 * This must never happens that abort() should be done here.
		 * But this happens not a few times ....
		 */
		abort() ;
	#endif
	
		visual_char_index = END_CHAR_INDEX(line) ;
	}

	return  visual_char_index ;
}

ml_image_line_t *
ml_imgline_shape(
	ml_image_line_t *  line ,
	ml_shape_t *  shape
	)
{
	ml_image_line_t *  orig ;
	ml_char_t *  shaped ;

	if( ( orig = malloc( sizeof( ml_image_line_t))) == NULL)
	{
		return  NULL ;
	}

	ml_imgline_share( orig , line) ;
	
	if( ( shaped = ml_str_new( line->num_of_chars)) == NULL)
	{
		return  NULL ;
	}

	line->num_of_filled_chars = (*shape->shape)( shape , shaped , line->num_of_chars ,
					line->chars , line->num_of_filled_chars) ;
	line->chars = shaped ;

	return  orig ;
}

int
ml_imgline_unshape(
	ml_image_line_t *  line ,
	ml_image_line_t *  orig
	)
{
	ml_str_delete( line->chars , line->num_of_chars) ;
	
	line->chars = orig->chars ;
	line->num_of_filled_chars = orig->num_of_filled_chars ;

	free( orig) ;

	return  1 ;
}
