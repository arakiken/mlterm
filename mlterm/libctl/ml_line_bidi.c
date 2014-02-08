/*
 *	$Id$
 */

#include  "ml_line_bidi.h"

#include  <string.h>		/* memset */
#include  <kiklib/kik_debug.h>


/* --- static functions --- */

static void
copy_char_with_mirror_check(
	ml_char_t *  dst ,
	ml_char_t *  src ,
	u_int16_t *  visual_order ,
	u_int16_t  visual_order_size ,
	int  pos
	)
{
	ml_char_copy( dst , src) ;

	if( (( pos > 0 && visual_order[pos - 1] == visual_order[pos] + 1) ||
	     ( pos + 1 < visual_order_size &&
	       visual_order[pos] == visual_order[pos + 1] + 1)) )
	{
		/*
		 * Pos is RTL character.
		 * => XXX It is assumed that pos is always US_ASCII or ISO10646_UCS4_1.
		 */
	#if  0
		mkf_charset_t  cs ;

		if( (cs = ml_char_cs( dst)) == US_ASCII || cs == ISO10646_UCS4_1)
	#endif
		{
			u_int  mirror ;

			if( ( mirror = ml_bidi_get_mirror_char( ml_char_code( dst))))
			{
				ml_char_set_code( dst , mirror) ;
			}
		}
	}
}


/* --- global functions --- */

int
ml_line_set_use_bidi(
	ml_line_t *  line ,
	int  flag
	)
{
	if( flag)
	{
		if( ml_line_is_using_bidi( line))
		{
			return  1 ;
		}
		else if( line->ctl_info_type != 0)
		{
			return  0 ;
		}

		if( ( line->ctl_info.bidi = ml_bidi_new()) == NULL)
		{
			return  0 ;
		}

		line->ctl_info_type = VINFO_BIDI ;
	}
	else
	{
		if( ml_line_is_using_bidi( line))
		{
			ml_bidi_delete( line->ctl_info.bidi) ;
			line->ctl_info_type = 0 ;
		}
	}

	return  1 ;
}

int
ml_line_bidi_render(
	ml_line_t *  line ,
	ml_bidi_mode_t  bidi_mode
	)
{
	int  base_is_rtl ;

	if( ! ml_line_is_using_bidi( line))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" Rendering failed. ctl_info_type isn't VINFO_BIDI.\n") ;
	#endif

		return  0 ;
	}

	base_is_rtl = BASE_IS_RTL( line->ctl_info.bidi) ;

	if( ! ml_bidi( line->ctl_info.bidi , line->chars , line->num_of_filled_chars , bidi_mode))
	{
		return  0 ;
	}

	/* Conforming line->change_{beg|end}_col to visual mode. */
	if( base_is_rtl != BASE_IS_RTL( line->ctl_info.bidi))
	{
		/*
		 * shifting RTL-base to LTR-base or LTR-base to RTL-base.
		 * (which requires redrawing line all)
		 */
		ml_line_set_modified_all( line) ;
	}
	else if( HAS_RTL( line->ctl_info.bidi) && ml_line_is_modified( line))
	{
		/*
		 * If line contains RTL chars, line is redrawn all.
		 * It is assumed that num of logical characters in line is the same as
		 * that of visual ones.
		 */
		ml_line_set_modified( line , 0 , ml_line_end_char_index( line)) ;
	}
	
	return  1 ;
}

int
ml_line_bidi_visual(
	ml_line_t *  line
	)
{
	int  count ;
	ml_char_t *  src ;
	
	if( ! ml_line_is_using_bidi( line) ||
	    line->ctl_info.bidi->size == 0 ||
	    ! HAS_RTL( line->ctl_info.bidi))
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Not need to visualize.\n") ;
	#endif

		return  1 ;
	}

	if( ( src = ml_str_alloca( line->ctl_info.bidi->size)) == NULL)
	{
		return  0 ;
	}

	ml_str_copy( src , line->chars , line->ctl_info.bidi->size) ;

	for( count = 0 ; count < line->ctl_info.bidi->size ; count ++)
	{
		copy_char_with_mirror_check(
				line->chars + line->ctl_info.bidi->visual_order[count] ,
				src + count , line->ctl_info.bidi->visual_order ,
				line->ctl_info.bidi->size , count) ;
	}

	ml_str_final( src , line->ctl_info.bidi->size) ;

	return  1 ;
}

int
ml_line_bidi_logical(
	ml_line_t *  line
	)
{
	int  count ;
	ml_char_t *  src ;
	
	if( ! ml_line_is_using_bidi( line) ||
	    line->ctl_info.bidi->size == 0 ||
	    ! HAS_RTL( line->ctl_info.bidi))
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Not need to logicalize.\n") ;
	#endif

		return  0 ;
	}

	if( ( src = ml_str_alloca( line->ctl_info.bidi->size)) == NULL)
	{
		return  0 ;
	}
	
	ml_str_copy( src , line->chars , line->ctl_info.bidi->size) ;

	for( count = 0 ; count < line->ctl_info.bidi->size ; count ++)
	{
		copy_char_with_mirror_check( line->chars + count ,
			src + line->ctl_info.bidi->visual_order[count] ,
			line->ctl_info.bidi->visual_order ,
			line->ctl_info.bidi->size , count) ;
	}

	ml_str_final( src , line->ctl_info.bidi->size) ;

	/*
	 * !! Notice !!
	 * is_modified is as it is , which should not be touched here.
	 */

	return  1 ;
}

int
ml_line_bidi_convert_logical_char_index_to_visual(
	ml_line_t *  line ,
	int  char_index ,
	int *  ltr_rtl_meet_pos
	)
{
	if( ml_line_is_using_bidi( line) &&
	    (u_int)char_index < line->ctl_info.bidi->size &&  /* same as 0 <= char_index < size */
	    HAS_RTL( line->ctl_info.bidi))
	{
		int  count ;

		if( ! BASE_IS_RTL( line->ctl_info.bidi) && char_index >= 1)
		{
			for( count = char_index - 2 ; count >= -1 ; count --)
			{
				/*
				 * visual order -> 1 2 4 3 5
				 *                   ^ ^   ^- char index
				 *                   | |
				 * cursor position --+ +-- meet position
				 *
				 * visual order -> 1 2*5*4 3 6
				 *                 ^ ^ ^     ^- char index
				 *                   | |
				 * cursor position --+ +-- meet position
				 *
				 * visual order -> 1 2 3 6 5 4 7
				 *                   ^ ^ ^     ^- char index
				 *                     | |
				 *   cursor position --+ +-- meet position
				 */
			#if  0
				kik_debug_printf(
				" Normal pos %d - Current pos %d %d %d - Meet position %d\n" ,
					line->ctl_info.bidi->visual_order[char_index] ,
					count >= 0 ? line->ctl_info.bidi->visual_order[count] : 0 ,
				        line->ctl_info.bidi->visual_order[count + 1] ,
				        line->ctl_info.bidi->visual_order[count + 2] ,
					*ltr_rtl_meet_pos) ;
			#endif
				if( (count < 0 ||
				        line->ctl_info.bidi->visual_order[count] <
				        line->ctl_info.bidi->visual_order[count + 1]) &&
				    line->ctl_info.bidi->visual_order[count + 1] + 1 <
				        line->ctl_info.bidi->visual_order[count + 2])
				{
					/*
					 * If meet position is not changed, text isn't changed
					 * but cursor is moved. In this case cursor position should
					 * not be fixed to visual_order[count + 1].
					 */
					if( *ltr_rtl_meet_pos !=
						line->ctl_info.bidi->visual_order[count + 1] +
					        line->ctl_info.bidi->visual_order[count + 2])
					{
						*ltr_rtl_meet_pos =
						    line->ctl_info.bidi->visual_order[count + 1] +
						    line->ctl_info.bidi->visual_order[count + 2] ;
						
						if( line->ctl_info.bidi->visual_order[char_index] ==
							line->ctl_info.bidi->visual_order[count + 2] + 1)
						{
							return  line->ctl_info.bidi->visual_order[
									count + 1] ;
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
		else if( BASE_IS_RTL( line->ctl_info.bidi) && char_index >= 1)
		{
			for( count = char_index - 2 ; count >= -1 ; count --)
			{
				/*
				 * visual order -> 6 5 4 2 3 1
				 *                   ^ ^ ^   ^- char index
				 *                     |
				 *                     +-- meet position & cursor position
				 * visual order -> 7 6 5 2 3*4*1
				 *                   ^ ^ ^     ^- char index
				 *                     |
				 *                     +-- meet position & cursor position
				 *
				 * visual order -> 7 6 4 5 3 2 1
				 *                 ^ ^ ^   ^- char index
				 *                   | |
				 * cursor position --+ +-- meet position
				 * visual order -> 7 6 3 4*5*2 1
				 *                 ^ ^ ^     ^- char index
				 *                   | |
				 * cursor position --+ +-- meet position
				 */
			#if  0
				kik_debug_printf(
				" Normal pos %d - Current pos %d %d %d - Meet position %d\n" ,
					line->ctl_info.bidi->visual_order[char_index] ,
					count >= 0 ? line->ctl_info.bidi->visual_order[count] : 0 ,
				        line->ctl_info.bidi->visual_order[count + 1] ,
				        line->ctl_info.bidi->visual_order[count + 2] ,
					*ltr_rtl_meet_pos) ;
			#endif

				if( (count < 0 ||
				        line->ctl_info.bidi->visual_order[count] >
				        line->ctl_info.bidi->visual_order[count + 1]) &&
				    line->ctl_info.bidi->visual_order[count + 1] >
				        line->ctl_info.bidi->visual_order[count + 2] + 1)
				{
					if( *ltr_rtl_meet_pos !=
						line->ctl_info.bidi->visual_order[count + 1] +
						line->ctl_info.bidi->visual_order[count + 2])
					{
						*ltr_rtl_meet_pos =
						    line->ctl_info.bidi->visual_order[count + 1] +
						    line->ctl_info.bidi->visual_order[count + 2] ;

						if( line->ctl_info.bidi->visual_order[char_index] + 1 ==
					            line->ctl_info.bidi->visual_order[count + 2])
						{
							return  line->ctl_info.bidi->visual_order[
									count + 1] ;
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
		
		return  line->ctl_info.bidi->visual_order[char_index] ;
	}
	else
	{
		*ltr_rtl_meet_pos = 0 ;
		
		return  char_index ;
	}
}


/*
 * This function is used only by a loader of this module (not used inside this module),
 * so it is assumed that ml_line_is_using_bidi() was already checked (otherwise this
 * module can be loaded unnecessarily).
 */
int
ml_line_bidi_convert_visual_char_index_to_logical(
	ml_line_t *  line ,
	int  char_index
	)
{
	u_int  count ;

	for( count = 0 ; count < line->ctl_info.bidi->size ; count++)
	{
		if( line->ctl_info.bidi->visual_order[count] == char_index)
		{
			return  count ;
		}
	}

	return  char_index ;
}

/*
 * This function is used only by a loader of this module (not used inside this module),
 * so it is assumed that ml_line_is_using_bidi() was already checked (otherwise this
 * module can be loaded unnecessarily).
 */
int
ml_line_bidi_is_rtl(
	ml_line_t *  line
	)
{
	return  BASE_IS_RTL( line->ctl_info.bidi) ;
}

int
ml_line_bidi_need_shape(
	ml_line_t *  line
	)
{
	return  HAS_RTL( line->ctl_info.bidi) ;
}

/*
 * This function is used only by a loader of this module (not used inside this module),
 * so it is assumed that ml_line_is_using_bidi() was already checked.
 */
int
ml_line_bidi_copy_logical_str(
	ml_line_t *  line ,
	ml_char_t *  dst ,
	int  beg ,		/* visual position */
	u_int  len
	)
{
	/*
	 * XXX
	 * adhoc implementation.
	 */

	int *  flags ;
	int  bidi_pos ;
	int  norm_pos ;
	int  dst_pos ;

	if( line->ctl_info.bidi->size == 0)
	{
		return  0 ;
	}

	if( ( flags = alloca( sizeof( int) * line->ctl_info.bidi->size)) == NULL)
	{
		return  0 ;
	}

	memset( flags , 0 , sizeof( int) * line->ctl_info.bidi->size) ;

	for( bidi_pos = beg ; bidi_pos < beg + len ; bidi_pos ++)
	{
		for( norm_pos = 0 ; norm_pos < line->ctl_info.bidi->size ; norm_pos ++)
		{
			if( line->ctl_info.bidi->visual_order[norm_pos] == bidi_pos)
			{
				flags[norm_pos] = 1 ;
			}
		}
	}

	for( dst_pos = norm_pos = 0 ; norm_pos < line->ctl_info.bidi->size ;
		norm_pos ++)
	{
		if( flags[norm_pos])
		{
			copy_char_with_mirror_check( &dst[dst_pos ++] ,
				line->chars + line->ctl_info.bidi->visual_order[norm_pos] ,
				line->ctl_info.bidi->visual_order ,
				line->ctl_info.bidi->size , norm_pos) ;
		}
	}

	return  1 ;
}
