/*
 *	$Id$
 */

#include  "kik_path.h"

#include  <stdio.h>	/* NULL */
#include  <string.h>

#include  "kik_str.h"	/* kik_str_alloca_dup */


/* --- global functions --- */

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

int
kik_path_cleanname(
	char *  cleaned_path ,
	size_t  size ,
	char *  path
	)
{
	char *  src ;
	char *  dst ;
	size_t  left ;
	char *  p ;

	if( size == 0)
	{
		return  0 ;
	}

	if( ( src = kik_str_alloca_dup( path)) == NULL)
	{
		return  0 ;
	}

	dst = cleaned_path ;
	left = size ;

	if( *src == '/')
	{
		*(dst ++) = '\0' ;
		left -- ;
		src ++ ;
	}
	
	while( ( p = strchr( src , '/')))
	{
		*p = '\0' ;
		
		if( strcmp( src , ".") == 0)
		{
			goto  end ;
		}
		else if( strcmp( src , "..") == 0 && left < size)
		{
			char *  last ;

			if( ( last = strrchr( cleaned_path , '/')))
			{
				last ++ ;
			}
			else
			{
				last = cleaned_path ;
			}

			if( *last != '\0' && strcmp( last , "..") != 0)
			{
				dst -= (strlen( last) + 1) ;
				left += (strlen( last) + 1) ;

				*dst = '\0' ;

				goto  end ;
			}
		}
		
		if( *src)
		{
			if( left < strlen( src) + 1)
			{
				return  1 ;
			}

			if( left < size)
			{
				*(dst - 1) = '/' ;
			}
			
			strcpy( dst , src) ;
			
			dst += (strlen( src) + 1) ;
			left -= (strlen( src) + 1) ;
		}

	end:
		src = p + 1 ;
	}

	if( src && *src)
	{
		if( left < strlen( src) + 1)
		{
			return  1 ;
		}

		if( left < size)
		{
			*(dst - 1) = '/' ;
		}

		strcpy( dst , src) ;
		
		dst += (strlen( src) + 1) ;
		left -= (strlen( src) + 1) ;
	}

	return  1 ;
}

int
kik_path_resolve(
	char *  resolved_path ,
	size_t  size ,
	char *  path
	)
{
	return  0 ;
}
