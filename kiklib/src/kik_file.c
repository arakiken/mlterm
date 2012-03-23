/*
 *	$Id$
 */

#include  "kik_file.h"

#include  <fcntl.h>		/* fcntl() */
#include  <sys/file.h>          /* flock() */
#include  <string.h>		/* memcpy */
#include  <errno.h>
#include  <sys/stat.h>		/* stat */

#include  "kik_def.h"		/* HAVE_FGETLN */
#include  "kik_mem.h"		/* malloc */
#include  "kik_str.h"		/* kik_str_alloca_dup */
#include  "kik_debug.h"


#define  BUF_UNIT_SIZE  512


/* --- global functions --- */

kik_file_t *
kik_file_new(
	FILE *  fp
	)
{
	kik_file_t *  file  ;

	if( ( file = malloc( sizeof( kik_file_t))) == NULL)
	{
		return  NULL ;
	}
	
	file->file = fp ;
	file->buffer = NULL ;
	file->buf_size = 0 ;

	return  file ;
}

int
kik_file_delete(
	kik_file_t *  file
	)
{
	/* not fclose(file->fp) */
	
	free( file->buffer) ;
	free( file) ;

	return  1 ;
}

kik_file_t *
kik_file_open(
	const char *  file_path ,
	const char *  mode
	)
{
	FILE *  fp ;
	
	if( ( fp = fopen( file_path , mode)) == NULL)
	{
		return  NULL ;
	}

	return  kik_file_new( fp) ;
}

int
kik_file_close(
	kik_file_t *  file
	)
{
	int  result ;
	
	if( fclose( file->file) == 0)
	{
		result = 1 ;
	}
	else
	{
		result = 0 ;
	}

	result |= kik_file_delete( file) ;

	return  result ;
}

FILE *
kik_fopen_with_mkdir(
	const char *  file_path ,
	const char *  mode
	)
{
	FILE *  fp ;
	char *  p ;

	if( ( fp = fopen( file_path , mode)))
	{
		return  fp ;
	}

	if( ( p = kik_str_alloca_dup( file_path)) == NULL || ! kik_mkdir_for_file( p , 0700))
	{
		return  NULL ;
	}

	return  fopen( file_path , mode) ;
}

#ifdef  HAVE_FGETLN

/*
 * This is a wrapper function of fgetln().
 * If 'from' file doesn't end with '\n', '\0' is automatically appended to the end of file.
 */
char *
kik_file_get_line(
	kik_file_t *  from ,
	size_t *  len
	)
{
	char *  line ;
	
	if( ( line = fgetln( from->file , len)) == NULL)
	{
		return  NULL ;
	}

	if( line[*len - 1] != '\n')
	{
		if( ( from->buffer = realloc( from->buffer , *len + 1)) == NULL)
		{
			return  NULL ;
		}
		memcpy( from->buffer , line , *len) ;
		from->buffer[*len] = '\0' ;
		from->buf_size = ++(*len) ;
	}

	return  line ;
}

#else

/*
 * This behaves like fgetln().
 *
 * This returns the pointer to the beginning of line , and it becomes invalid
 * after the next kik_file_get_line() (whether successful or not) or as soon as
 * kik_file_close() is executed.
 * If 'from' file doesn't end with '\n', '\0' is automatically appended to the end of file.
 */
char *
kik_file_get_line(
	kik_file_t *  from ,
	size_t *  len
	)
{
	size_t  filled ;
	int  c ;

	filled = 0 ;
	
	if( ( c = fgetc( from->file)) < 0)
	{
		return  NULL ;
	}

	while( 1)
	{
		if( filled == from->buf_size)
		{
			from->buf_size += BUF_UNIT_SIZE ;
			from->buffer = realloc( from->buffer , from->buf_size) ;
		}

		if( c < 0)
		{
			from->buffer[filled++] = '\0' ;
			break ;
		}
		else
		{
			from->buffer[filled++] = c ;
			
			if( c == '\n')
			{
				break ;
			}
		}

		c = fgetc( from->file) ;
	}

	*len = filled ;

	return  from->buffer ;
}

#endif

#if defined(HAVE_FLOCK) && defined(LOCK_EX) && defined(LOCK_UN)

int
kik_file_lock(
	int  fd
	)
{
	if( flock( fd , LOCK_EX) == -1)
	{
		return  0 ;
	}
	else
	{
		return  1 ;
	}
}

int
kik_file_unlock(
	int  fd
	)
{
	if( flock( fd , LOCK_UN) == -1)
	{
		return  0 ;
	}
	else
	{
		return  1 ;
	}
}

#else

int
kik_file_lock(
	int  fd
	)
{
	return  0 ;
}

int
kik_file_unlock(
	int  fd
	)
{
	return  0 ;
}

#endif


#ifdef  F_GETFD

int
kik_file_set_cloexec(
	int fd
	)
{
	int  old_flags ;
	
	old_flags = fcntl( fd, F_GETFD) ;
	if( old_flags == -1)
	{
		return  0 ;
	}
	
	if( !(old_flags & FD_CLOEXEC)
	 && (fcntl( fd, F_SETFD, old_flags|FD_CLOEXEC) == -1) )
	{
		return  0 ;
	}
	return  1 ;
}

int
kik_file_unset_cloexec(
	int fd
	)
{
  	int  old_flags ;

	old_flags = fcntl( fd, F_GETFD) ;
        if( old_flags == -1)
        {
		return  0 ;
	}
	if( (old_flags & FD_CLOEXEC)
	 && (fcntl( fd, F_SETFD, old_flags & (~FD_CLOEXEC)) == -1) )
        {
		return  0 ;
	}
	return  1 ;
}

#else /* F_GETFD */

int
kik_file_set_cloexec(
	int fd
	)
{
	/* do nothing */

	return  0 ;
}

int
kik_file_unset_cloexec(
	int fd
	)
{
  	/* do nothing */

  	return  0 ;
}

#endif

/*
 * /a/b/c  => mkdir /a ; mkdir /a/b
 * /a/b/c/ => mkdir /a ; mkdir /a/b ; mkdir /a/b/c
 * /a => do nothing
 */
int
kik_mkdir_for_file(
	char *  file_path ,	/* Not const. Don't specify read only data. */
	mode_t  dir_mode
	)
{
	char *  p ;
	
	p = file_path + 1 ;
	while( *p)
	{
		if( *p == '/'
		#ifdef  USE_WIN32API
			|| *p == '\\'
		#endif
			)
		{
			struct stat  s ;
			char  c ;

			c = *p ;	/* save */
			
			*p = '\0' ;
			if( stat( file_path , &s) != 0)
			{
				if( errno == ENOENT &&
				#ifdef  USE_WIN32API
					mkdir( file_path) != 0
				#else
					mkdir( file_path , dir_mode) != 0
				#endif
					)
				{
					kik_msg_printf( "Failed to mkdir %s\n" , file_path) ;

					*p = c ;	/* restore */
					
					return  0 ;
				}
			}
			
			*p = c ;	/* restore */
		}

		p ++ ;
	}

	return  1 ;
}
