/*
 *	$Id$
 */

#include  "ml_shape.h"

#include  <kiklib/kik_mem.h>	/* alloca */
#include  "ml_ctl_loader.h"
#include  "ml_gsub.h"


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


#ifdef  USE_GSUB
u_int
ml_shape_gsub(
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
	u_int32_t *  gsub_buf ;
	u_int  gsub_filled ;
	ml_char_t *  ch ;
	ml_char_t *  dst_shaped ;
	u_int  count ;
	ml_font_t  prev_font ;
	ml_font_t  cur_font ;
	void *  xfont ;

	if( ( ucs_buf = alloca( src_len * (MAX_COMB_SIZE + 1) * sizeof(*ucs_buf))) == NULL)
	{
		return  0 ;
	}

#define  DST_LEN  (dst_len * (MAX_COMB_SIZE + 1))
	if( ( gsub_buf = alloca( DST_LEN * sizeof(*ucs_buf))) == NULL)
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
			cur_font &= ~MAX_CHARSET ;
			cur_font |= ISO10646_UCS4_1 ;
		}

		if( prev_font != cur_font)
		{
			if( ucs_filled)
			{
				gsub_filled = ml_gsub_shape( xfont , gsub_buf , DST_LEN ,
							NULL , ucs_buf , ucs_filled) ;

				/*
				 * If EOL char is a gsub byte which presents two gsubs and
				 * its second gsub is out of screen, 'gsub_filled' is greater
				 * than 'dst + dst_len - dst_shaped'.
				 */
				if( gsub_filled > dst + dst_len - dst_shaped)
				{
					gsub_filled = dst + dst_len - dst_shaped ;
				}

			#ifdef  __DEBUG
				{
					int  i ;

					for( i = 0 ; i < ucs_filled ; i ++)
					{
						kik_msg_printf( "%.2x " , ucs_buf[i]) ;
					}
					kik_msg_printf( "=>\n") ;

					for( i = 0 ; i < gsub_filled ; i ++)
					{
						kik_msg_printf( "%.2x " , gsub_buf[i]) ;
					}
					kik_msg_printf( "\n") ;
				}
			#endif

				for( count = 0 ; count < gsub_filled ; count ++)
				{
					ml_char_set_cs( dst_shaped , ISO10646_UCS4_1) ;
					ml_char_set_code( dst_shaped ++ , gsub_buf[count]) ;
				}

				ucs_filled = 0 ;
				dst_shaped = NULL ;
			}

			if( FONT_CS(cur_font) == ISO10646_UCS4_1)
			{
				xfont = ml_gsub_get_font( ctl_info.gsub->term , cur_font) ;
			}
			else
			{
				xfont = NULL ;
			}
		}

		prev_font = cur_font ;

		if( xfont)
		{
			ml_char_t *  comb ;
			u_int  comb_size ;

			if( dst_shaped == NULL)
			{
				dst_shaped = &dst[dst_filled] ;
			}

			if( ! ml_char_is_null( ch))
			{
				ucs_buf[ucs_filled ++] = ml_char_code( ch) ;

				comb = ml_get_combining_chars( ch , &comb_size) ;
				for( count = 0 ; count < comb_size ; count ++)
				{
					ucs_buf[ucs_filled ++] =
						ml_char_code( &comb[count]) ;
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
		gsub_filled = ml_gsub_shape( xfont , gsub_buf , DST_LEN ,
					NULL , ucs_buf , ucs_filled) ;

		/*
		 * If EOL char is a gsub byte which presents two gsubs and its second
		 * gsub is out of screen, 'gsub_filled' is greater then
		 * 'dst + dst_len - dst_shaped'.
		 */
		if( gsub_filled > dst + dst_len - dst_shaped)
		{
			gsub_filled = dst + dst_len - dst_shaped ;
		}

		for( count = 0 ; count < gsub_filled ; count ++)
		{
			ml_char_set_cs( dst_shaped + count , ISO10646_UCS4_1) ;
			ml_char_set_code( dst_shaped + count , gsub_buf[count]) ;
		}
	}

	return  dst_filled ;
}
#endif
