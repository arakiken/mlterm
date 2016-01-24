/*
 *	$Id$
 */

#include  "ml_shape.h"

#include  <kiklib/kik_mem.h>	/* alloca */
#include  "ml_ctl_loader.h"
#include  "ml_ot_layout.h"


/* --- static functions --- */

static int
combine_replacing_code(
	ml_char_t *  dst ,
	ml_char_t *  src ,
	u_int  new_code ,
	int  was_vcol
	)
{
	ml_char_t  ch ;
	u_int  code ;

	ml_char_init( &ch) ;
	ml_char_copy( &ch , src) ;

	code = ml_char_code( &ch) ;

	if( ( 0x900 <= code && code <= 0xd7f) ||
	    ( code == 0 && was_vcol))
	{
		ml_char_set_cs( &ch , ISO10646_UCS4_1_V) ;
		was_vcol = 1 ;
	}
	else
	{
		ml_char_set_cs( &ch , ISO10646_UCS4_1) ;
		was_vcol = 0 ;
	}

	ml_char_set_code( &ch , new_code) ;
	ml_char_combine_simple( dst , &ch) ;

	return  was_vcol ;
}

static int
replace_code(
	ml_char_t *  ch ,
	u_int  new_code ,
	int  was_vcol
	)
{
	u_int  code ;

	code = ml_char_code( ch) ;

	if( ( 0x900 <= code && code <= 0xd7f) ||
	    ( code == 0 && was_vcol))
	{
		ml_char_set_cs( ch , ISO10646_UCS4_1_V) ;
		was_vcol = 1 ;
	}
	else
	{
		ml_char_set_cs( ch , ISO10646_UCS4_1) ;
		was_vcol = 0 ;
	}

	ml_char_set_code( ch , new_code) ;

	return  was_vcol ;
}


/* --- global functions --- */

#ifndef  NO_DYNAMIC_LOAD_CTL

#ifdef  __APPLE__
u_int  ml_shape_arabic( ml_char_t * , u_int , ml_char_t * , u_int) __attribute__((weak)) ;
u_int16_t  ml_is_arabic_combining( ml_char_t * , ml_char_t * , ml_char_t *) __attribute__((weak)) ;
u_int  ml_shape_iscii( ml_char_t * , u_int , ml_char_t * , u_int) __attribute__((weak)) ;
#endif

u_int
ml_shape_arabic(
	ml_char_t *  dst ,
	u_int  dst_len ,
	ml_char_t *  src ,
	u_int  src_len
	)
{
	u_int (*func)( ml_char_t *  dst , u_int  dst_len , ml_char_t *  src , u_int  src_len) ;

	if( ! (func = ml_load_ctl_bidi_func( ML_SHAPE_ARABIC)))
	{
		return  0 ;
	}

	return  (*func)( dst , dst_len , src , src_len) ;
}

u_int16_t
ml_is_arabic_combining(
	ml_char_t *  prev2 ,		/* can be NULL */
	ml_char_t *  prev ,		/* must be ISO10646_UCS4_1 character */
	ml_char_t *  ch			/* must be ISO10646_UCS4_1 character */
	)
{
	u_int16_t (*func)( ml_char_t * , ml_char_t * , ml_char_t *) ;

	if( ! (func = ml_load_ctl_bidi_func( ML_IS_ARABIC_COMBINING)))
	{
		return  0 ;
	}

	return  (*func)( prev2 , prev , ch) ;
}

u_int
ml_shape_iscii(
	ml_char_t *  dst ,
	u_int  dst_len ,
	ml_char_t *  src ,
	u_int  src_len
	)
{
	u_int (*func)( ml_char_t *  dst , u_int  dst_len , ml_char_t *  src , u_int  src_len) ;

	if( ! (func = ml_load_ctl_iscii_func( ML_SHAPE_ISCII)))
	{
		return  0 ;
	}

	return  (*func)( dst , dst_len , src , src_len) ;
}

#endif


#ifdef  USE_OT_LAYOUT
u_int
ml_shape_ot_layout(
	ml_char_t *  dst ,
	u_int  dst_len ,
	ml_char_t *  src ,
	u_int  src_len ,
	ctl_info_t  ctl_info
	)
{
	int  src_pos ;
	u_int  dst_filled ;
	u_int32_t *  ucs_buf ;
	u_int  ucs_filled ;
	u_int32_t *  shaped_buf ;
	u_int  shaped_filled ;
	ml_char_t *  ch ;
	ml_char_t *  dst_shaped ;
	u_int  count ;
	ml_font_t  prev_font ;
	ml_font_t  cur_font ;
	void *  xfont ;
	ml_char_t *  comb ;
	u_int  comb_size ;
	int  src_pos_mark ;
	int  was_vcol ;

	if( ( ucs_buf = alloca( src_len * (MAX_COMB_SIZE + 1) * sizeof(*ucs_buf))) == NULL)
	{
		return  0 ;
	}

#define  DST_LEN  (dst_len * (MAX_COMB_SIZE + 1))
	if( ( shaped_buf = alloca( DST_LEN * sizeof(*ucs_buf))) == NULL)
	{
		return  0 ;
	}

	dst_filled = 0 ;
	ucs_filled = 0 ;
	dst_shaped = NULL ;
	prev_font = UNKNOWN_CS ;
	xfont = NULL ;
	for( src_pos = 0 ; src_pos < src_len ; src_pos ++)
	{
		ch = &src[src_pos] ;
		cur_font = ml_char_font( ch) ;

		if( FONT_CS(cur_font) == US_ASCII)
		{
			cur_font &= ~US_ASCII ;
			cur_font |= ISO10646_UCS4_1 ;
		}

		if( prev_font != cur_font)
		{
			if( ucs_filled)
			{
				shaped_filled = ml_ot_layout_shape( xfont , shaped_buf , DST_LEN ,
							NULL , ucs_buf , ucs_filled) ;

				/*
				 * If EOL char is a ot_layout byte which presents two ot_layouts and
				 * its second ot_layout is out of screen, 'shaped_filled' is greater
				 * than 'dst + dst_len - dst_shaped'.
				 */
				if( shaped_filled > dst + dst_len - dst_shaped)
				{
					shaped_filled = dst + dst_len - dst_shaped ;
				}

			#ifdef  __DEBUG
				{
					int  i ;

					for( i = 0 ; i < ucs_filled ; i ++)
					{
						kik_msg_printf( "%.2x " , ucs_buf[i]) ;
					}
					kik_msg_printf( "=>\n") ;

					for( i = 0 ; i < shaped_filled ; i ++)
					{
						kik_msg_printf( "%.2x " , shaped_buf[i]) ;
					}
					kik_msg_printf( "\n") ;
				}
			#endif

				was_vcol = 0 ;
				for( count = 0 ; count < shaped_filled ; dst_shaped ++)
				{
					was_vcol = replace_code( dst_shaped ,
							shaped_buf[count++] , was_vcol) ;

					comb = ml_get_combining_chars( &src[src_pos_mark++] ,
							&comb_size) ;
					for( ; comb_size > 0 ; comb_size -- , comb++)
					{
						if( ml_char_is_null( comb))
						{
							was_vcol = combine_replacing_code(
									dst_shaped , comb ,
									shaped_buf[count++] ,
									was_vcol) ;
						}
					}
				}

				dst_filled = dst_shaped - dst ;

				ucs_filled = 0 ;
				dst_shaped = NULL ;
			}

			if( FONT_CS(cur_font) == ISO10646_UCS4_1)
			{
				xfont = ml_ot_layout_get_font( ctl_info.ot_layout->term , cur_font) ;
			}
			else
			{
				xfont = NULL ;
			}
		}

		prev_font = cur_font ;

		if( xfont)
		{
			if( dst_shaped == NULL)
			{
				dst_shaped = &dst[dst_filled] ;
				src_pos_mark = src_pos ;
			}

			if( ! ml_char_is_null( ch))
			{
				ucs_buf[ucs_filled ++] = ml_char_code( ch) ;

				comb = ml_get_combining_chars( ch , &comb_size) ;
				for( count = 0 ; count < comb_size ; count ++)
				{
					if( ! ml_char_is_null( &comb[count]))
					{
						ucs_buf[ucs_filled ++] =
							ml_char_code( &comb[count]) ;
					}
				}
			}

			ml_char_copy( &dst[dst_filled ++] , ml_get_base_char( ch)) ;

			if( dst_filled >= dst_len)
			{
				break ;
			}
		}
		else
		{
			ml_char_copy( &dst[dst_filled ++] , ch) ;

			if( dst_filled >= dst_len)
			{
				return  dst_filled ;
			}
		}
	}

	if( ucs_filled)
	{
		shaped_filled = ml_ot_layout_shape( xfont , shaped_buf , DST_LEN ,
					NULL , ucs_buf , ucs_filled) ;

		/*
		 * If EOL char is a ot_layout byte which presents two ot_layouts and its second
		 * ot_layout is out of screen, 'shaped_filled' is greater then
		 * 'dst + dst_len - dst_shaped'.
		 */
		if( shaped_filled > dst + dst_len - dst_shaped)
		{
			shaped_filled = dst + dst_len - dst_shaped ;
		}

		was_vcol = 0 ;
		for( count = 0 ; count < shaped_filled ; dst_shaped ++)
		{
			was_vcol = replace_code( dst_shaped , shaped_buf[count++] ,
					was_vcol) ;

			comb = ml_get_combining_chars( &src[src_pos_mark++] , &comb_size) ;
			for( ; comb_size > 0 ; comb_size -- , comb++)
			{
				if( ml_char_is_null( comb))
				{
					was_vcol = combine_replacing_code( dst_shaped ,
							comb , shaped_buf[count++] ,
							was_vcol) ;
				}
			}
		}

		dst_filled = dst_shaped - dst ;
	}

	return  dst_filled ;
}
#endif
