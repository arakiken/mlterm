/*
 *	$Id$
 */

#include  "ml_char.h"

#include  <string.h>		/* memset/memcpy */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>	/* K_MIN */


#define  UNDERLINE_STYLE(attr)  (((attr) >> 21) & 0x3)

#define  IS_ZEROWIDTH(attr)  ((attr) & (0x1 << 20))

/* Combination of UNICODE_AREA, IS_ITALIC, IS_BOLD, IS_FULLWIDTH and CHARSET */
#define  MLFONT(attr) \
	((attr) & (0x1 << 17)) ?	/* is unicode area cs or not */ \
	((((attr) >> 5) & 0xe00) | ISO10646_UCS4_1 | (((attr) << 7) & 0x1ff000)) : \
	(((attr) >> 5) & 0xfff)

#define  IS_ITALIC(attr)  ((attr) & (0x1 << 16))
#define  IS_BOLD(attr)  ((attr) & (0x1 << 15))
#define  IS_FULLWIDTH(attr)  ((attr) & (0x1 << 14))
#define  CHARSET(attr) \
	((attr) & (0x1 << 17)) ?	/* is unicode area cs or not */ \
	ISO10646_UCS4_1 : \
	(((attr) >> 5) & 0x1ff)

#define  IS_REVERSED(attr)  ((attr) & (0x1 << 4))
#define  REVERSE_COLOR(attr) ((attr) |= (0x1 << 4))
#define  RESTORE_COLOR(attr) ((attr) &= ~(0x1 << 4))

#define  IS_CROSSED_OUT(attr)  ((attr) & (0x1 << 3))

#define  IS_COMB(attr)  ((attr) & (0x1 << 2))

#define  IS_COMB_TRAILING(attr)  ((attr) & (0x1 << 1))
#define  SET_COMB_TRAILING(attr)  ((attr) |= (0x1 << 1))
#define  UNSET_COMB_TRAILING(attr)  ((attr) &= 0xfffffd)

#define  IS_SINGLE_CH(attr)  ((attr) & 0x1)
#define  USE_MULTI_CH(attr)  ((attr) &= 0xfffffe)
#define  UNUSE_MULTI_CH(attr)  ((attr) |= 0x1)

#define  COMPOUND_ATTR(charset,is_zerowidth,is_fullwidth,is_bold,is_italic,is_unicode_area_cs,underline_style,is_crossed_out,is_comb) \
	( ((underline_style) << 21) | ((is_zerowidth) << 20) | ((is_unicode_area_cs) << 17) | \
	  ((is_italic) << 16) | ((is_bold) << 15) | ((is_fullwidth) << 14) | ((charset) << 5) | \
	  ( 0x0 << 4) |  ((is_crossed_out) << 3) | ((is_comb) << 2) | ( 0x0 << 1) | 0x1)


/* --- static variables --- */

static int  use_multi_col_char = 1 ;

static struct
{
	u_int32_t  min ;
	u_int32_t  max ;

} *  unicode_areas ;
static u_int  num_of_unicode_areas ;


/* --- static functions --- */

inline static u_int
get_comb_size(
	ml_char_t *  multi_ch
	)
{
	u_int  size ;

	size = 0 ;
	while( IS_COMB_TRAILING( multi_ch->u.ch.attr))
	{
		size ++ ;
		multi_ch ++ ;
	}

	return  size ;
}


/* --- global functions --- */

int
ml_set_use_multi_col_char(
	int  use_it
	)
{
	use_multi_col_char = use_it ;

	return  1 ;
}

ml_font_t
ml_char_get_unicode_area_font(
	u_int32_t  min ,
	u_int32_t  max
	)
{
	u_int  idx ;
	void *  p ;

	for( idx = num_of_unicode_areas ; idx > 0 ; idx--)
	{
		if( min == unicode_areas[idx - 1].min &&
		    max == unicode_areas[idx - 1].max)
		{
			return  ISO10646_UCS4_1 | (idx << 12) ;
		}
	}

	if( num_of_unicode_areas == 511 /* Max is 2^9-1 */ ||
	    ! ( p = realloc( unicode_areas ,
			sizeof(*unicode_areas) * (num_of_unicode_areas + 1))))
	{
		kik_msg_printf( "No more unicode areas.\n") ;

		return  UNKNOWN_CS ;
	}

	unicode_areas = p ;
	unicode_areas[num_of_unicode_areas].min = min ;
	unicode_areas[num_of_unicode_areas++].max = max ;

	return  ISO10646_UCS4_1 | (num_of_unicode_areas << 12) ;
}


/*
 * character functions
 */

int
ml_char_init(
	ml_char_t *  ch
	)
{
	if( sizeof( ml_char_t *) != sizeof( ml_char_t))
	{
		/*ILP32*/
		memset( ch , 0 , sizeof( ml_char_t)) ;
		/* set u.ch.is_single_ch */
		ch->u.ch.attr = 0x1 ;
	}
	else
	{
		/*LP64*/
		/* LSB of multi_ch must be "is_single_ch"  */
		ch->u.multi_ch =(ml_char_t *)0x1 ;
	}

	return  1 ;
}

int
ml_char_final(
	ml_char_t *  ch
	)
{
	if( ! IS_SINGLE_CH(ch->u.ch.attr))
	{
		free( ch->u.multi_ch) ;
	}

	return  1 ;
}

int
ml_char_set(
	ml_char_t *  ch ,
	u_int32_t  code ,
	mkf_charset_t  cs ,
	int  is_fullwidth ,
	int  is_comb ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color ,
	int  is_bold ,
	int  is_italic ,
	int  underline_style ,
	int  is_crossed_out
	)
{
	u_int  idx ;
	int  is_zerowidth ;

	ml_char_final( ch) ;

	ch->u.ch.code = code ;

	if( unicode_areas && cs == ISO10646_UCS4_1)
	{
		for( idx = num_of_unicode_areas ; idx > 0 ; idx --)
		{
			if( unicode_areas[idx - 1].min <= code &&
			    code <= unicode_areas[idx - 1].max)
			{
				cs = idx ;
				break ;
			}
		}
	}
	else
	{
		idx = 0 ;
	}

#if  1
	/*
	 * 0 should be returned for all zero-width characters of Unicode,
	 * but 0 is returned for following characters alone for now.
	 * 200C;ZERO WIDTH NON-JOINER
	 * 200D;ZERO WIDTH JOINER
	 * 200E;LEFT-TO-RIGHT MARK
	 * 200F;RIGHT-TO-LEFT MARK
	 * 202A;LEFT-TO-RIGHT EMBEDDING
	 * 202B;RIGHT-TO-LEFT EMBEDDING
	 * 202C;POP DIRECTIONAL FORMATTING
	 * 202D;LEFT-TO-RIGHT OVERRIDE
	 * 202E;RIGHT-TO-LEFT OVERRIDE
	 *
	 * see is_noconv_unicode() in ml_vt100_parser.c
	 */
	if( cs == ISO10646_UCS4_1 &&
	    ( (0x200c <= code && code <= 0x200f) || (0x202a <= code && code <= 0x202e)))
	{
		is_zerowidth = 1 ;
	}
	else
#endif
	{
		is_zerowidth = 0 ;
	}

	ch->u.ch.attr = COMPOUND_ATTR(cs,is_zerowidth,is_fullwidth!=0,is_bold!=0,
				is_italic!=0,idx>0,underline_style,is_crossed_out!=0,is_comb!=0) ;
	ch->u.ch.fg_color = fg_color ;
	ch->u.ch.bg_color = bg_color ;

	return  1 ;
}

int
ml_char_combine(
	ml_char_t *  ch ,
	u_int32_t  code ,
	mkf_charset_t  cs ,
	int  is_fullwidth ,
	int  is_comb ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color ,
	int  is_bold ,
	int  is_italic ,
	int  underline_style ,
	int  is_crossed_out
	)
{
	ml_char_t *  multi_ch ;

	/*
	 * This check should be excluded, because characters whose is_comb flag
	 * (combining property of mkf) is NULL can be combined
	 * if ml_is_arabic_combining(them) returns non-NULL.
	 */
#if  0
	if( ! is_comb)
	{
		return  0 ;
	}
#endif

	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		if( IS_ZEROWIDTH(ch->u.ch.attr))
		{
			/*
			 * Zero width characters must not be combined to
			 * show string like U+09b0 + U+200c + U+09cd + U+09af correctly.
			 */
			return  0 ;
		}

		if( ( multi_ch = malloc( sizeof( ml_char_t) * 2)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
		#endif

			return  0 ;
		}
		
#if !defined(__GLIBC__)
		if( sizeof( multi_ch) >= 8 && ((long)( multi_ch) & 0x1UL) != 0)
		{
			kik_msg_printf( "Your malloc() doesn't return 2 bits aligned address."
			                "Character combining is not supported.\n") ;

			return  0 ;
		}
#endif

		ml_char_init( multi_ch) ;
		ml_char_copy( multi_ch , ch) ;
		SET_COMB_TRAILING( multi_ch->u.ch.attr) ;

		ml_char_init( multi_ch + 1) ;
		if( ml_char_set( multi_ch + 1 , code , cs , is_fullwidth , is_comb ,
			fg_color , bg_color , is_bold , is_italic , underline_style ,
			is_crossed_out) == 0)
		{
			return  0 ;
		}
	}
	else
	{
		u_int  comb_size ;

		if( IS_ZEROWIDTH(ch->u.multi_ch->u.ch.attr))
		{
			/*
			 * Zero width characters must not be combined to
			 * show string like U+09b0 + U+200c + U+09cd + U+09af correctly.
			 */
			return  0 ;
		}

		if( ( comb_size = get_comb_size( ch->u.multi_ch)) >= MAX_COMB_SIZE)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
			" This char is already combined by %d chars, so no more combined.\n",
			comb_size) ;
		#endif
		
			return  0 ;
		}
		
		if( ( multi_ch = realloc( ch->u.multi_ch ,
			sizeof( ml_char_t) * (comb_size + 2))) == NULL)
		{
			return  0 ;
		}

	#ifndef  __GLIBC__
		if( sizeof( multi_ch) >= 8 && ((long)( multi_ch) & 0x1UL) != 0)
		{
			kik_msg_printf( "Your malloc() doesn't return 2 bits aligned address."
			                "Character combining is not supported.\n") ;

			return  0 ;
		}
	#endif

		SET_COMB_TRAILING( multi_ch[comb_size].u.ch.attr) ;
		ml_char_init( multi_ch + comb_size + 1) ;
		if( ml_char_set( multi_ch + comb_size + 1 , code , cs , is_fullwidth ,
			is_comb , fg_color , bg_color , is_bold , is_italic , underline_style ,
			is_crossed_out) == 0)
		{
			return  0 ;
		}
	}

	ch->u.multi_ch = multi_ch ;
	USE_MULTI_CH(ch->u.ch.attr) ;	/* necessary for 64bit big endian */

	return  1 ;
}

int
ml_char_combine_simple(
	ml_char_t *  ch ,
	ml_char_t *  comb
	)
{
	return  ml_char_combine( ch , ml_char_code( comb) , CHARSET(comb->u.ch.attr) ,
			IS_FULLWIDTH(comb->u.ch.attr) , IS_COMB(comb->u.ch.attr) ,
			comb->u.ch.fg_color , comb->u.ch.bg_color ,
			IS_BOLD(comb->u.ch.attr) , IS_ITALIC(comb->u.ch.attr) ,
			UNDERLINE_STYLE(comb->u.ch.attr) , IS_CROSSED_OUT(comb->u.ch.attr)) ;
}

ml_char_t *
ml_get_base_char(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  ch ;
	}
	else
	{
		return  ch->u.multi_ch ;
	}
}

ml_char_t *
ml_get_combining_chars(
	ml_char_t *  ch ,
	u_int *  size
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		*size = 0 ;

		return  NULL ;
	}
	else
	{
		*size = get_comb_size( ch->u.multi_ch) ;

		return  ch->u.multi_ch + 1 ;
	}
}

/*
 * Not used for now.
 */
#if  0
int
ml_char_move(
	ml_char_t *  dst ,
	ml_char_t *  src
	)
{
	if( dst == src)
	{
		return  0 ;
	}
	
	ml_char_final( dst) ;

	memcpy( dst , src , sizeof( ml_char_t)) ;

#if  0
	/* invalidated src */
	ml_char_init( src) ;
#endif

	return  1 ;
}
#endif

int
ml_char_copy(
	ml_char_t *  dst ,
	ml_char_t *  src
	)
{
	if( dst == src)
	{
		return  0 ;
	}
	
	ml_char_final( dst) ;

	memcpy( dst , src , sizeof( ml_char_t)) ;

	if( ! IS_SINGLE_CH(src->u.ch.attr))
	{
		ml_char_t *  multi_ch ;
		u_int  comb_size ;

		comb_size = get_comb_size( src->u.multi_ch) ;
		
		if( ( multi_ch = malloc( sizeof( ml_char_t) * (comb_size + 1))) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " failed to malloc.\n") ;
		#endif

			return  0 ;
		}

		memcpy( multi_ch , src->u.multi_ch , sizeof( ml_char_t) * (comb_size + 1)) ;

		dst->u.multi_ch = multi_ch ;
		USE_MULTI_CH(dst->u.ch.attr) ;	/* necessary for 64bit big endian */
	}
	
	return  1 ;
}

u_int32_t
ml_char_code(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  ch->u.ch.code ;
	}
	else
	{
		return  ml_char_code( ch->u.multi_ch) ;
	}
}

int
ml_char_set_code(
	ml_char_t *  ch ,
	u_int32_t  code
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		ch->u.ch.code = code ;
	}
	else
	{
		ml_char_set_code( ch->u.multi_ch , code) ;
	}
	
	return  1 ;
}

mkf_charset_t
ml_char_cs(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  CHARSET(ch->u.ch.attr) ;
	}
	else
	{
		return  ml_char_cs( ch->u.multi_ch) ;
	}
}

ml_font_t
ml_char_font(
	ml_char_t *  ch
	)
{
	u_int  attr ;

	attr = ch->u.ch.attr ;

	if( IS_SINGLE_CH(attr))
	{
		return  MLFONT(attr) ;
	}
	else
	{
		return  ml_char_font( ch->u.multi_ch) ;
	}
}

/*
 * Return the number of columns when ch is shown in the screen.
 * (If ml_char_cols(ch) returns 0, nothing is shown in the screen.)
 */
u_int
ml_char_cols(
	ml_char_t *  ch
	)
{
	u_int  attr ;

	attr = ch->u.ch.attr ;

	if( IS_SINGLE_CH(attr))
	{
		if( IS_ZEROWIDTH(attr))
		{
			return  0 ;
		}
		else if( use_multi_col_char && IS_FULLWIDTH(attr))
		{
			return  2 ;
		}

		return  1 ;
	}
	else
	{
		return  ml_char_cols( ch->u.multi_ch) ;
	}
}

/*
 * 'use_multi_col_char' not concerned.
 */
int
ml_char_is_fullwidth(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  IS_FULLWIDTH(ch->u.ch.attr) ;
	}
	else
	{
		return  ml_char_is_fullwidth( ch->u.multi_ch) ;
	}
}

int
ml_char_is_comb(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  IS_COMB(ch->u.ch.attr) ;
	}
	else
	{
		return  ml_char_is_comb( ch->u.multi_ch) ;
	}
}

ml_color_t
ml_char_fg_color(
	ml_char_t *  ch
	)
{
	u_int  attr ;

	attr = ch->u.ch.attr ;

	if( IS_SINGLE_CH(attr))
	{
		return  IS_REVERSED(attr) ? ch->u.ch.bg_color : ch->u.ch.fg_color ;
	}
	else
	{
		return  ml_char_fg_color( ch->u.multi_ch) ;
	}
}

int
ml_char_set_fg_color(
	ml_char_t *  ch ,
	ml_color_t  color
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		ch->u.ch.fg_color = color ;
	}
	else
	{
		u_int  count ;
		u_int  comb_size ;

		comb_size = get_comb_size( ch->u.multi_ch) ;
		for( count = 0 ; count < comb_size + 1 ; count ++)
		{
			ml_char_set_fg_color( ch->u.multi_ch + count , color) ;
		}
	}

	return  1 ;
}

ml_color_t
ml_char_bg_color(
	ml_char_t *  ch
	)
{
	u_int  attr ;

	attr = ch->u.ch.attr ;

	if( IS_SINGLE_CH(attr))
	{
		return  IS_REVERSED(attr) ? ch->u.ch.fg_color : ch->u.ch.bg_color ;
	}
	else
	{
		return  ml_char_bg_color( ch->u.multi_ch) ;
	}
}

int
ml_char_set_bg_color(
	ml_char_t *  ch ,
	ml_color_t  color
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		ch->u.ch.bg_color = color ;
	}
	else
	{
		u_int  count ;
		u_int  comb_size ;

		comb_size = get_comb_size( ch->u.multi_ch) ;
		for( count = 0 ; count < comb_size + 1 ; count ++)
		{
			ml_char_set_bg_color( ch->u.multi_ch + count , color) ;
		}
	}

	return  1 ;
}

int
ml_char_underline_style(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  UNDERLINE_STYLE(ch->u.ch.attr) ;
	}
	else
	{
		return  ml_char_underline_style( ch->u.multi_ch) ;
	}
}

int
ml_char_is_crossed_out(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  IS_CROSSED_OUT(ch->u.ch.attr) ;
	}
	else
	{
		return  ml_char_is_crossed_out( ch->u.multi_ch) ;
	}
}

int
ml_char_reverse_color(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		if( IS_REVERSED(ch->u.ch.attr))
		{
			return  0 ;
		}

		REVERSE_COLOR(ch->u.ch.attr) ;

		return  1 ;
	}
	else
	{
		u_int  count ;
		u_int  comb_size ;

		comb_size = get_comb_size( ch->u.multi_ch) ;
		for( count = 0 ; count < comb_size + 1 ; count ++)
		{
			ml_char_reverse_color( ch->u.multi_ch + count) ;
		}

		return  1 ;
	}
}

int
ml_char_restore_color(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		if( ! IS_REVERSED(ch->u.ch.attr))
		{
			return  0 ;
		}

		RESTORE_COLOR(ch->u.ch.attr) ;

		return  1 ;
	}
	else
	{
		u_int  count ;
		u_int  comb_size ;

		comb_size = get_comb_size( ch->u.multi_ch) ;
		for( count = 0 ; count < comb_size + 1 ; count ++)
		{
			ml_char_restore_color( ch->u.multi_ch + count) ;
		}

		return  1 ;
	}
}

/* XXX only used in iscii_logical() */
int
ml_char_copy_color_reversed_flag(
	ml_char_t *  dst ,
	ml_char_t *  src
	)
{
	if( IS_SINGLE_CH(src->u.ch.attr))
	{
		if( IS_REVERSED(src->u.ch.attr))
		{
			return  ml_char_reverse_color( dst) ;
		}
		else
		{
			return  ml_char_restore_color( dst) ;
		}
	}
	else
	{
		/* See the first character only */
		if( IS_REVERSED(src->u.multi_ch->u.ch.attr))
		{
			return  ml_char_reverse_color( dst) ;
		}
		else
		{
			return  ml_char_restore_color( dst) ;
		}
	}
}

int
ml_char_is_null(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH( ch->u.ch.attr))
	{
		return  ch->u.ch.code == 0 ;
	}
	else
	{
		return  ml_char_is_null( ch->u.multi_ch) ;
	}
}

/*
 * XXX
 * Returns inaccurate result in dealing with combined characters.
 * Even if they have the same code, false is returned since
 * ml_char_t:multi_ch-s never point the same address.)
 */
int
ml_char_equal(
	ml_char_t *  ch1 ,
	ml_char_t *  ch2
	)
{
	return  memcmp( ch1 , ch2 , sizeof( ml_char_t)) == 0 ;
}

int
ml_char_code_is(
	ml_char_t *  ch ,
	u_int32_t  code ,
	mkf_charset_t  cs
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		if( CHARSET(ch->u.ch.attr) == cs && ch->u.ch.code == code)
		{
			return  1 ;
		}
		else
		{
			return  0 ;
		}
	}
	else
	{
		return  ml_char_code_is( ch->u.multi_ch , code , cs) ;
	}
}

int
ml_char_code_equal(
	ml_char_t *  ch1 ,
	ml_char_t *  ch2
	)
{
	ml_char_t *  comb1 ;
	ml_char_t *  comb2 ;
	u_int  comb1_size ;
	u_int  comb2_size ;
	u_int  count ;

	if( ml_char_code( ch1) != ml_char_code( ch2))
	{
		return  0 ;
	}

	comb1 = ml_get_combining_chars( ch1 , &comb1_size) ;
	comb2 = ml_get_combining_chars( ch2 , &comb2_size) ;

	if( comb1_size != comb2_size)
	{
		return  0 ;
	}

	for( count = 0 ; count < comb1_size ; count ++)
	{
		if( comb1[count].u.ch.code != comb2[count].u.ch.code)
		{
			return  0 ;
		}
	}

	return  1 ;
}

ml_char_t *
ml_sp_ch(void)
{
	static ml_char_t  sp_ch ;

	if( sp_ch.u.ch.attr == 0)
	{
		ml_char_init( &sp_ch) ;
		ml_char_set( &sp_ch , ' ' , US_ASCII , 0 , 0 ,
			ML_FG_COLOR , ML_BG_COLOR , 0 , 0 , 0 , 0) ;
	}

	return  &sp_ch ;
}

ml_char_t *
ml_nl_ch(void)
{
	static ml_char_t  nl_ch ;

	if( nl_ch.u.ch.attr == 0)
	{
		ml_char_init( &nl_ch) ;
		ml_char_set( &nl_ch , '\n' , US_ASCII , 0 , 0 ,
			ML_FG_COLOR , ML_BG_COLOR , 0 , 0 , 0 , 0) ;
	}

	return  &nl_ch ;
}


#ifdef  DEBUG

#if  0
#define  DUMP_HEX
#endif

/*
 * for debugging.
 */
void
ml_char_dump(
	ml_char_t *  ch
	)
{
	u_int  comb_size ;
	ml_char_t *  comb_chars ;
	
#ifdef  DUMP_HEX
	kik_msg_printf( "[%.4x]" , ml_char_code(ch)) ;
#else
	if( ml_char_code(ch) >= 0x100)
	{
		if( ml_char_cs(ch) == JISX0208_1983)
		{
			/* only eucjp */

			kik_msg_printf( "%c%c" ,
				((ml_char_code(ch) >> 8) & 0xff) | 0x80 ,
				(ml_char_code(ch) & 0xff) | 0x80) ;
		}
		else
		{
			kik_msg_printf( "**") ;
		}
	}
	else
	{
		kik_msg_printf( "%c" , ml_char_code(ch)) ;
	}
#endif

	if( ( comb_chars = ml_get_combining_chars( ch , &comb_size)) != NULL)
	{
		int  count ;

		for( count = 0 ; count < comb_size ; count ++)
		{
			ml_char_dump( &comb_chars[count]) ;
		}
	}
}

#endif
