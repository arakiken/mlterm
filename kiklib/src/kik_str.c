/*
 *	update: <2001/11/26(02:38:44)>
 *	$Id$
 */

#include  "kik_str.h"

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

#ifndef  HAVE_BASENAME

char *
__kik_basename(
	char *  path
	)
{
	char *  p ;

	if( path == NULL || *path == '\0')
	{
		return "." ;
	}

	p = path + strlen(path) - 1 ;
	while( *p == '/' && p != path)
	{
		*p-- = '\0' ;
	}

	if( ( p = strrchr( path , '/')) == NULL || p[1] == '\0')
	{
		return  path ;
	}
	else
	{
		return  p + 1 ;
	}
}

#endif

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
	char *  src
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
	u_char *  src ,
	size_t  src_len ,
	size_t  tab_len 
	)
{
	size_t  pos_in_tab ;
	size_t  space_num ;
	int  dst_pos ;
	int  src_pos ;
	int  counter ;

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
				for( counter = 0 ; counter < space_num ; counter ++)
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
		for( counter = 0 ; counter < space_num ; counter ++)
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
	int  pos ;
	
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
kik_str_n_to_int(
	int *  i ,
	char *  s ,
	size_t  n
	)
{
	int  digit ;
	int  _i ;

	if( *s == '\0')
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
kik_str_to_int(
	int *  i ,
	char *  s
	)
{
	int  _i ;

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
