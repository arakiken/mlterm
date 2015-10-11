/*
 *	$Id$
 */

#include  "ml_line.h"

#include  <stdio.h>	/* NULL */
#include  <kiklib/kik_mem.h>
#include  "ml_shape.h"
#include  "ml_ctl_loader.h"


#define  ml_line_is_using_bidi( line)  ((line)->ctl_info_type == VINFO_BIDI)
#define  ml_line_is_using_iscii( line)  ((line)->ctl_info_type == VINFO_ISCII)


/* --- static functions --- */

#ifndef  NO_DYNAMIC_LOAD_CTL

static int
ml_line_bidi_need_shape(
	ml_line_t *  line
	)
{
	int (*func)( ml_line_t *) ;

	if( ! ( func = ml_load_ctl_bidi_func( ML_LINE_BIDI_NEED_SHAPE)))
	{
		return  0 ;
	}

	return  (*func)( line) ;
}

static int
ml_line_iscii_need_shape(
	ml_line_t *  line
	)
{
	int (*func)( ml_line_t *) ;

	if( ! ( func = ml_load_ctl_iscii_func( ML_LINE_ISCII_NEED_SHAPE)))
	{
		return  0 ;
	}

	return  (*func)( line) ;
}

#else

#ifndef  USE_FRIBIDI
#define  ml_line_bidi_need_shape( line)  (0)
#else
/* Link functions in libctl/ml_*bidi.c */
int  ml_line_bidi_need_shape( ml_line_t *  line) ;
#endif	/* USE_FRIBIDI */

#ifndef  USE_IND
#define  ml_line_iscii_need_shape( line)  (0)
#else
/* Link functions in libctl/ml_*iscii.c */
int  ml_line_iscii_need_shape( ml_line_t *  line) ;
#endif	/* USE_IND */

#endif


/* --- global functions --- */

ml_line_t *
ml_line_shape(
	ml_line_t *  line
	)
{
	ml_line_t *  orig ;
	ml_char_t *  shaped ;
	u_int  (*func)( ml_char_t * , u_int , ml_char_t * , u_int) ;

	if( line->ctl_info_type)
	{
		if( ml_line_is_using_bidi( line))
		{
			if( ! ml_line_bidi_need_shape( line))
			{
				return  NULL ;
			}

			func = ml_shape_arabic ;
		}
		else /* if( ml_line_is_using_iscii( line)) */
		{
			if( ! ml_line_iscii_need_shape( line))
			{
				return  NULL ;
			}

			func = ml_shape_iscii ;
		}

		if( ( orig = malloc( sizeof( ml_line_t))) == NULL)
		{
			return  NULL ;
		}

		ml_line_share( orig , line) ;

		if( ( shaped = ml_str_new( line->num_of_chars)) == NULL)
		{
			free( orig) ;

			return  NULL ;
		}

		line->num_of_filled_chars = (*func)( shaped , line->num_of_chars ,
							line->chars , line->num_of_filled_chars) ;

		line->chars = shaped ;

		return  orig ;
	}

	return  NULL ;
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
