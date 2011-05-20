/*
 *	$Id$
 */

#include  "ml_char.h"

#include  <string.h>		/* memset/memcpy */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>	/* K_MIN */


/*
 * !! Notice !!
 */
#define  CHARSET(attr)  (((attr) >> 7) & 0x1ff)
#define  SIZE(attr) (CS_SIZE(CHARSET(attr)))

#define  IS_BIWIDTH(attr)  (((attr) >> 6) & 0x1)

#define  IS_REVERSED(attr)  (((attr) >> 5) & 0x1)
#define  REVERSE_COLOR(attr) ((attr) |= (0x1 << 5))
#define  RESTORE_COLOR(attr) ((attr) &= ~(0x1 << 5))

#define  IS_BOLD(attr)  (((attr) >> 4) & 0x1)

#define  IS_UNDERLINED(attr)  (((attr) >> 3) & 0x1)

#define  IS_COMB(attr)  (((attr) >> 2) & 0x1)

#define  IS_COMB_TRAILING(attr)  (((attr) >> 1) & 0x1)
#define  SET_COMB_TRAILING(attr)  ((attr) |= 0x2)
#define  UNSET_COMB_TRAILING(attr)  ((attr) &= 0xfffd)

#define  IS_SINGLE_CH(attr)  ((attr) & 0x1)
#define  USE_MULTI_CH(attr)  ((attr) &= 0xfffe)
#define  UNUSE_MULTI_CH(attr)  ((attr) |= 0x1)

#define  COMPOUND_ATTR(charset,is_biwidth,is_bold,is_underlined,is_comb) \
	( ((charset) << 7) | ((is_biwidth) << 6) | \
	( 0x0 << 5) | ((is_bold) << 4) | ((is_underlined) << 3) | \
	( (is_comb) << 2) | ( 0x0 << 1) | 0x1)


/* --- static variables --- */

static int  use_char_combining = 1 ;
static int  use_multi_col_char = 1 ;


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

inline static u_int8_t
intern_color(
	ml_color_t  ex_color
	)
{
	if( ex_color == ML_FG_COLOR)
	{
		return  0x10 ;
	}
	else if( ex_color == ML_BG_COLOR)
	{
		return  0xe7 ;
	}
	else if( IS_256_COLOR(ex_color))
	{
		if( ex_color == 0x10)
		{
			/* avoid batting internal presentation of ML_FG_COLOR. */
			return  ML_BLACK ;
		}
		else if( ex_color == 0xe7)
		{
			/* avoid batting internal presentation of ML_BG_COLOR. */
			return  ML_WHITE | ML_BOLD_COLOR_MASK ;
		}
	}
	
	return  ex_color ;
}

inline static ml_color_t
extern_color(
	u_int8_t  in_color
	)
{
	if( in_color == 0x10)
	{
		return  ML_FG_COLOR ;
	}
	else if( in_color == 0xe7)
	{
		return  ML_BG_COLOR ;
	}
	else
	{
		return  in_color ;
	}
}


/* --- global functions --- */

int
ml_set_use_char_combining(
	int  use_it
	)
{
	use_char_combining = use_it ;

	return  1 ;
}

int
ml_set_use_multi_col_char(
	int  use_it
	)
{
	use_multi_col_char = use_it ;

	return  1 ;
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
	u_char *  bytes ,
	size_t  size ,
	mkf_charset_t  cs ,
	int  is_biwidth ,
	int  is_comb ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color ,
	int  is_bold ,
	int  is_underlined
	)
{
	if( CS_SIZE(cs) != size)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " char size error => size %d <=> CS_SIZE %d\n",
			size, CS_SIZE(cs)) ;
	#endif
		return  0 ;
	}

	if( size > MAX_CHAR_SIZE)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " size %d is over MAX CHAR SIZE %d\n",
			size, MAX_CHAR_SIZE) ;
	#endif
		return  0 ;
	}
	
	ml_char_final( ch) ;

	memcpy( ch->u.ch.bytes , bytes , size) ;
	memset( ch->u.ch.bytes + size , 0 , MAX_CHAR_SIZE - size) ;

#ifdef  DEBUG
	if( cs >= MAX_CHARSET)
	{
		kik_warn_printf(KIK_DEBUG_TAG "charset(%x) is out of range\n", cs) ;
	}
	if( is_biwidth > 1 || is_biwidth < 0)
	{
		kik_warn_printf(KIK_DEBUG_TAG "is_biwidth should be 0/1\n") ;
	}
	if( is_comb > 1 || is_comb < 0)
	{
		kik_warn_printf(KIK_DEBUG_TAG "is_comb should be 0/1\n") ;
	}
	if( is_bold > 1 || is_bold < 0)
	{
		kik_warn_printf(KIK_DEBUG_TAG "is_bold should be 0/1\n") ;
	}
	if( is_underlined > 1 || is_underlined < 0)
	{
		kik_warn_printf(KIK_DEBUG_TAG "is_underlined should be 0/1\n") ;
	}
#endif

	ch->u.ch.attr = COMPOUND_ATTR(cs,is_biwidth,is_bold,is_underlined,is_comb) ;
	ch->u.ch.fg_color = intern_color(fg_color) ;
	ch->u.ch.bg_color = intern_color(bg_color) ;

	return  1 ;
}

int
ml_char_combine(
	ml_char_t *  ch ,
	u_char *  bytes ,
	size_t  size ,
	mkf_charset_t  cs ,
	int  is_biwidth ,
	int  is_comb ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color ,
	int  is_bold ,
	int  is_underlined
	)
{
	ml_char_t *  multi_ch ;

	if( ! use_char_combining)
	{
		return  0 ;
	}

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
		if( ml_char_set( multi_ch + 1 , bytes , size , cs , is_biwidth , is_comb ,
			fg_color , bg_color , is_bold , is_underlined) == 0)
		{
			return  0 ;
		}
	}
	else
	{
		u_int  comb_size ;
		
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

	#if !defined(__GLIBC__)
		if( sizeof( multi_ch) >= 8 && ((long)( multi_ch) & 0x1UL) != 0)
		{
			kik_msg_printf( "Your malloc() doesn't return 2 bits aligned address."
			                "Character combining is not supported.\n") ;

			return  0 ;
		}
	#endif

		SET_COMB_TRAILING( multi_ch[comb_size].u.ch.attr) ;
		ml_char_init( multi_ch + comb_size + 1) ;
		if( ml_char_set( multi_ch + comb_size + 1 , bytes , size , cs , is_biwidth ,
			is_comb , fg_color , bg_color , is_bold , is_underlined) == 0)
		{
			return  0 ;
		}
	}

	ch->u.multi_ch = multi_ch ;
	USE_MULTI_CH(ch->u.ch.attr) ;

	return  1 ;
}

int
ml_combine_chars(
	ml_char_t *  ch ,
	ml_char_t *  comb
	)
{
	return  ml_char_combine( ch , ml_char_bytes( comb) , SIZE(comb->u.ch.attr) ,
			CHARSET(comb->u.ch.attr) ,
			IS_BIWIDTH(comb->u.ch.attr) , IS_COMB(comb->u.ch.attr) ,
			comb->u.ch.fg_color , comb->u.ch.bg_color ,
			IS_BOLD(comb->u.ch.attr) , IS_UNDERLINED(comb->u.ch.attr)) ;
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
		USE_MULTI_CH(dst->u.ch.attr) ;
	}
	
	return  1 ;
}

u_char *
ml_char_bytes(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  ch->u.ch.bytes ;
	}
	else
	{
		return  ml_char_bytes( ch->u.multi_ch) ;
	}
}

size_t
ml_char_size(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  SIZE(ch->u.ch.attr) ;
	}
	else
	{
		return  ml_char_size( ch->u.multi_ch) ;
	}
}

int
ml_char_set_bytes(
	ml_char_t *  ch ,
	u_char *  bytes
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		memcpy( ch->u.ch.bytes , bytes , SIZE(ch->u.ch.attr)) ;
	}
	else
	{
		ml_char_set_bytes( ch->u.multi_ch , bytes) ;
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
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		ml_font_t  font ;

		font = CHARSET(ch->u.ch.attr) ;

		if( IS_BOLD(ch->u.ch.attr))
		{
			font |= FONT_BOLD ;
		}

		if( IS_BIWIDTH(ch->u.ch.attr))
		{
			font |= FONT_BIWIDTH ;
		}

		return  font ;
	}
	else
	{
		return  ml_char_font( ch->u.multi_ch) ;
	}
}

u_int
ml_char_cols(
	ml_char_t *  ch
	)
{
	if( use_multi_col_char && ml_char_is_biwidth(ch))
	{
		return  2 ;
	}
	else
	{
		return  1 ;
	}
}

/*
 * 'use_multi_col_char' not concerned.
 */
u_int
ml_char_is_biwidth(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  IS_BIWIDTH(ch->u.ch.attr) ;
	}
	else
	{
		return  ml_char_is_biwidth( ch->u.multi_ch) ;
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
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		ml_color_t  color ;

		if( IS_REVERSED(ch->u.ch.attr))
		{
			color = extern_color( ch->u.ch.bg_color) ;
		}
		else
		{
			color = extern_color( ch->u.ch.fg_color) ;
		}

		if( color < MAX_VTSYS_COLORS && IS_BOLD(ch->u.ch.attr))
		{
			color |= ML_BOLD_COLOR_MASK ;
		}

		return  color ;
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
		ch->u.ch.fg_color = intern_color(color) ;

		return  1 ;
	}
	else
	{
		int  count ;
		u_int  comb_size ;

		comb_size = get_comb_size( ch->u.multi_ch) ;
		for( count = 0 ; count < comb_size + 1 ; count ++)
		{
			ml_char_set_fg_color( ch->u.multi_ch + count , color) ;
		}

		return  1 ;
	}
}

ml_color_t
ml_char_bg_color(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		ml_color_t  color ;

		if( IS_REVERSED(ch->u.ch.attr))
		{
			color = extern_color(ch->u.ch.fg_color) ;
		}
		else
		{
			color = extern_color(ch->u.ch.bg_color) ;
		}

		/*
		 * xterm doesn't use hilighted color for bold background color.
		 */
	#if  0
		if( color < MAX_VTSYS_COLORS && IS_BOLD(ch->u.ch.attr))
		{
			color |= ML_BOLD_COLOR_MASK ;
		}
	#endif

		return  color ;
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
		ch->u.ch.bg_color = intern_color(color) ;

		return  1 ;
	}
	else
	{
		int  count ;
		u_int  comb_size ;

		comb_size = get_comb_size( ch->u.multi_ch) ;
		for( count = 0 ; count < comb_size + 1 ; count ++)
		{
			ml_char_set_bg_color( ch->u.multi_ch + count , color) ;
		}

		return  1 ;
	}
}

int
ml_char_is_underlined(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  IS_UNDERLINED(ch->u.ch.attr) ;
	}
	else
	{
		return  ml_char_is_underlined( ch->u.multi_ch) ;
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
		int  count ;
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
		int  count ;
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
		return  (SIZE(ch->u.ch.attr) == 1 && ch->u.ch.bytes[0] == '\0') ;
	}
	else
	{
		return  ml_char_is_null( ch->u.multi_ch) ;
	}
}

/*
 * XXX
 * Returns inaccurate result in dealing with combined characters.
 * Even if they have the same bytes, false is returned since
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
ml_char_bytes_is(
	ml_char_t *  ch ,
	char *  bytes ,
	size_t  size ,
	mkf_charset_t  cs
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		if( CHARSET(ch->u.ch.attr) == cs &&
			SIZE(ch->u.ch.attr) == size &&
			memcmp( ml_char_bytes( ch) , bytes , size) == 0)
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
		return  ml_char_bytes_is( ch->u.multi_ch , bytes , size , cs) ;
	}
}

int
ml_char_bytes_equal(
	ml_char_t *  ch1 ,
	ml_char_t *  ch2
	)
{
	u_int  size1 ;
	u_int  size2 ;
	ml_char_t *  comb1 ;
	ml_char_t *  comb2 ;
	u_int  comb1_size ;
	u_int  comb2_size ;
	int  count ;

	size1 = ml_char_size( ch1) ;
	size2 = ml_char_size( ch2) ;

	if( size1 != size2)
	{
		return  0 ;
	}

	if( memcmp( ml_char_bytes( ch1) , ml_char_bytes( ch2) , size1) != 0)
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
		if( ! ml_char_bytes_equal( &comb1[count] , &comb2[count]))
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
		ml_char_set( &sp_ch , (u_char *)" " , 1 , US_ASCII , 0 , 0 ,
			ML_FG_COLOR , ML_BG_COLOR , 0 , 0) ;
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
		ml_char_set( &nl_ch , (u_char *)"\n" , 1 , US_ASCII , 0 , 0 ,
			ML_FG_COLOR , ML_BG_COLOR , 0 , 0) ;
	}

	return  &nl_ch ;
}


#ifdef  DEBUG

#if  1
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
	int  i ;
	
	kik_msg_printf( "[") ;
	for( i = 0 ; i < ml_char_size(ch) ; i ++)
	{
		kik_msg_printf( "%.2x" , ml_char_bytes(ch)[i]) ;
	}
	kik_msg_printf( "]") ;
#else
	if( ml_char_size(ch) == 2)
	{
		if( ml_char_cs(ch) == JISX0208_1983)
		{
			/* only eucjp */

			kik_msg_printf( "%c%c" , ml_char_bytes(ch)[0] | 0x80 ,
				ml_char_bytes(ch)[1] | 0x80) ;
		}
		else
		{
			kik_msg_printf( "**") ;
		}
	}
	else if( ml_char_size(ch) == 1)
	{
		kik_msg_printf( "%c" , ml_char_bytes(ch)[0]) ;
	}
	else
	{
		kik_msg_printf( "!!! unsupported char[0x%.2x len %d] !!!" ,
			ml_char_bytes(ch)[0] , SIZE(ch->u.ch.attr)) ;
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
