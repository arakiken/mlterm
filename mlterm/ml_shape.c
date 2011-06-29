/*
 *	$Id$
 */

#include  "ml_shape.h"

#include  <string.h>		/* strncpy */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_debug.h>	/* kik_msg_printf */
#include  <mkf/mkf_char.h>	/* mkf_bytes_to_int */


typedef struct arabic_present
{
	u_int16_t  base_arabic ;

	/* presentations. right or left is visual order's one. */
	u_int16_t  no_joining_present ;
	u_int16_t  right_joining_present ;
	u_int16_t  left_joining_present ;
	u_int16_t  both_joining_present ;
	
} arabic_present_t ;

typedef struct arabic_comb
{
	/* first or second is logical order's one */
	u_int16_t  first ;
	u_int16_t  second ;
	u_int16_t  comb ;
	u_int16_t  comb_right ;

} arabic_comb_t ;


/* --- static variables --- */

#ifdef  USE_FRIBIDI

static arabic_present_t  arabic_present_table[] =
{
	{ 0x0621 , 0xFE80 , 0x0000 , 0x0000 , 0x0000 , } ,
	{ 0x0622 , 0xFE81 , 0xFE82 , 0x0000 , 0x0000 , } ,
	{ 0x0623 , 0xFE83 , 0xFE84 , 0x0000 , 0x0000 , } ,
	{ 0x0624 , 0xFE85 , 0xFE86 , 0x0000 , 0x0000 , } ,
	{ 0x0625 , 0xFE87 , 0xFE88 , 0x0000 , 0x0000 , } ,
	{ 0x0626 , 0xFE89 , 0xFE8A , 0xFE8B , 0xFE8C , } ,
	{ 0x0627 , 0xFE8D , 0xFE8E , 0x0000 , 0x0000 , } ,
	{ 0x0628 , 0xFE8F , 0xFE90 , 0xFE91 , 0xFE92 , } ,
	{ 0x0629 , 0xFE93 , 0xFE94 , 0x0000 , 0x0000 , } ,
	{ 0x062A , 0xFE95 , 0xFE96 , 0xFE97 , 0xFE98 , } ,
	{ 0x062B , 0xFE99 , 0xFE9A , 0xFE9B , 0xFE9C , } ,
	{ 0x062C , 0xFE9D , 0xFE9E , 0xFE9F , 0xFEA0 , } ,
	{ 0x062D , 0xFEA1 , 0xFEA2 , 0xFEA3 , 0xFEA4 , } ,
	{ 0x062E , 0xFEA5 , 0xFEA6 , 0xFEA7 , 0xFEA8 , } ,
	{ 0x062F , 0xFEA9 , 0xFEAA , 0x0000 , 0x0000 , } ,
	{ 0x0630 , 0xFEAB , 0xFEAC , 0x0000 , 0x0000 , } ,
	{ 0x0631 , 0xFEAD , 0xFEAE , 0x0000 , 0x0000 , } ,
	{ 0x0632 , 0xFEAF , 0xFEB0 , 0x0000 , 0x0000 , } ,
	{ 0x0633 , 0xFEB1 , 0xFEB2 , 0xFEB3 , 0xFEB4 , } ,
	{ 0x0634 , 0xFEB5 , 0xFEB6 , 0xFEB7 , 0xFEB8 , } ,
	{ 0x0635 , 0xFEB9 , 0xFEBA , 0xFEBB , 0xFEBC , } ,
	{ 0x0636 , 0xFEBD , 0xFEBE , 0xFEBF , 0xFEC0 , } ,
	{ 0x0637 , 0xFEC1 , 0xFEC2 , 0xFEC3 , 0xFEC4 , } ,
	{ 0x0638 , 0xFEC5 , 0xFEC6 , 0xFEC7 , 0xFEC8 , } ,
	{ 0x0639 , 0xFEC9 , 0xFECA , 0xFECB , 0xFECC , } ,
	{ 0x063A , 0xFECD , 0xFECE , 0xFECF , 0xFED0 , } ,
	{ 0x0640 , 0x0640 , 0x0640 , 0x0640 , 0x0640 , } ,
	{ 0x0641 , 0xFED1 , 0xFED2 , 0xFED3 , 0xFED4 , } ,
	{ 0x0642 , 0xFED5 , 0xFED6 , 0xFED7 , 0xFED8 , } ,
	{ 0x0643 , 0xFED9 , 0xFEDA , 0xFEDB , 0xFEDC , } ,
	{ 0x0644 , 0xFEDD , 0xFEDE , 0xFEDF , 0xFEE0 , } ,
	{ 0x0645 , 0xFEE1 , 0xFEE2 , 0xFEE3 , 0xFEE4 , } ,
	{ 0x0646 , 0xFEE5 , 0xFEE6 , 0xFEE7 , 0xFEE8 , } ,
	{ 0x0647 , 0xFEE9 , 0xFEEA , 0xFEEB , 0xFEEC , } ,
	{ 0x0648 , 0xFEED , 0xFEEE , 0x0000 , 0x0000 , } ,
	{ 0x0649 , 0xFEEF , 0xFEF0 , 0xFBE8 , 0xFBE9 , } ,
	{ 0x064A , 0xFEF1 , 0xFEF2 , 0xFEF3 , 0xFEF4 , } ,
	{ 0x0671 , 0xFB50 , 0xFB51 , 0x0000 , 0x0000 , } ,
	{ 0x0679 , 0xFB66 , 0xFB67 , 0xFB68 , 0xFB69 , } ,
	{ 0x067A , 0xFB5E , 0xFB5F , 0xFB60 , 0xFB61 , } ,
	{ 0x067B , 0xFB52 , 0xFB53 , 0xFB54 , 0xFB55 , } ,
	{ 0x067E , 0xFB56 , 0xFB57 , 0xFB58 , 0xFB59 , } ,
	{ 0x067F , 0xFB62 , 0xFB63 , 0xFB64 , 0xFB65 , } ,
	{ 0x0680 , 0xFB5A , 0xFB5B , 0xFB5C , 0xFB5D , } ,
	{ 0x0683 , 0xFB76 , 0xFB77 , 0xFB78 , 0xFB79 , } ,
	{ 0x0684 , 0xFB72 , 0xFB73 , 0xFB74 , 0xFB75 , } ,
	{ 0x0686 , 0xFB7A , 0xFB7B , 0xFB7C , 0xFB7D , } ,
	{ 0x0687 , 0xFB7E , 0xFB7F , 0xFB80 , 0xFB81 , } ,
	{ 0x0688 , 0xFB88 , 0xFB89 , 0x0000 , 0x0000 , } ,
	{ 0x068C , 0xFB84 , 0xFB85 , 0x0000 , 0x0000 , } ,
	{ 0x068D , 0xFB82 , 0xFB83 , 0x0000 , 0x0000 , } ,
	{ 0x068E , 0xFB86 , 0xFB87 , 0x0000 , 0x0000 , } ,
	{ 0x0691 , 0xFB8C , 0xFB8D , 0x0000 , 0x0000 , } ,
	{ 0x0698 , 0xFB8A , 0xFB8B , 0x0000 , 0x0000 , } ,
	{ 0x06A4 , 0xFB6A , 0xFB6B , 0xFB6C , 0xFB6D , } ,
	{ 0x06A6 , 0xFB6E , 0xFB6F , 0xFB70 , 0xFB71 , } ,
	{ 0x06A9 , 0xFB8E , 0xFB8F , 0xFB90 , 0xFB91 , } ,
	{ 0x06AD , 0xFBD3 , 0xFBD4 , 0xFBD5 , 0xFBD6 , } ,
	{ 0x06AF , 0xFB92 , 0xFB93 , 0xFB94 , 0xFB95 , } ,
	{ 0x06B1 , 0xFB9A , 0xFB9B , 0xFB9C , 0xFB9D , } ,
	{ 0x06B3 , 0xFB96 , 0xFB97 , 0xFB98 , 0xFB99 , } ,
	{ 0x06BB , 0xFBA0 , 0xFBA1 , 0xFBA2 , 0xFBA3 , } ,
	{ 0x06BE , 0xFBAA , 0xFBAB , 0xFBAC , 0xFBAD , } ,
	{ 0x06C0 , 0xFBA4 , 0xFBA5 , 0x0000 , 0x0000 , } ,
	{ 0x06C1 , 0xFBA6 , 0xFBA7 , 0xFBA8 , 0xFBA9 , } ,
	{ 0x06C5 , 0xFBE0 , 0xFBE1 , 0x0000 , 0x0000 , } ,
	{ 0x06C6 , 0xFBD9 , 0xFBDA , 0x0000 , 0x0000 , } ,
	{ 0x06C7 , 0xFBD7 , 0xFBD8 , 0x0000 , 0x0000 , } ,
	{ 0x06C8 , 0xFBDB , 0xFBDC , 0x0000 , 0x0000 , } ,
	{ 0x06C9 , 0xFBE2 , 0xFBE3 , 0x0000 , 0x0000 , } ,
	{ 0x06CB , 0xFBDE , 0xFBDF , 0x0000 , 0x0000 , } ,
	{ 0x06CC , 0xFBFC , 0xFBFD , 0xFBFE , 0xFBFF , } ,
	{ 0x06D0 , 0xFBE4 , 0xFBE5 , 0xFBE6 , 0xFBE7 , } ,
	{ 0x06D2 , 0xFBAE , 0xFBAF , 0x0000 , 0x0000 , } ,
	{ 0x06D3 , 0xFBB0 , 0xFBB1 , 0x0000 , 0x0000 , } ,
	
} ;

static arabic_comb_t  arabic_comb_table[] =
{
	{ 0x0644 , 0x0622 , 0xFEF5 , 0xFEF6 , } ,
	{ 0x0644 , 0x0623 , 0xFEF7 , 0xFEF8 , } ,
	{ 0x0644 , 0x0625 , 0xFEF9 , 0xFEFA , } ,
	{ 0x0644 , 0x0627 , 0xFEFB , 0xFEFC , } ,

} ;

#endif	/* USE_FRIBIDI */


/* --- static functions --- */

#ifdef  USE_FRIBIDI

static arabic_present_t *
get_arabic_present(
	ml_char_t *  ch
	)
{
	u_int16_t  code ;
	u_char *  bytes ;
	int  count ;

	if( ml_char_cs(ch) == ISO10646_UCS4_1)
	{
		bytes = ml_char_bytes(ch) ;
		code = (bytes[2] << 8) + bytes[3] ;
	}
	else
	{
		return  NULL ;
	}

	for( count = 0 ;
		count < sizeof( arabic_present_table) / sizeof( arabic_present_table[0]) ;
		count ++)
	{
		if( arabic_present_table[count].base_arabic == code)
		{
			return  &arabic_present_table[count] ;
		}
	}

	return  NULL ;
}

/*
 * 'src' characters are right to left (visual) order.
 */
static u_int
shape_arabic(
	ml_shape_t *  shape ,
	ml_char_t *  dst ,
	u_int  dst_len ,
	ml_char_t *  src ,
	u_int  src_len
	)
{
	int  count ;
	arabic_present_t **  list ;
	u_int16_t  code ;
	ml_char_t *  comb ;
	ml_char_t *  cur ;
	ml_char_t *  next ;		/* the same as 'prev' in logical order */
	u_int  size ;

	if( ( list = alloca( sizeof( arabic_present_t*) * (src_len + 2))) == NULL)
	{
		return  0 ;
	}

	/* head is NULL */
	*(list ++) = NULL ;

	for( count = 0 ; count < src_len ; count ++)
	{
		list[count] = get_arabic_present( &src[count]) ;
	}

	/* tail is NULL */
	list[count] = NULL ;

	cur = src ;
	
	if( src_len <= 1)
	{
		next = NULL ;
	}
	else
	{
		next = cur + 1 ;
	}

	for( count = 0 ; count < src_len && count < dst_len ; count ++)
	{
		comb = ml_get_combining_chars( cur , &size) ;

		if( comb && ( code = ml_is_arabic_combining(
					count + 1 >= src_len ? NULL : &src[count + 1] ,
					ml_get_base_char( cur) , comb)))
		{
			u_char  bytes[4] ;

			ml_char_copy( &dst[count] , ml_get_base_char(cur)) ;
			ml_char_set_bytes( &dst[count] ,
				mkf_int_to_bytes( bytes , ml_char_size(cur) , code)) ;
		}
		else if( list[count])
		{
		#if  0
			/*
			 * Tanween characters combining their proceeded characters will
			 * be ignored by ml_get_base_char(cur).
			 */
			ml_char_copy( &dst[count] , ml_get_base_char(cur)) ;
		#else
			ml_char_copy( &dst[count] , cur) ;
		#endif

			if( list[count - 1] && list[count - 1]->right_joining_present)
			{
				if( ( list[count + 1] && list[count + 1]->left_joining_present) &&
					! ( next && (comb = ml_get_combining_chars( next , &size)) &&
					ml_is_arabic_combining(
						count + 2 >= src_len ? NULL : &src[count + 2] ,
						ml_get_base_char( next) , comb)) )
				{
					if( list[count]->both_joining_present)
					{
						code = list[count]->both_joining_present ;
					}
					else if( list[count]->left_joining_present)
					{
						code = list[count]->left_joining_present ;
					}
					else if( list[count]->right_joining_present)
					{
						code = list[count]->right_joining_present ;
					}
					else
					{
						code = list[count]->no_joining_present ;
					}
				}
				else if( list[count]->left_joining_present)
				{
					code = list[count]->left_joining_present ;
				}
				else
				{
					code = list[count]->no_joining_present ;
				}
			}
			else if( ( list[count + 1] && list[count + 1]->left_joining_present) &&
				! ( next && ( comb = ml_get_combining_chars( next , &size)) &&
					ml_is_arabic_combining(
						count + 2 >= src_len ? NULL : &src[count + 2] ,
						ml_get_base_char( next) , comb)) )
			{
				if( list[count]->right_joining_present)
				{
					code = list[count]->right_joining_present ;
				}
				else
				{
					code = list[count]->no_joining_present ;
				}
			}
			else
			{
				code = list[count]->no_joining_present ;
			}

			if( code)
			{
				u_char *  bytes ;
				
				bytes = ml_char_bytes( &dst[count]) ;

				bytes[0] = 0x0 ;
				bytes[1] = 0x0 ;
				bytes[2] = (code >> 8) & 0xff ;
				bytes[3] = code & 0xff ;
			}
		}
		else
		{
			ml_char_copy( &dst[count] , cur) ;
		}

		cur = next ;
		next ++ ;
	}

	return  count ;
}

static int
arabic_delete(
	ml_shape_t *  shape
	)
{
	free( shape) ;

	return  1 ;
}

#endif	/* USE_FRIBIDI */


#ifdef  USE_IND

static u_int
shape_iscii(
	ml_shape_t *  shape ,
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
					ml_char_set_bytes( dst_shaped ++ , font_buf + count) ;
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
				iscii_buf[iscii_filled ++] = ml_char_bytes( ch)[0] ;
				
				comb = ml_get_combining_chars( ch , &comb_size) ;
				for( count = 0 ; count < comb_size ; count ++)
				{
					iscii_buf[iscii_filled ++] =
						ml_char_bytes( &comb[count])[0] ;
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
			ml_char_set_bytes( dst_shaped + count , font_buf + count) ;
		}
	}

	return  dst_filled ;
}

static int
iscii_delete(
	ml_shape_t *  shape
	)
{
	free( shape) ;

	return  1 ;
}

#endif	/* USE_IND */


/* --- global functions --- */

ml_shape_t *
ml_arabic_shape_new(void)
{
#ifdef  USE_FRIBIDI
	ml_shape_t *  shape ;
	
	if( ( shape = malloc( sizeof( ml_shape_t))) == NULL)
	{
		return  NULL ;
	}

	shape->shape = shape_arabic ;
	shape->delete = arabic_delete ;

	return  shape ;
#else
	return  NULL ;
#endif
}

u_int16_t
ml_is_arabic_combining(
	ml_char_t *  prev2 ,		/* can be NULL */
	ml_char_t *  prev ,		/* must be ISO10646_UCS4_1 character */
	ml_char_t *  ch			/* must be ISO10646_UCS4_1 character */
	)
{
#ifdef  USE_FRIBIDI
	ml_char_t *  seq[4] ;		/* reverse order */
	u_int16_t  ucs_seq[4] ;		/* reverse order */
	int  count ;
	int  prev2_is_comb ;
	arabic_present_t *  prev2_present ;

	seq[0] = ch ;
	seq[1] = prev ;
	seq[2] = prev2 ;
	seq[3] = NULL ;

	if( prev2)
	{
		ml_char_t *  comb ;
		u_int  size ;

		prev2_present = get_arabic_present( prev2) ;

		if( ( comb = ml_get_combining_chars( prev2 , &size)))
		{
			seq[3] = ml_get_base_char( prev2) ;
			seq[2] = comb ;
		}
	}
	else
	{
		prev2_present = NULL ;
	}
	
	for( count = 0 ; count < 4 ; count ++)
	{
		if( seq[count] && ml_char_cs(seq[count]) == ISO10646_UCS4_1)
		{
			ucs_seq[count] = mkf_bytes_to_int(ml_char_bytes(seq[count]), 4) ;
		}
		else if ( count < 2)
		{
			/* Ignore the previous combinational/two characters */
			
			return  0 ;
		}
		else
		{
			ucs_seq[count] = 0 ;
		}
	}

	prev2_is_comb = 0 ;

	if( seq[3] && prev2_present)
	{
		/* See if the current character was proceeded by combinational character */
		for(count = 0 ;
		    count < sizeof(arabic_comb_table) / sizeof(arabic_comb_table[0]) ;
		    count ++)
		{
			if( ( ucs_seq[3] == arabic_comb_table[count].first &&
				ucs_seq[2] == arabic_comb_table[count].second))
			{
				prev2_is_comb = 1 ;

				break ;
			}
		}
	}

	/* Shape the current combinational character */
	for(count = 0 ;
	    count < sizeof(arabic_comb_table) / sizeof(arabic_comb_table[0]) ;
	    count ++)
	{
		if( ucs_seq[1] == arabic_comb_table[count].first &&
			ucs_seq[0] == arabic_comb_table[count].second)
		{
			if( ! prev2_is_comb && prev2_present &&
				prev2_present->left_joining_present)
			{
				return  arabic_comb_table[count].comb_right ;
			}
			else
			{
				return  arabic_comb_table[count].comb ;
			}
		}
	}
#endif	/* USE_FRIBIDI */

	return  0 ;
}

ml_shape_t *
ml_iscii_shape_new(void)
{
#ifdef  USE_IND
	ml_shape_t *  shape ;
	
	if( ( shape = malloc( sizeof( ml_shape_t))) == NULL)
	{
		return  NULL ;
	}

	shape->shape = shape_iscii ;
	shape->delete = iscii_delete ;

	return  shape ;
#else
	return  NULL ;
#endif
}
