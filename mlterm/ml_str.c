/*
 *	$Id$
 */

#include  <string.h>

#include  "ml_str.h"

#include  <kiklib/kik_debug.h>


/* --- global functions --- */

/*
 * string functions
 */
 
int
ml_str_init(
	ml_char_t *  str ,
	u_int  size
	)
{
	int  count ;

	for( count = 0 ; count < size ; count ++)
	{
		ml_char_init( str ++) ;
	}

	return  1 ;
}

ml_char_t *
__ml_str_init(
	ml_char_t *  str ,	/* alloca()-ed memory (see ml_char.h) */
	u_int  size
	)
{
	if( str == NULL)
	{
		/* alloca() failed. */
		
		return  NULL ;
	}

	if( ! ( ml_str_init( str , size)))
	{
		return  NULL ;
	}

	return  str ;
}
	
ml_char_t *
ml_str_new(
	u_int  size
	)
{
	ml_char_t *  str ;

	if( ( str = malloc( sizeof( ml_char_t) * size)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif
	
		return  NULL ;
	}

	if( ml_str_init( str , size) == 0)
	{
		free( str) ;
		
		return  NULL ;
	}

	return  str ;
}

int
ml_str_final(
	ml_char_t *  str ,
	u_int  size
	)
{
	int  count ;

	for( count = 0 ; count < size ; count ++)
	{
		ml_char_final( &str[count]) ;
	}

	return  1 ;
}

int
ml_str_delete(
	ml_char_t *  str ,
	u_int  size
	)
{
	if( ml_str_final( str , size))
	{
		free( str) ;

		return  1 ;
	}
	else
	{
		free( str) ;

		return  0 ;
	}
}

/*
 * dst and src may overlap.
 */
inline int
ml_str_copy(
	ml_char_t *  dst ,
	ml_char_t *  src ,
	u_int  size
	)
{
	int  count ;

	if( size == 0 || dst == src)
	{
		return  0 ;
	}

	if( dst < src)
	{
		for( count = 0 ; count < size ; count ++)
		{
			ml_char_copy( dst++ , src++) ;
		}
	}
	else if( dst > src)
	{
		dst += size ;
		src += size ;
		for( count = 0 ; count < size ; count ++)
		{
			ml_char_copy( --dst  , --src) ;
		}
	}

	return  1 ;
}

inline u_int
ml_str_cols(
	ml_char_t *  chars ,
	u_int  len
	)
{
	int  count ;
	u_int  cols ;

	cols = 0 ;

	for( count = 0 ; count < len ; count ++)
	{
		cols += ml_char_cols( &chars[count]) ;
	}

	return  cols ;
}

/*
 * XXX
 * Returns inaccurate result in dealing with combined characters.
 * Even if they have the same bytes, false is returned since
 * ml_char_t:multi_ch-s never point the same address.)
 */
inline int
ml_str_equal(
	ml_char_t *  str1 ,
	ml_char_t *  str2 ,
	u_int  len
	)
{
	return  memcmp( str1 , str2 , sizeof( ml_char_t) * len) == 0 ;
}

inline int
ml_str_bytes_equal(
	ml_char_t *  str1 ,
	ml_char_t *  str2 ,
	u_int  len
	)
{
	int  count ;

	for( count = 0 ; count < len ; count ++)
	{
		if( ! ml_char_bytes_equal( str1 ++ , str2 ++))
		{
			return  0 ;
		}
	}

	return  1 ;
}

#ifdef  DEBUG

void
ml_str_dump(
	ml_char_t *  chars ,
	u_int  len
	)
{
	int  count ;

	for( count = 0 ; count < len ; count ++)
	{
		ml_char_dump( &chars[count]) ;
	}

	kik_msg_printf( "\n") ;
}

#endif
