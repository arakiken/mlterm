/*
 *	$Id$
 */

#include  "ml_line_iscii.h"

#include  <stdio.h>			/* NULL */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>		/* K_MIN */

#include  "ml_iscii.h"


/* --- global functions --- */

int
ml_line_set_use_iscii(
	ml_line_t *  line ,
	int  flag
	)
{
	if( flag)
	{
		if( ml_line_is_using_iscii( line))
		{
			return  1 ;
		}
		else if( line->ctl_info_type != 0)
		{
			return  0 ;
		}

		if( ( line->ctl_info.iscii = ml_iscii_new()) == NULL)
		{
			return  0 ;
		}

		line->ctl_info_type = VINFO_ISCII ;
	}
	else
	{
		if( ml_line_is_using_iscii( line))
		{
			ml_iscii_delete( line->ctl_info.iscii) ;
			line->ctl_info_type = 0 ;
		}
	}

	return  1 ;
}

/* The caller should check ml_line_is_using_iscii() in advance. */
int
ml_line_iscii_render(
	ml_line_t *  line
	)
{
	int  had_iscii ;
	int  ret ;

	had_iscii = line->ctl_info.iscii->has_iscii ;

	if( ! ( ret = ml_iscii( line->ctl_info.iscii , line->chars , line->num_of_filled_chars)))
	{
		return  0 ;
	}

	/*
	 * Not only has_iscii but also had_iscii should be checked.
	 *
	 * Lower case: ASCII
	 * Upper case: ISCII
	 *    (Logical) AAA == (Visual) BBBBB
	 * => (Logical) aaa == (Visual) aaa
	 * In this case ml_line_is_cleared_to_end() returns 0, so "BB" remains on
	 * the screen unless following ml_line_set_modified().
	 */
	if( ( had_iscii || line->ctl_info.iscii->has_iscii) && ml_line_is_modified( line))
	{
		/*
		 * Conforming line->change_{beg|end}_col to visual mode.
		 * If this line contains ISCII chars, it should be redrawn to the end of line.
		 */
		ml_line_set_modified( line ,
			ml_line_iscii_convert_logical_char_index_to_visual( line ,
				ml_line_get_beg_of_modified( line)) ,
			line->num_of_chars) ;
	}

	return  ret ;
}

/* The caller should check ml_line_is_using_iscii() in advance. */
int
ml_line_iscii_visual(
	ml_line_t *  line
	)
{
	ml_char_t *  src ;
	u_int  src_len ;
	ml_char_t *  dst ;
	u_int  dst_len ;
	int  dst_pos ;
	int  src_pos ;

	if( line->ctl_info.iscii->size == 0 ||
	    ! line->ctl_info.iscii->has_iscii)
	{
	#ifdef  __DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Not need to visualize.\n") ;
	#endif

		return  1 ;
	}

	src_len = line->num_of_filled_chars ;
	if( ( src = ml_str_alloca( src_len)) == NULL)
	{
		return  0 ;
	}
	
	ml_str_copy( src , line->chars , src_len) ;

	dst_len = line->ctl_info.iscii->size ;
	if( line->num_of_chars < dst_len)
	{
		ml_char_t *  chars ;

		if( ( chars = ml_str_new( dst_len)))
		{
			/* XXX => shrunk at ml_screen.c and ml_logical_visual_iscii.c */
			ml_str_delete( line->chars , line->num_of_chars) ;
			line->chars = chars ;
			line->num_of_chars = dst_len ;
		}
		else
		{
			dst_len = line->num_of_chars ;
		}
	}

	dst = line->chars ;

	src_pos = 0 ;
	for( dst_pos = 0 ; dst_pos < dst_len ; dst_pos ++)
	{
		if( line->ctl_info.iscii->num_of_chars_array[dst_pos] == 0)
		{
			ml_char_copy( dst + dst_pos , ml_get_base_char( src + src_pos - 1)) ;
			/* NULL */
			ml_char_set_code( dst + dst_pos , 0) ;
		}
		else
		{
			u_int  count ;

			ml_char_copy( dst + dst_pos , src + (src_pos ++)) ;

			for( count = 1 ;
			     count < line->ctl_info.iscii->num_of_chars_array[dst_pos] ; count++)
			{
				ml_char_t *  comb ;
				u_int  num ;

			#ifdef  DEBUG
				if( ml_char_is_comb( ml_get_base_char( src + src_pos)))
				{
					kik_debug_printf( KIK_DEBUG_TAG " illegal iscii\n") ;
				}
			#endif
				ml_char_combine_simple( dst + dst_pos ,
					ml_get_base_char( src + src_pos)) ;

				if( ( comb = ml_get_combining_chars( src + (src_pos ++) , &num)))
				{
					for( ; num > 0 ; num--)
					{
					#ifdef  DEBUG
						if( ! ml_char_is_comb( comb))
						{
							kik_debug_printf( KIK_DEBUG_TAG
								" illegal iscii\n") ;
						}
					#endif
						ml_char_combine_simple( dst + dst_pos , comb ++) ;
					}
				}
			}
		}
	}

#ifdef  DEBUG
	if( src_pos != src_len)
	{
		kik_debug_printf( KIK_DEBUG_TAG "ml_line_iscii_visual() failed: %d -> %d\n" ,
			src_len , src_pos) ;
	}
#endif

	ml_str_final( src , src_len) ;

	line->num_of_filled_chars = dst_pos ;

	return  1 ;
}

/* The caller should check ml_line_is_using_iscii() in advance. */
int
ml_line_iscii_logical(
	ml_line_t *  line
	)
{
	ml_char_t *  src ;
	u_int  src_len ;
	ml_char_t *  dst ;
	int  src_pos ;

	if( line->ctl_info.iscii->size == 0 ||
	    ! line->ctl_info.iscii->has_iscii)
	{
	#ifdef  __DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Not need to logicalize.\n") ;
	#endif

		return  1 ;
	}

	src_len = line->num_of_filled_chars ;
	if( ( src = ml_str_alloca( src_len)) == NULL)
	{
		return  0 ;
	}

	ml_str_copy( src , line->chars , src_len) ;
	dst = line->chars ;

	for( src_pos = 0 ; src_pos < line->ctl_info.iscii->size ; src_pos++)
	{
		ml_char_t *  comb ;
		u_int  num ;

		if( line->ctl_info.iscii->num_of_chars_array[src_pos] == 0)
		{
			continue ;
		}
		else if( line->ctl_info.iscii->num_of_chars_array[src_pos] == 1)
		{
			ml_char_copy( dst , src + src_pos) ;
		}
		else
		{
			ml_char_copy( dst , ml_get_base_char( src + src_pos)) ;

			if( ( comb = ml_get_combining_chars( src + src_pos , &num)))
			{
				for( ; num > 0 ; num-- , comb++)
				{
					if( ml_char_is_comb( comb))
					{
						ml_char_combine_simple( dst , comb) ;
					}
					else
					{
						ml_char_copy( ++ dst , comb) ;
					}
				}
			}
		}

		dst++ ;
	}

	ml_str_final( src , src_len) ;

	line->num_of_filled_chars = dst - line->chars ;

	return  1 ;
}

/* The caller should check ml_line_is_using_iscii() in advance. */
int
ml_line_iscii_convert_logical_char_index_to_visual(
	ml_line_t *  line ,
	int  logical_char_index
	)
{
	int  visual_char_index ;
	int  end_char_index ;

	if( ml_line_is_empty(line))
	{
		return  0 ;
	}

	if( line->ctl_info.iscii->size == 0 ||
	    ! line->ctl_info.iscii->has_iscii)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " logical char_index is same as visual one.\n") ;
	#endif
		return  logical_char_index ;
	}

	end_char_index = ml_line_end_char_index( line) ;
	for( visual_char_index = 0 ; visual_char_index < end_char_index ; visual_char_index++)
	{
		if( ( logical_char_index -=
			line->ctl_info.iscii->num_of_chars_array[visual_char_index]) < 0)
		{
			break ;
		}
	}

	return  visual_char_index ;
}

int
ml_line_iscii_need_shape(
	ml_line_t *  line
	)
{
	return  line->ctl_info.iscii->size > 0 && line->ctl_info.iscii->has_iscii ;
}
