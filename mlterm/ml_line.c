/*
 *	$Id$
 */

#include  "ml_line.h"

#include  <string.h>		/* memset */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>	/* K_MIN */

#include  "ml_iscii.h"


#ifdef  DEBUG
#define  END_CHAR_INDEX(line) \
	( (line)->num_of_filled_chars == 0 && \
		kik_warn_printf( "END_CHAR_INDEX()[" __FUNCTION__  "] num_of_filled_chars is 0.\n") ? \
		0 : (line)->num_of_filled_chars - 1 )
#else
#define  END_CHAR_INDEX(line) \
	( (line)->num_of_filled_chars == 0 ? 0 : (line)->num_of_filled_chars - 1 )
#endif

#define  IS_MODIFIED(flag)  ((flag) & 0x1)
#define  SET_MODIFIED(flag)  ((flag) |= 0x1)
#define  UNSET_MODIFIED(flag)  ((flag) &= ~0x1)

#define  IS_CONTINUED_TO_NEXT(flag)  (((flag) >> 1) & 0x1)
#define  SET_CONTINUED_TO_NEXT(flag)  ((flag) |= 0x2)
#define  UNSET_CONTINUED_TO_NEXT(flag)  ((flag) &= ~0x2)

#define  IS_EMPTY(line)  ((line)->num_of_filled_chars == 0)


#if  0
#define  __DEBUG
#endif


/* --- global functions --- */

/*
 * Functions which doesn't have to care about visual order.
 */
 
int
ml_line_init(
	ml_line_t *  line ,
	u_int  num_of_chars
	)
{
	memset( line , 0 , sizeof( ml_line_t)) ;

	if( ( line->chars = ml_str_new( num_of_chars)) == NULL)
	{
		return  0 ;
	}

	line->num_of_chars = num_of_chars ;

	return  1 ;
}

int
ml_line_clone(
	ml_line_t *  clone ,
	ml_line_t *  orig ,
	u_int  num_of_chars
	)
{
	ml_line_init( clone , num_of_chars) ;
	ml_line_copy_line( clone , orig) ;

	return  1 ;
}

int
ml_line_final(
	ml_line_t *  line
	)
{
	if( line->bidi_state)
	{
		ml_line_unuse_bidi( line) ;
	}

	if( line->chars)
	{
		ml_str_delete( line->chars , line->num_of_chars) ;
	}
	
	return  1 ;
}

/*
 * return: actually broken chars.
 */
u_int
ml_line_break_boundary(
	ml_line_t *  line ,
	u_int  size
	)
{
	int  count ;

	if( line->num_of_filled_chars + size > line->num_of_chars)
	{
		/* over line length */

	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " breaking from col %d by size %d failed." ,
			line->num_of_filled_chars , size) ;
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
	for( count = line->num_of_filled_chars ; count < line->num_of_filled_chars + size ; count ++)
	{
		ml_char_copy( &line->chars[count] , ml_sp_ch()) ;
	}

	/*
	 * change char index is not updated , because space has no glyph.
	 */
#if  0
	ml_line_set_modified( line , END_CHAR_INDEX(line) + 1 , END_CHAR_INDEX(line) + size) ;
#endif

	line->num_of_filled_chars += size ;

	return  size ;
}

int
ml_line_assure_boundary(
	ml_line_t *  line ,
	int  char_index
	)
{
	if( char_index >= line->num_of_filled_chars)
	{
		u_int  brk_size ;

		brk_size = char_index - line->num_of_filled_chars + 1 ;
		
		if( ml_line_break_boundary( line , brk_size) < brk_size)
		{
			return  0 ;
		}
	}

	return  1 ;
}

int
ml_line_reset(
	ml_line_t *  line
	)
{
	if( IS_EMPTY(line))
	{
		/* already reset */
		
		return  1 ;
	}
	
	ml_line_set_modified( line , 0 , END_CHAR_INDEX(line)) ;
	
	line->num_of_filled_chars = 0 ;

	if( line->bidi_state)
	{
		ml_bidi_reset( line->bidi_state) ;
	}

	UNSET_CONTINUED_TO_NEXT(line->flag) ;
	
	return 1 ;
}

int
ml_line_clear(
	ml_line_t *  line ,
	int  char_index
	)
{
	if( char_index < line->num_of_filled_chars)
	{
		ml_line_set_modified( line , char_index , END_CHAR_INDEX(line)) ;

		ml_char_copy( &line->chars[char_index] , ml_sp_ch()) ;
		line->num_of_filled_chars = char_index + 1 ;
	}
	
	return  1 ;
}

int
ml_line_overwrite(
	ml_line_t *  line ,
	int  beg_char_index ,
	ml_char_t *  chars ,
	u_int  len ,
	u_int  cols
	)
{
	int  count ;
	u_int  cols_to_beg ;
	u_int  cols_rest ;
	u_int  padding ;
	u_int  new_len ;
	u_int  copy_len ;
	ml_char_t *  copy_src ;

	if( beg_char_index > line->num_of_filled_chars)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " beg_char_index[%d] is over filled region.\n" ,
			beg_char_index) ;
	#endif
	
		beg_char_index = line->num_of_filled_chars ;
	}
	
	if( beg_char_index + len > line->num_of_chars)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " beg_char_index[%d] + len[%d] is over line capacity.\n" ,
			beg_char_index , len) ;
	#endif
	
		len = line->num_of_chars - beg_char_index ;
	}

	cols_to_beg = ml_str_cols( line->chars , beg_char_index) ;

	if( cols_to_beg + cols < line->num_of_chars)
	{
		int  char_index ;
		
		char_index = ml_convert_col_to_char_index( line , &cols_rest , cols_to_beg + cols , 0) ;

		if( 1 <= cols_rest && cols_rest < ml_char_cols( &line->chars[char_index]))
		{
			padding = ml_char_cols( &line->chars[char_index]) - cols_rest ;
			char_index ++ ;
		}
		else
		{
			padding = 0 ;
		}

		if( line->num_of_filled_chars > char_index)
		{
			copy_len = line->num_of_filled_chars - char_index ;
		}
		else
		{
			copy_len = 0 ;
		}

		copy_src = &line->chars[char_index] ;
	}
	else
	{
		padding = 0 ;
		copy_len = 0 ;
		copy_src = NULL ;
	}
	
	new_len = beg_char_index + len + padding + copy_len ;
	
	if( new_len > line->num_of_chars)
	{
	#ifdef  DEBUG
		kik_warn_printf(
			KIK_DEBUG_TAG " new line len %d(beg %d ow %d padding %d copy %d) is overflowed\n" ,
			new_len , beg_char_index , len , padding , copy_len) ;
	#endif
		
		new_len = line->num_of_chars ;

		if( new_len > padding + beg_char_index + len)
		{
			copy_len = new_len - padding - beg_char_index - len ;
		}
		else
		{
			copy_len = 0 ;
			padding = new_len - beg_char_index - len ;
		}

	#ifdef  DEBUG
		kik_msg_printf( " ... modified -> new_len %d , copy_len %d\n" , new_len , copy_len) ;
	#endif
	}

	if( copy_len > 0)
	{
		/* making space */
		ml_str_copy( &line->chars[beg_char_index + len + padding] , copy_src , copy_len) ;
	}

	for( count = 0 ; count < padding ; count ++)
	{
		ml_char_copy( &line->chars[beg_char_index + len + count] , ml_sp_ch()) ;
	}

	ml_str_copy( &line->chars[beg_char_index] , chars , len) ;

	line->num_of_filled_chars = new_len ;

	ml_line_set_modified( line , beg_char_index , beg_char_index + len + padding - 1) ;

	return  1 ;
}

int
ml_line_overwrite_all(
	ml_line_t *  line ,
	ml_char_t *  chars ,
	int  len
	)
{
	ml_line_set_modified( line , 0 , END_CHAR_INDEX(line)) ;

	ml_str_copy( line->chars , chars , len) ;
	line->num_of_filled_chars = len ;

	ml_line_set_modified( line , 0 , END_CHAR_INDEX(line)) ;

	return  1 ;
}

int
ml_line_fill(
	ml_line_t *  line ,
	ml_char_t *  ch ,
	int  beg ,
	u_int  num
	)
{
	int  count ;
	int  char_index ;
	u_int  left_cols ;
	u_int  copy_len ;

	if( beg > line->num_of_filled_chars || beg >= line->num_of_chars)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " illegal beg[%d].\n" , beg) ;
	#endif
	
		return  0 ;
	}

	num = K_MIN(num,line->num_of_chars - beg) ;

	char_index = beg ;
	left_cols = num * ml_char_cols( ch) ;

	while( 1)
	{
		if( char_index >= line->num_of_filled_chars)
		{
			left_cols = 0 ;
			copy_len = 0 ;
			
			break ;
		}
		else if( left_cols < ml_char_cols( &line->chars[char_index]))
		{
			if( beg + num + left_cols > line->num_of_chars)
			{
				left_cols = line->num_of_chars - beg - num ;
				copy_len = 0 ;
			}
			else
			{
				copy_len = line->num_of_filled_chars - char_index - left_cols ;

				if( beg + num + left_cols + copy_len > line->num_of_chars)
				{
					/*
					 * line->num_of_chars is equal to or larger than
					 * beg + num + left_cols since
					 * 'if( beg + num + left_cols > line->num_of_chars)'
					 * is already passed here.
					 */
					copy_len = line->num_of_chars - beg - num - left_cols ;
				}
			}
			
			char_index += (left_cols / ml_char_cols(ch)) ;
			
			break ;
		}
		else
		{
			left_cols -= ml_char_cols( &line->chars[char_index]) ;
			char_index ++ ;
		}
	}

	if( copy_len > 0)
	{
		/* making space */
		ml_str_copy( &line->chars[beg + num + left_cols] , &line->chars[char_index] , copy_len) ;
	}

	char_index = beg ;
	
	for( count = 0 ; count < num ; count ++)
	{
		ml_char_copy( &line->chars[char_index++] , ch) ;
	}

	/* padding */
	for( count = 0 ; count < left_cols ; count ++)
	{
		ml_char_copy( &line->chars[char_index++] , ml_sp_ch()) ;
	}
	
	line->num_of_filled_chars = char_index + copy_len ;

	ml_line_set_modified( line , beg , beg + num + left_cols) ;

	return  1 ;
}

ml_char_t *
ml_line_get_char(
	ml_line_t *  line ,
	int  char_index
	)
{
	if( char_index >= line->num_of_filled_chars)
	{
		return  NULL ;
	}
	else
	{
		return  &line->chars[char_index] ;
	}
}

int
ml_line_set_modified(
	ml_line_t *  line ,
	int  beg_char_index ,
	int  end_char_index
	)
{
	int  count ;
	int  beg_col ;
	int  end_col ;
	
	if( beg_char_index > end_char_index)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " beg_char_index %d > end_char_index %d\n" ,
			beg_char_index , end_char_index) ;
	#endif

		return  0 ;
	}

	if( beg_char_index >= line->num_of_filled_chars)
	{
		beg_char_index = END_CHAR_INDEX(line) ;
	}
	
	if( end_char_index >= line->num_of_filled_chars)
	{
		end_char_index = END_CHAR_INDEX(line) ;
	}

	beg_col = 0 ;
	for( count = 0 ; count < beg_char_index ; count ++)
	{
		beg_col += ml_char_cols( &line->chars[count]) ;
	}

	end_col = beg_col ;
	for( ; count < end_char_index ; count ++)
	{
		end_col += ml_char_cols( &line->chars[count]) ;
	}

	if( IS_MODIFIED(line->flag))
	{
		if( beg_col < line->change_beg_col)
		{
			line->change_beg_col = beg_col ;
		}

		if( end_col > line->change_end_col)
		{
			line->change_end_col = end_col ;
		}
	}
	else
	{
		line->change_beg_col = beg_col ;
		line->change_end_col = end_col ;

		SET_MODIFIED(line->flag) ;
	}

	return  1 ;
}

int
ml_line_set_modified_all(
	ml_line_t *  line
	)
{
	line->change_beg_col = 0 ;
	
	/*
	 * '* 2' assures change_end_col should point over the end of line.
	 * If triple width(or wider) characters(!) were to exist, this hack would make
	 * no sense...
	 */
	line->change_end_col = line->num_of_chars * 2 ;

	SET_MODIFIED(line->flag) ;

	return  1 ;
}

int
ml_line_is_cleared_to_end(
	ml_line_t *  line
	)
{
	if( ml_line_get_num_of_filled_cols( line) < line->change_end_col + 1)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
ml_line_is_modified(
	ml_line_t *  line
	)
{
	return  IS_MODIFIED(line->flag) ;
}

int
ml_line_get_beg_of_modified(
	ml_line_t *  line
	)
{
	if( IS_EMPTY( line))
	{
		return  0 ;
	}
	else
	{
		return  ml_convert_col_to_char_index( line , NULL , line->change_beg_col , 0) ;
	}
}

int
ml_line_get_end_of_modified(
	ml_line_t *  line
	)
{
	if( IS_EMPTY( line))
	{
		return  0 ;
	}
	else
	{
		return  ml_convert_col_to_char_index( line , NULL , line->change_end_col , 0) ;
	}
}

u_int
ml_line_get_num_of_redrawn_chars(
	ml_line_t *  line
	)
{
	if( IS_EMPTY( line))
	{
		return  0 ;
	}
	else
	{
		return  ml_line_get_end_of_modified( line) - ml_line_get_beg_of_modified( line) + 1 ;
	}
}

void
ml_line_updated(
	ml_line_t *  line
	)
{
	UNSET_MODIFIED(line->flag) ;

	line->change_beg_col = 0 ;
	line->change_end_col = 0 ;
}

int
ml_line_is_continued_to_next(
	ml_line_t *  line
	)
{
	return  IS_CONTINUED_TO_NEXT(line->flag) ;
}

void
ml_line_set_continued_to_next(
	ml_line_t *  line
	)
{
	SET_CONTINUED_TO_NEXT(line->flag) ;
}

void
ml_line_unset_continued_to_next(
	ml_line_t *  line
	)
{
	UNSET_CONTINUED_TO_NEXT(line->flag) ;
}

int
ml_convert_char_index_to_col(
	ml_line_t *  line ,
	int  char_index ,
	int  flag		/* BREAK_BOUNDARY */
	)
{
	int  count ;
	int  col ;

	if( IS_EMPTY(line))
	{
		return  0 ;
	}

	if( char_index >= line->num_of_chars)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG
			" char index %d is larger than num_of_chars(%d) ... modified -> %d.\n" ,
			char_index , line->num_of_chars , line->num_of_chars - 1) ;
	#endif

		char_index = line->num_of_chars - 1 ;
	}
	
	col = 0 ;

	if( (flag & BREAK_BOUNDARY) && line->num_of_filled_chars <= char_index)
	{
		for( count = 0 ; count < line->num_of_filled_chars ; count ++)
		{
		#ifdef  DEBUG
			if( ml_char_cols( &line->chars[count]) == 0)
			{
				kik_warn_printf( KIK_DEBUG_TAG " ml_char_cols returns 0.\n") ;
			
				continue ;
			}
		#endif
		
			col += ml_char_cols( &line->chars[count]) ;
		}

		col += (char_index - count) ;
	}
	else if( line->num_of_filled_chars > 0)
	{
		/*
		 * excluding the width of the last char.
		 */
		for( count = 0 ; count < K_MIN(char_index,END_CHAR_INDEX(line)) ; count ++)
		{
			col += ml_char_cols( &line->chars[count]) ;
		}
	}

	return  col ;
}

int
ml_convert_col_to_char_index(
	ml_line_t *  line ,
	u_int *  cols_rest ,
	int  col ,
	int  flag	/* BREAK_BOUNDARY */
	)
{
	int  char_index ;

#ifdef  DEBUG
	if( col >= line->num_of_chars * 2 && cols_rest)
	{
		kik_warn_printf( KIK_DEBUG_TAG
			" Since col [%d] is over line->num_of_chars * 2 [%d],"
			" cols_rest will be corrupt...\n" ,
			col , line->num_of_chars * 2) ;
	}
#endif
	
	for( char_index = 0 ; char_index + 1 < line->num_of_filled_chars ; char_index ++)
	{
		int  cols ;

		cols = ml_char_cols( &line->chars[char_index]);
		if( col < cols)
		{
			goto  end ;
		}

		col -= cols ;
	}
	
	if( col >= ml_char_cols( &line->chars[char_index]) && (flag & BREAK_BOUNDARY))
	{
		col -= ml_char_cols( &line->chars[char_index++]) ;
		char_index += col ;
	}

end:
	if( cols_rest != NULL)
	{
		*cols_rest = col ;
	}

	return  char_index ;
}

int
ml_line_reverse_color(
	ml_line_t *  line ,
	int  char_index
	)
{
	if( char_index >= line->num_of_filled_chars)
	{
		return  0 ;
	}

	ml_char_reverse_color( &line->chars[char_index]) ;

	ml_line_set_modified( line , char_index , char_index) ;

	return  1 ;
}

int
ml_line_restore_color(
	ml_line_t *  line ,
	int  char_index
	)
{
	if( char_index >= line->num_of_filled_chars)
	{
		return  0 ;
	}

	ml_char_restore_color( &line->chars[char_index]) ;

	ml_line_set_modified( line , char_index , char_index) ;

	return  1 ;
}

/*
 * This copys a line as it is and doesn't care about visual order.
 * But bidi parameters are also copyed as it is.
 */
int
ml_line_copy_line(
	ml_line_t *  dst ,
	ml_line_t *  src
	)
{
	u_int  copy_len ;

	copy_len = K_MIN(src->num_of_filled_chars,dst->num_of_chars) ;

	ml_str_copy( dst->chars , src->chars , copy_len) ;
	dst->num_of_filled_chars = copy_len ;

	dst->change_beg_col = K_MIN(src->change_beg_col,dst->num_of_chars) ;
	dst->change_end_col = K_MIN(src->change_end_col,dst->num_of_chars) ;
	
	dst->flag = src->flag ;

	/*
	 * bidi parameters.
	 */
	if( ml_line_is_using_bidi( src))
	{
		ml_line_use_bidi( dst) ;
		ml_line_bidi_render( dst) ;
	}
	else
	{
		ml_line_unuse_bidi( dst) ;
	}

	return  1 ;
}

int
ml_line_share(
	ml_line_t *  dst ,
	ml_line_t *  src
	)
{
	dst->chars = src->chars ;
	dst->num_of_filled_chars = src->num_of_filled_chars ;
	
	dst->bidi_state = src->bidi_state ;

	dst->change_beg_col = src->change_beg_col ;
	dst->change_end_col = src->change_end_col ;
	dst->flag = src->flag ;

	return  1 ;
}

int
ml_line_is_empty(
	ml_line_t *  line
	)
{
	return  IS_EMPTY(line) ;
}

u_int
ml_line_get_num_of_filled_cols(
	ml_line_t *  line
	)
{
	return  ml_str_cols( line->chars , line->num_of_filled_chars) ;
}

int
ml_line_end_char_index(
	ml_line_t *  line
	)
{
	if( IS_EMPTY(line))
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
ml_line_beg_char_index_except_spaces(
	ml_line_t *  line
	)
{
	int  char_index ;

	if( ml_line_is_rtl( line))
	{
		for( char_index = 0 ; char_index < line->num_of_filled_chars ; char_index ++)
		{
			if( ! ml_char_bytes_is( &line->chars[char_index] , " " , 1 , US_ASCII))
			{
				return  char_index ;
			}
		}
	}

	return  0 ;
}

u_int
ml_get_num_of_filled_chars_except_spaces(
	ml_line_t *  line
	)
{
	int  char_index ;

	if( IS_EMPTY(line))
	{
		return  0 ;
	}
	else if( ml_line_is_rtl( line))
	{
		return  line->num_of_filled_chars ;
	}
	else
	{
		for( char_index = END_CHAR_INDEX(line) ; char_index >= 0 ; char_index --)
		{
			if( ! ml_char_bytes_is( &line->chars[char_index] , " " , 1 , US_ASCII))
			{
				return  char_index + 1 ;
			}
		}

		return  0 ;
	}
}


/*
 * Functions which must care about visual order.
 */

int
ml_line_is_using_bidi(
	ml_line_t *  line
	)
{
	if( line->bidi_state)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
ml_line_use_bidi(
	ml_line_t *  line
	)
{
	if( line->bidi_state)
	{
		return  1 ;
	}

	if( ( line->bidi_state = ml_bidi_new()) == NULL)
	{
		return  0 ;
	}

	return  1 ;
}

int
ml_line_unuse_bidi(
        ml_line_t *  line
        )
{
	if( line->bidi_state)
	{
		ml_bidi_delete( line->bidi_state) ;
		line->bidi_state = NULL ;
	}

	return  1 ;
}

int
ml_line_is_rtl(
	ml_line_t *  line
	)
{
	if( line->bidi_state)
	{
		return  line->bidi_state->base_is_rtl ;
	}
	else
	{
		return  0 ;
	}
}

int
ml_line_bidi_render(
	ml_line_t *  line
	)
{
	int  base_is_rtl ;
	int  result ;
	
	if( ! line->bidi_state)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " render failed. bidi_state is NULL\n") ;
	#endif

		return  0 ;
	}

	base_is_rtl = line->bidi_state->base_is_rtl ;

	result = ml_bidi( line->bidi_state , line->chars , line->num_of_filled_chars) ;

	if( base_is_rtl && ! line->bidi_state->base_is_rtl)
	{
		/* shifting RTL-base to LTR-base (which requires redrawing line all) */
		ml_line_set_modified_all( line) ;
	}
	else if( line->bidi_state->has_rtl && IS_MODIFIED(line->flag))
	{
		/* if line contains RTL chars, line is redrawn all */
		ml_line_set_modified( line , 0 , END_CHAR_INDEX(line)) ;
	}
	
	return  result ;
}

int
ml_line_bidi_visual(
	ml_line_t *  line
	)
{
	int  count ;
	ml_char_t *  src ;
	
	if( ! line->bidi_state)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " bidi_state is NULL\n") ;
	#endif

		return  0 ;
	}

	if( line->bidi_state->size == 0)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " not rendered yet.\n") ;
	#endif
	
		return  0 ;
	}

	if( ( src = ml_str_alloca( line->bidi_state->size)) == NULL)
	{
		return  0 ;
	}

	ml_str_copy( src , line->chars , line->bidi_state->size) ;

	for( count = 0 ; count < line->bidi_state->size ; count ++)
	{
		ml_char_copy( &line->chars[line->bidi_state->visual_order[count]] , &src[count]) ;
	}

	ml_str_final( src , line->bidi_state->size) ;

	return  1 ;
}

int
ml_line_bidi_logical(
	ml_line_t *  line
	)
{
	int  count ;
	ml_char_t *  src ;
	
	if( ! line->bidi_state)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " bidi_state is NULL\n") ;
	#endif

		return  0 ;
	}

	if( line->bidi_state->size == 0)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " not rendered yet.\n") ;
	#endif
	
		return  0 ;
	}

	if( ( src = ml_str_alloca( line->bidi_state->size)) == NULL)
	{
		return  0 ;
	}
	
	ml_str_copy( src , line->chars , line->bidi_state->size) ;

	for( count = 0 ; count < line->bidi_state->size ; count ++)
	{
		ml_char_copy( &line->chars[count] , &src[line->bidi_state->visual_order[count]]) ;
	}

	ml_str_final( src , line->bidi_state->size) ;

	/*
	 * !! Notice !!
	 * is_modified is as it is , which should not be touched here.
	 */

	return  1 ;
}

int
ml_bidi_convert_logical_char_index_to_visual(
	ml_line_t *  line ,
	int  char_index ,
	int *  ltr_rtl_meet_pos
	)
{
	if( ! line->bidi_state)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " bidi_state is NULL\n") ;
	#endif
	
		return  char_index ;
	}

	if( 0 <= char_index && char_index < line->bidi_state->size)
	{
		int  count ;

		if( ! line->bidi_state->base_is_rtl && char_index >= 2)
		{
			for( count = char_index - 2 ; count >= 0 ; count --)
			{
				if( line->bidi_state->visual_order[count] <
						line->bidi_state->visual_order[count + 1] &&
					line->bidi_state->visual_order[count + 1] + 1 <
						line->bidi_state->visual_order[count + 2])
				{
					if( *ltr_rtl_meet_pos != line->bidi_state->visual_order[count + 2])
					{
						*ltr_rtl_meet_pos =
							line->bidi_state->visual_order[count + 2] ;

						if( line->bidi_state->visual_order[count + 2] + 1 ==
							line->bidi_state->visual_order[char_index])
						{
							return  line->bidi_state->visual_order[count + 1] ;
						}
					}

					break ;
				}
			}

			if( count == 0)
			{
				*ltr_rtl_meet_pos = 0 ;
			}
		}
		else if( line->bidi_state->base_is_rtl && char_index >= 2)
		{
			for( count = char_index - 2 ; count >= 0 ; count --)
			{
				if( line->bidi_state->visual_order[count] >
						line->bidi_state->visual_order[count + 1] &&
					line->bidi_state->visual_order[count + 1] >
						line->bidi_state->visual_order[count + 2] + 1)
				{
					if( *ltr_rtl_meet_pos != line->bidi_state->visual_order[count + 2])
					{
						*ltr_rtl_meet_pos =
							line->bidi_state->visual_order[count + 2] ;

						if( line->bidi_state->visual_order[count + 2] ==
							line->bidi_state->visual_order[char_index] + 1)
						{
							return  line->bidi_state->visual_order[count + 1] ;
						}
					}

					break ;
				}
			}

			if( count == 0)
			{
				*ltr_rtl_meet_pos = 0 ;
			}
		}
		else
		{
			*ltr_rtl_meet_pos = 0 ;
		}
		
		return  line->bidi_state->visual_order[char_index] ;
	}
	else
	{
		*ltr_rtl_meet_pos = 0 ;
		
		return  char_index ;
	}
}

int
ml_line_copy_str(
	ml_line_t *  line ,
	ml_char_t *  dst ,
	int  beg ,
	u_int  len
	)
{
	if( line->bidi_state == NULL || line->bidi_state->size == 0)
	{
		return  ml_str_copy( dst , &line->chars[beg] , len) ;
	}
	else
	{
		/*
		 * XXX
		 * adhoc implementation.
		 */
		 
		int *  flags ;
		int  bidi_pos ;
		int  norm_pos ;
		int  dst_pos ;

		if( ( flags = alloca( sizeof( int) * line->bidi_state->size)) == NULL)
		{
			return  0 ;
		}

		memset( flags , 0 , sizeof( int) * line->bidi_state->size) ;

		for( bidi_pos = beg ; bidi_pos < beg + len ; bidi_pos ++)
		{
			for( norm_pos = 0 ; norm_pos < line->bidi_state->size ; norm_pos ++)
			{
				if( line->bidi_state->visual_order[norm_pos] == bidi_pos)
				{
					flags[norm_pos] = 1 ;
				}
			}
		}

		for( dst_pos = norm_pos = 0 ; norm_pos < line->bidi_state->size ; norm_pos ++)
		{
			if( flags[norm_pos])
			{
				ml_char_copy( &dst[dst_pos ++] ,
					&line->chars[line->bidi_state->visual_order[norm_pos]]) ;
			}
		}

		return  1 ;
	}
}


int
ml_line_iscii_visual(
	ml_line_t *  line ,
	ml_iscii_lang_t  iscii_lang
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
	int  count ;

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
		
		if( ml_char_cs( ch) == ISCII)
		{
			u_int  font_filled ;
			
			iscii_buf[iscii_filled ++] = ml_char_bytes( ch)[0] ;
			iscii_buf[iscii_filled] = '\0' ;
			font_filled = ml_iscii_shape( iscii_lang , font_buf , font_buf_len , iscii_buf) ;

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
				
				for( count = 1 ; count <= prev_font_filled - font_filled ; count ++)
				{
					int  comb_pos ;
					
					c = &dst[dst_pos + count] ;

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
						else if( ! ml_combine_chars( &dst[dst_pos] , c))
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
				else if( ! ml_combine_chars( &dst[dst_pos] , ch))
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
					for( count = 0 ; count < font_filled - prev_font_filled - 1 ;
						count ++)
					{
						if( ++ dst_pos >= dst_len)
						{
							goto  end ;
						}

						ml_char_copy( &dst[dst_pos] , ch) ;
						
						/* NULL */
						ml_char_set_bytes( &dst[dst_pos] , "\x0") ;
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
		ml_line_set_modified_all( line) ;
	}

	return  1 ;
}

/*
 * this should be called after ml_line_iscii_visaul()
 */
int
ml_iscii_convert_logical_char_index_to_visual(
	ml_line_t *  line ,
	int  logical_char_index
	)
{
	int  count ;
	int  visual_char_index ;
	u_int  comb_size ;

	if( IS_EMPTY(line))
	{
		return  0 ;
	}

	visual_char_index = 0 ;

	for( count = 0 ; count < logical_char_index && count < END_CHAR_INDEX(line) ; count ++)
	{
		if( ml_char_cs( &line->chars[count]) == ISCII)
		{
			/*
			 * skipping trailing null characters of previous character.
			 */
			while( ml_char_is_null( &line->chars[count]))
			{
				if( ++ count >= line->num_of_filled_chars)
				{
					goto  end ;
				}

				logical_char_index ++ ;
				visual_char_index ++ ;
			}

			if( ml_get_combining_chars( &line->chars[count] , &comb_size))
			{
				logical_char_index -= comb_size ;
			}
		}
		
		visual_char_index ++ ;
	}

	while( ml_char_is_null( &line->chars[count]))
	{
		if( ++ count >= line->num_of_filled_chars)
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
		 * This must never happens, so abort() should be done here.
		 * But since this happens not a few times, this is enclosed by __DEBUG.
		 */
		abort() ;
	#endif
	
		visual_char_index = END_CHAR_INDEX(line) ;
	}

	return  visual_char_index ;
}

ml_line_t *
ml_line_shape(
	ml_line_t *  line ,
	ml_shape_t *  shape
	)
{
	ml_line_t *  orig ;
	ml_char_t *  shaped ;

	if( ( orig = malloc( sizeof( ml_line_t))) == NULL)
	{
		return  NULL ;
	}

	ml_line_share( orig , line) ;
	
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
ml_line_unshape(
	ml_line_t *  line ,
	ml_line_t *  orig
	)
{
	ml_str_delete( line->chars , line->num_of_chars) ;
	
	line->chars = orig->chars ;
	line->num_of_filled_chars = orig->num_of_filled_chars ;

	free( orig) ;

	return  1 ;
}

#ifdef  DEBUG

void
ml_line_dump(
	ml_line_t *  line
	)
{
	int  count ;

	for( count = 0 ; count < line->num_of_filled_chars ; count ++)
	{
		ml_char_dump( &line->chars[count]) ;
	}

	kik_msg_printf( "\n") ;
}

#endif
