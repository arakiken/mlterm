/*
 *	$Id$
 */

#include  <kiklib/kik_locale.h>	/* kik_get_codeset() */

#ifdef  NO_DYNAMIC_LOAD_SSH
#include  "libptyssh/ml_pty_ssh.c"
#else  /* NO_DYNAMIC_LOAD_SSH */

#include  "ml_pty.h"

#include  <stdio.h>		/* snprintf */
#include  <string.h>		/* strcmp */
#include  <kiklib/kik_types.h>
#include  <kiklib/kik_dlfcn.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_path.h>	/* kik_basename */
#include  <kiklib/kik_mem.h>	/* alloca */

#ifndef  LIBDIR
#define  SSHLIB_DIR  "/usr/local/lib/mlterm/"
#else
#define  SSHLIB_DIR  LIBDIR "/mlterm/"
#endif

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static ml_pty_ptr_t (*ssh_new)( const char * , char ** , char ** , const char * ,
		const char * , const char * , const char * , u_int , u_int) ;
static void * (*search_ssh_session)( const char * , const char * , const char *) ;
static int  (*set_use_loopback)( ml_pty_ptr_t , int) ;
static int  (*ssh_scp)( ml_pty_ptr_t , int , char * , char *) ;
static void  (*ssh_set_cipher_list)( const char *) ;
static void  (*ssh_set_keepalive_interval)( u_int) ;
static int  (*ssh_keepalive)( u_int) ;
static void  (*ssh_set_use_x11_forwarding)( int) ;
static u_int  (*ssh_get_x11_fds)( int **) ;
static int  (*ssh_send_recv_x11)( int , int) ;

static int  is_tried ;
static kik_dl_handle_t  handle ;


/* --- static functions --- */

static void
load_library(void)
{
	is_tried = 1 ;

	if( ! ( handle = kik_dl_open( SSHLIB_DIR , "ptyssh")) &&
	    ! ( handle = kik_dl_open( "" , "ptyssh")))
	{
		kik_error_printf( "SSH: Could not load.\n") ;

		return ;
	}

	kik_dl_close_at_exit( handle) ;

	ssh_new = kik_dl_func_symbol( handle , "ml_pty_ssh_new") ;
	search_ssh_session = kik_dl_func_symbol( handle , "ml_search_ssh_session") ;
	set_use_loopback = kik_dl_func_symbol( handle , "ml_pty_set_use_loopback") ;
	ssh_scp = kik_dl_func_symbol( handle , "ml_pty_ssh_scp_intern") ;
	ssh_set_cipher_list = kik_dl_func_symbol( handle , "ml_pty_ssh_set_cipher_list") ;
	ssh_set_keepalive_interval = kik_dl_func_symbol( handle ,
					"ml_pty_ssh_set_keepalive_interval") ;
	ssh_keepalive = kik_dl_func_symbol( handle , "ml_pty_ssh_keepalive") ;
	ssh_set_use_x11_forwarding = kik_dl_func_symbol( handle ,
					"ml_pty_ssh_set_use_x11_forwarding") ;
	ssh_get_x11_fds = kik_dl_func_symbol( handle , "ml_pty_ssh_get_x11_fds") ;
	ssh_send_recv_x11 = kik_dl_func_symbol( handle , "ml_pty_ssh_send_recv_x11") ;
}


/* --- global functions --- */

ml_pty_ptr_t
ml_pty_ssh_new(
	const char *  cmd_path ,
	char **  cmd_argv ,
	char **  env ,
	const char *  uri ,
	const char *  pass ,
	const char *  pubkey ,
	const char *  privkey ,
	u_int  cols ,
	u_int  rows
	)
{
	if( ! is_tried)
	{
		load_library() ;
	}

	if( ssh_new)
	{
		return  (*ssh_new)( cmd_path , cmd_argv , env , uri , pass ,
				pubkey , privkey , cols , rows) ;
	}
	else
	{
		return  NULL ;
	}
}

void *
ml_search_ssh_session(
	const char *  host ,
	const char *  port ,	/* can be NULL */
	const char *  user	/* can be NULL */
	)
{
	if( search_ssh_session)
	{
		return  (*search_ssh_session)( host , port , user) ;
	}
	else
	{
		return  NULL ;
	}
}

int
ml_pty_set_use_loopback(
	ml_pty_ptr_t  pty ,
	int  use
	)
{
	if( set_use_loopback)
	{
		return  (*set_use_loopback)( pty , use) ;
	}
	else
	{
		return  0 ;
	}
}

void
ml_pty_ssh_set_cipher_list(
	const char *  list
	)
{
	/* This function can be called before ml_pty_ssh_new() */
	if( ! is_tried)
	{
		load_library() ;
	}

	if( ssh_set_cipher_list)
	{
		return  (*ssh_set_cipher_list)( list) ;
	}
}

void
ml_pty_ssh_set_keepalive_interval(
	u_int  interval_sec
	)
{
	/* This function can be called before ml_pty_ssh_new() */
	if( ! is_tried)
	{
		load_library() ;
	}

	if( ssh_set_keepalive_interval)
	{
		return  (*ssh_set_keepalive_interval)( interval_sec) ;
	}
}

int
ml_pty_ssh_keepalive(
	u_int  spent_msec
	)
{
	if( ssh_keepalive)
	{
		return  (*ssh_keepalive)( spent_msec) ;
	}
	else
	{
		return  0 ;
	}
}

void
ml_pty_ssh_set_use_x11_forwarding(
	int  use
	)
{
	/* This function can be called before ml_pty_ssh_new() */
	if( ! is_tried)
	{
		load_library() ;
	}

	if( ssh_set_use_x11_forwarding)
	{
		return  (*ssh_set_use_x11_forwarding)( use) ;
	}
	else
	{
		return ;
	}
}

u_int
ml_pty_ssh_get_x11_fds(
	int **  fds
	)
{
	if( ssh_get_x11_fds)
	{
		return  (*ssh_get_x11_fds)( fds) ;
	}
	else
	{
		return  0 ;
	}
}

int
ml_pty_ssh_send_recv_x11(
	int  idx ,
	int  bidirection
	)
{
	if( ssh_send_recv_x11)
	{
		return  (*ssh_send_recv_x11)( idx , bidirection) ;
	}
	else
	{
		return  0 ;
	}
}

#endif	/* NO_DYNAMIC_LOAD_SSH */

int
ml_pty_ssh_scp(
	ml_pty_ptr_t  pty ,
	ml_char_encoding_t  pty_encoding ,	/* Not ML_UNKNOWN_ENCODING */
	ml_char_encoding_t  path_encoding ,	/* Not ML_UNKNOWN_ENCODING */
	char *  dst_path ,
	char *  src_path
	)
{
	int  dst_is_remote ;
	int  src_is_remote ;
	char *  file ;
	char *  _dst_path ;
	char *  _src_path ;
	size_t  len ;
	char *  p ;
	ml_char_encoding_t  locale_encoding ;

	if( strncmp( dst_path , "remote:" , 7) == 0)
	{
		dst_path += 7 ;
		dst_is_remote = 1 ;
	}
	else if( strncmp( dst_path , "local:" , 6) == 0)
	{
		dst_path += 6 ;
		dst_is_remote = 0 ;
	}
	else
	{
		dst_is_remote = -1 ;
	}
	
	if( strncmp( src_path , "local:" , 6) == 0)
	{
		src_path += 6 ;
		src_is_remote = 0 ;
	}
	else if( strncmp( src_path , "remote:" , 7) == 0)
	{
		src_path += 7 ;
		src_is_remote = 1 ;
	}
	else
	{
		if( dst_is_remote == -1)
		{
			src_is_remote = 0 ;
			dst_is_remote = 1 ;
		}
		else
		{
			src_is_remote = (! dst_is_remote) ;
		}
	}

	if( dst_is_remote == -1)
	{
		dst_is_remote = (! src_is_remote) ;
	}
	else if( dst_is_remote == src_is_remote)
	{
		kik_msg_printf( "SCP: Destination host(%s) and source host(%s) is the same.\n" ,
			dst_path , src_path) ;

		return  0 ;
	}

	if( ! *dst_path)
	{
		dst_path = "." ;
	}

	/* scp /tmp/TEST /home/user => scp /tmp/TEST /home/user/TEST */
	file = kik_basename( src_path) ;
	if( ( p = alloca( strlen(dst_path) + strlen( file) + 2)))
	{
		sprintf( p , "%s/%s" , dst_path , file) ;
		dst_path = p ;
	}

#if  0
	kik_debug_printf( "SCP: %s%s -> %s%s\n" ,
		src_is_remote ? "remote:" : "local:" , src_path ,
		dst_is_remote ? "remote:" : "local:" , dst_path) ;
#endif

	if( path_encoding != pty_encoding)
	{
		/* convert to terminal encoding */
		len = strlen( dst_path) ;
		if( ( _dst_path = alloca( len * 2 + 1)))
		{
			_dst_path[ ml_char_encoding_convert( _dst_path , len * 2 , pty_encoding ,
					dst_path , len , path_encoding)] = '\0' ;
		}
		len = strlen( src_path) ;
		if( ( _src_path = alloca( len * 2 + 1)))
		{
			_src_path[ml_char_encoding_convert( _src_path , len * 2 , pty_encoding ,
					src_path , len , path_encoding)] = '\0' ;
		}
	}
	else
	{
		_dst_path = dst_path ;
		_src_path = src_path ;
	}

	if( ( locale_encoding = ml_get_char_encoding( kik_get_codeset())) == ML_UNKNOWN_ENCODING)
	{
		locale_encoding = path_encoding ;
	}

	if( src_is_remote)
	{
		/* Remote: convert to terminal encoding */
		if( _src_path)
		{
			src_path = _src_path ;
		}

		/* Local: convert to locale encoding */
		len = strlen( dst_path) ;
		if( locale_encoding != path_encoding && ( p = alloca( len * 2 + 1)))
		{
			p[ml_char_encoding_convert( p , len * 2 , locale_encoding ,
				dst_path , len , path_encoding)] = '\0' ;
			dst_path = p ;
		}
	}
	else /* if( dst_is_remote) */
	{
		/* Remote: convert to terminal encoding */
		if( _dst_path)
		{
			dst_path = _dst_path ;
		}

		/* Local: convert to locale encoding */
		len = strlen( src_path) ;
		if( locale_encoding != path_encoding && ( p = alloca( len * 2 + 1)))
		{
			p[ ml_char_encoding_convert( p , len * 2 , locale_encoding ,
				src_path , len , path_encoding)] = '\0' ;
			src_path = p ;
		}
	}

#ifdef  NO_DYNAMIC_LOAD_SSH
	if( ml_pty_ssh_scp_intern( pty , src_is_remote , dst_path , src_path))
#else
	if( ssh_scp && (*ssh_scp)( pty , src_is_remote , dst_path , src_path))
#endif
	{
		len = 24 + strlen( _src_path) + strlen( _dst_path) + 1 ;
		if( ( p = alloca( len)))
		{
			sprintf( p , "\r\nSCP: %s%s => %s%s" ,
				src_is_remote ? "remote:" : "local:" , _src_path ,
				src_is_remote ? "local:" : "remote:" , _dst_path) ;
			ml_write_to_pty( pty , p , len - 1) ;
		}

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}
