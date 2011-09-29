/*
 *	$Id$
 */

#include  "ml_line_iscii.h"

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

int
ml_line_iscii_render(
	ml_line_t *  line
	)
{
	if( ! ml_line_is_using_iscii( line))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" Rendering failed. ctl_info_type isn't VINFO_ISCII.\n") ;
	#endif

		return  0 ;
	}

	if( ! ml_iscii( line->ctl_info.iscii , line->chars , line->num_of_filled_chars))
	{
		return  0 ;
	}

	if( line->ctl_info.iscii->has_iscii && ml_line_is_modified( line))
	{
		/*
		 * Conforming line->change_{beg|end}_col to visual mode.
		 * If line contains ISCII chars, line is redrawn to end of line.
		 */
		ml_line_set_modified( line ,
			ml_line_iscii_convert_logical_char_index_to_visual( line ,
				ml_line_get_beg_of_modified( line)) ,
			line->num_of_chars) ;
	}

	return  1 ;
}

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

	if( ! ml_line_is_using_iscii( line) ||
	    line->ctl_info.iscii->size == 0 ||
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

	dst = line->chars ;
	dst_len = K_MIN(line->num_of_chars,line->ctl_info.iscii->size) ;

	src_pos = 0 ;
	for( dst_pos = 0 ; dst_pos < dst_len ; dst_pos ++)
	{
		if( line->ctl_info.iscii->num_of_chars_array[dst_pos] == 0)
		{
			ml_char_copy( dst + dst_pos , src + src_pos - 1) ;
			/* NULL */
			ml_char_set_bytes( dst + dst_pos , "\x0") ;
		}
		else
		{
			u_int  count ;

			ml_char_copy( dst + dst_pos , src + (src_pos ++)) ;

			for( count = 1 ;
			     count < line->ctl_info.iscii->num_of_chars_array[dst_pos] ; count++)
			{
				ml_char_combine_simple( dst + dst_pos , src + (src_pos ++)) ;
			}
		}
	}

	ml_str_final( src , src_len) ;

	line->num_of_filled_chars = dst_pos ;

	return  1 ;
}

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

	if( ! ml_line_is_using_iscii( line) ||
	    line->ctl_info.iscii->size == 0 ||
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
