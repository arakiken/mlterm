/*
 *	$Id$
 */

#include  "../ml_shape.h"

#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_debug.h>	/* kik_msg_printf */
#include  <mkf/mkf_char.h>
#include  "ml_iscii.h"


/* --- global functions --- */

u_int
ml_shape_iscii(
	ml_char_t *  dst ,
	u_int  dst_len ,
	ml_char_t *  src ,
	u_int  src_len
	)
{
	int  src_pos ;
	u_int  dst_filled ;
	u_char *  iscii_buf ;
	u_int  iscii_filled ;
	u_char *  font_buf ;
	u_int  font_filled ;
	ml_char_t *  ch ;
	ml_char_t *  dst_shaped ;
	u_int  count ;
	mkf_charset_t  cs ;

	if( ( iscii_buf = alloca( src_len * (MAX_COMB_SIZE + 1))) == NULL)
	{
		return  0 ;
	}

#define  DST_LEN  (dst_len * (MAX_COMB_SIZE + 1))
	if( ( font_buf = alloca( DST_LEN)) == NULL)
	{
		return  0 ;
	}

	dst_filled = 0 ;
	iscii_filled = 0 ;
	dst_shaped = NULL ;
	cs = UNKNOWN_CS ;
	for( src_pos = 0 ; src_pos < src_len ; src_pos ++)
	{
		ch = &src[src_pos] ;

		if( cs != ml_char_cs( ch))
		{
			if( iscii_filled)
			{
				iscii_buf[iscii_filled] = '\0' ;
				font_filled = ml_iscii_shape( cs ,
						font_buf , DST_LEN , iscii_buf) ;

				/*
				 * If EOL char is a iscii byte which presents two glyphs and
				 * its second glyph is out of screen, 'font_filled' is greater
				 * than 'dst + dst_len - dst_shaped'.
				 */
				if( font_filled > dst + dst_len - dst_shaped)
				{
					font_filled = dst + dst_len - dst_shaped ;
				}
		
			#ifdef  __DEBUG
				{
					int  i ;
					
					for( i = 0 ; i < iscii_filled ; i ++)
					{
						kik_msg_printf( "%.2x " , iscii_buf[i]) ;
					}
					kik_msg_printf( "=>\n") ;
					
					for( i = 0 ; i < font_filled ; i ++)
					{
						kik_msg_printf( "%.2x " , font_buf[i]) ;
					}
					kik_msg_printf( "\n") ;
				}
			#endif

				for( count = 0 ; count < font_filled ; count ++)
				{
					ml_char_set_code( dst_shaped ++ , font_buf[count]) ;
				}

				iscii_filled = 0 ;
				dst_shaped = NULL ;
			}
		}

		cs = ml_char_cs( ch) ;
		
		if( IS_ISCII( cs))
		{
			ml_char_t *  comb ;
			u_int  comb_size ;
			
			if( dst_shaped == NULL)
			{
				dst_shaped = &dst[dst_filled] ;
			}

			if( ! ml_char_is_null( ch))
			{
				iscii_buf[iscii_filled ++] = ml_char_code( ch) ;
				
				comb = ml_get_combining_chars( ch , &comb_size) ;
				for( count = 0 ; count < comb_size ; count ++)
				{
					iscii_buf[iscii_filled ++] =
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

	if( iscii_filled)
	{
		iscii_buf[iscii_filled] = '\0' ;
		font_filled = ml_iscii_shape( cs , font_buf , DST_LEN , iscii_buf) ;

		/*
		 * If EOL char is a iscii byte which presents two glyphs and its second
		 * glyph is out of screen, 'font_filled' is greater then
		 * 'dst + dst_len - dst_shaped'.
		 */
		if( font_filled > dst + dst_len - dst_shaped)
		{
			font_filled = dst + dst_len - dst_shaped ;
		}
		
		for( count = 0 ; count < font_filled ; count ++)
		{
			ml_char_copy( dst_shaped + count , dst_shaped) ;
			ml_char_set_code( dst_shaped + count , font_buf[count]) ;
		}
	}

	return  dst_filled ;
}
