/*
 *	$Id$
 */

#include  "kik_str.h"

#include  <stdio.h>		/* sprintf */
#include  <stdarg.h>		/* va_list */
#include  <ctype.h>		/* isdigit */

#include  "kik_debug.h"
#include  "kik_mem.h"


#undef  kik_str_sep
#undef  kik_basename


/* --- global functions --- */

#ifndef  HAVE_STRSEP

char *
__kik_str_sep(
	char **  strp ,
	const char *  delim
	)
{

        char *  s ; 
        const char *  spanp ;
        int  c ;
	int  sc ;
        char *  tok ;
 
	if( ( s = *strp) == NULL)
	{
		return	NULL ;
	}
	
	for( tok = s ; ; )
	{
		c = *s++ ; 
		spanp = delim ; 
		do
		{ 
			if( ( sc = *spanp++) == c)
			{ 
				if( c == 0)
				{ 
					s = NULL ;
				}
				else
				{
					s[-1] = 0 ;
				}
				
				*strp = s ;
				 
				return  tok ; 
			}	
		}
		while( sc != 0) ; 
	}
}

#endif

/*
 * !! Notice !!
 * It is a caller that is responsible to check buffer overrun.
 */
int
kik_snprintf(
	char *  str ,
	size_t  size ,
	const char *  format ,
	...
	)
{
	va_list  arg_list ;

	va_start( arg_list , format) ;

#ifdef  HAVE_SNPRINTF
	return  vsnprintf( str , size , format , arg_list) ;
#else
	/*
	 * XXX
	 * this may cause buffer overrun.
	 */

	return  vsprintf( str , format , arg_list) ;
#endif
}

char *
kik_str_dup(
	const char *  str ,
	const char *  file ,	/* should be allocated memory. */
	int  line ,
	const char *  func	/* should be allocated memory. */
	)
{
	char *  new_str = NULL ;

	if( ( new_str = kik_mem_malloc( strlen( str) + 1 , file , line , func)) == NULL)
	{
		return  NULL ;
	}

	strcpy( new_str , str) ;

	return  new_str ;
}

char *
__kik_str_copy(
	char *  dst ,	/* alloca()-ed memory (see kik_str.h) */
	const char *  src
	)
{
	if( dst == NULL)
	{
		/* alloca() failed */
		
		return  NULL ;
	}

	return  strcpy( dst , src) ;
}

/*
 * XXX
 * this doesn't concern about ISO2022 sequences or so.
 * dst/src must be u_char since 0x80 - 0x9f is specially dealed.
 */
size_t
kik_str_tabify(
	u_char *  dst ,
	size_t  dst_len ,
	const u_char *  src ,
	size_t  src_len ,
	size_t  tab_len 
	)
{
	size_t  pos_in_tab ;
	size_t  space_num ;
	int  dst_pos ;
	int  src_pos ;
	int  count ;

	if( tab_len == 0)
	{
	#ifdef  KIK_DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " 0 is illegal tab length.\n") ;
	#endif
	
		return  0 ;
	}

	dst_pos = 0 ;
	pos_in_tab = 0 ;
	space_num = 0 ;
	for( src_pos = 0 ; src_pos < src_len ; src_pos ++)
	{
		if( src[src_pos] == ' ')
		{
			if( pos_in_tab == tab_len - 1)
			{
				dst[dst_pos++] = '\t' ;

				if( dst_pos >= dst_len)
				{
					return  dst_pos ;
				}

				space_num = 0 ;

				/* next */
				pos_in_tab = 0 ;
			}
			else
			{
				space_num ++ ;

				/* next */
				pos_in_tab ++ ;
			}
		}
		else
		{
			if( space_num > 0)
			{
				for( count = 0 ; count < space_num ; count ++)
				{
					dst[dst_pos++] = ' ' ;

					if( dst_pos >= dst_len)
					{
						return  dst_pos ;
					}
				}
				
				space_num = 0 ;
			}

			
			dst[dst_pos++] = src[src_pos] ;

			if( dst_pos >= dst_len)
			{
				return  dst_pos ;
			}
			
			if( src[src_pos] == '\n' || src[src_pos] == '\t')
			{
				/* next */
				pos_in_tab = 0 ;
			}
			else if( ( 0x20 <= src[src_pos] && src[src_pos] < 0x7f) || 0xa0 <= src[src_pos])
			{
				/* next */
				if( pos_in_tab == tab_len - 1)
				{
					pos_in_tab = 0 ;
				}
				else
				{
					pos_in_tab ++ ;
				}
			}
			else if( src[src_pos] == 0x1b)
			{
				/* XXX  ISO2022 seq should be considered. */
			}
		}
	}

	if( space_num > 0)
	{
		for( count = 0 ; count < space_num ; count ++)
		{
			dst[dst_pos++] = ' ' ;

			if( dst_pos >= dst_len)
			{
				return  dst_pos ;
			}
		}
	}
	
	return  dst_pos ;
}

char *
kik_str_chop_spaces(
	char *  str
	)
{
	size_t  pos ;
	
	pos = strlen( str) ;

	while( pos > 0)
	{
		pos -- ;

		if( str[pos] != ' ' && str[pos] != '\t')
		{
			str[pos + 1] = '\0' ;

			break ;
		}
	}
	
	return  str ;
}

int
kik_str_n_to_uint(
	u_int *  i ,
	const char *  s ,
	size_t  n
	)
{
	u_int  _i ;
	int  digit ;

	if( n == 0)
	{
		return  0 ;
	}

	_i = 0 ;
	for( digit = 0 ; digit < n && s[digit] ; digit ++)
	{
		if( ! isdigit( s[digit]))
		{
			return  0 ;
		}

		_i *= 10 ;
		_i += (s[digit] - 0x30) ;
	}

	*i = _i ;
	
	return  1 ;
}

int
kik_str_n_to_int(
	int *  i ,
	const char *  s ,
	size_t  n
	)
{
	u_int  _i ;
	int  is_minus ;

	if( n == 0)
	{
		return  0 ;
	}

	if( *s == '-')
	{
		if( -- n == 0)
		{
			return  0 ;
		}

		s ++ ;
		
		is_minus = 1 ;
	}
	else
	{
		is_minus = 0 ;
	}

	if( ! kik_str_n_to_uint( &_i , s , n))
	{
		return  0 ;
	}
	
	if( (int)_i < 0)
	{
		return  0 ;
	}

	if( is_minus)
	{
		*i = -((int)_i) ;
	}
	else
	{
		*i = (int)_i ;
	}

	return  1 ;
}

int
kik_str_to_uint(
	u_int *  i ,
	const char *  s
	)
{
	u_int  _i ;

	if( *s == '\0')
	{
		return  0 ;
	}
	
	_i = 0 ;
	while( *s)
	{
		if( ! isdigit( *s))
		{
			return  0 ;
		}

		_i *= 10 ;
		_i += (*s - 0x30) ;

		s ++ ;
	}

	*i = _i ;

	return  1 ;
}

int
kik_str_to_int(
	int *  i ,
	const char *  s
	)
{
	u_int  _i ;
	int  is_minus ;

	if( *s == '\0')
	{
		return  0 ;
	}
	
	if( *s == '-')
	{
		if( *(++ s) == '\0')
		{
			return  0 ;
		}
		
		is_minus = 1 ;
	}
	else
	{
		is_minus = 0 ;
	}

	if( ! kik_str_to_uint( &_i , s))
	{
		return  0 ;
	}

	if( (int)_i < 0)
	{
		return  0 ;
	}

	if( is_minus)
	{
		*i = -((int)_i) ;
	}
	else
	{
		*i = (int)_i ;
	}

	return  1 ;
}

u_int
kik_count_char_in_str(
	const char *  str ,
	char  ch
	)
{
	u_int  count ;

	count = 0 ;
	while( *str)
	{
		if( *str == ch)
		{
			count ++ ;
		}

		str ++ ;
	}

	return  count ;
}

/* str1 and str2 can be NULL */
int
kik_compare_str(
	const char *  str1 ,
	const char *  str2
	)
{
	if( str1 == str2)
	{
		return  0 ;
	}

	if( str1 == NULL)
	{
		return  -1 ;
	}
	else if( str2 == NULL)
	{
		return  1 ;
	}

	return  strcmp( str1 , str2) ;
}

char *
kik_str_replace(
	const char *  str ,
	const char *  orig ,
	const char *  new
	)
{
	char *  p ;
	char *  new_str ;

	if( ! ( p = strstr( str , orig)) ||
	    ! ( new_str = malloc( strlen( str) + strlen(new) - strlen(orig) + 1)))
	{
		return  NULL ;
	}

	strncpy( new_str , str , p - str) ;
	sprintf( new_str + (p - str) , "%s%s" , new , p + strlen(orig)) ;

	return  new_str ;
}

char *
kik_str_unescape(
	const char *  str
	)
{
	char *  new_str ;
	char *  p ;

	if( ( new_str = malloc( strlen( str) + 1)) == NULL)
	{
		return  NULL ;
	}

	for( p = new_str ; *str ; str++ , p++)
	{
		if( *str == '\\')
		{
			u_int  digit ;

			if( *(++str) == '\0')
			{
				break ;
			}
			else if( sscanf( str , "x%2x" , &digit) == 1)
			{
				*p = (char)digit ;
				str += 2 ;
			}
			else if( *str == 'n')
			{
				*p = '\n' ;
			}
			else if( *str == 'r')
			{
				*p = '\r' ;
			}
			else if( *str == 't')
			{
				*p = '\t' ;
			}
			else if( *str == 'e' || *str == 'E')
			{
				*p = '\033' ;
			}
			else
			{
				*p = *str ;
			}
		}
		else if( *str == '^')
		{
			if( *(++str) == '\0')
			{
				break ;
			}
			else if( '@' <= *str && *str <= '_')
			{
				*p = *str - 'A' + 1 ;
			}
			else if( *str == '?')
			{
				*p = '\x7f' ;
			}
			else
			{
				*p = *str ;
			}
		}
		else
		{
			*p = *str ;
		}
	}

	*p = '\0' ;

	return  new_str ;
}
