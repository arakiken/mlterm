/*
 *	$Id$
 */

#include  "ml_config_menu.h"

#ifdef  USE_WIN32API
#include  <windows.h>
#endif

#include  <stdio.h>		/* sprintf */
#include  <string.h>		/* strchr */
#include  <unistd.h>		/* fork */
#include  <kiklib/kik_sig_child.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */
#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_file.h>   /* kik_file_set_cloexec, kik_file_unset_cloexec */
#include  <kiklib/kik_def.h>	/* HAVE_WINDOWS_H */

#ifndef  LIBEXECDIR
#define  LIBEXECDIR  "/usr/local/libexec"
#endif


/* --- static functions --- */

#ifdef  USE_WIN32API

static DWORD WINAPI
wait_child_exited(
	LPVOID thr_param
  	)
{
	ml_config_menu_t *  config_menu ;
	DWORD  ev ;

#if  0
	kik_debug_printf( "wait_child_exited thread.\n") ;
#endif

	config_menu = thr_param ;

	while( 1)
	{
		ev = WaitForSingleObject( config_menu->pid , INFINITE) ;

	#if  0
		kik_debug_printf( "WaitForMultipleObjects %dth event signaled.\n", ev) ;
	#endif

		if( ev == WAIT_OBJECT_0)
		{
			CloseHandle( config_menu->fd) ;
			CloseHandle( config_menu->pid) ;
			config_menu->fd = -1 ;
			config_menu->pid = 0 ;

		#ifdef  USE_LIBSSH2
			ml_pty_unuse_loopback( config_menu->pty) ;
			config_menu->pty = NULL ;
		#endif

			break ;
		}
	}

  	ExitThread( 0) ;

  	return  0 ;
}

#else

static void
sig_child(
	void *  self ,
	pid_t  pid
	)
{
	ml_config_menu_t *  config_menu ;
	
	config_menu = self ;

	if( config_menu->pid == pid)
	{
		config_menu->pid = 0 ;

		close( config_menu->fd) ;
		config_menu->fd = -1 ;

	#ifdef  USE_LIBSSH2
		if( config_menu->pty)
		{
			ml_pty_unuse_loopback( config_menu->pty) ;
			config_menu->pty = NULL ;
		}
	#endif
	}
}

#endif


/* --- global functions --- */

int
ml_config_menu_init(
	ml_config_menu_t *  config_menu
	)
{
	config_menu->pid = 0 ;
#ifdef  USE_WIN32API
	config_menu->fd = 0 ;
#else
	config_menu->fd = -1 ;

	kik_add_sig_child_listener( config_menu , sig_child) ;
#endif
#ifdef  USE_LIBSSH2
	config_menu->pty = NULL ;
#endif

	return  1 ;
}

int
ml_config_menu_final(
	ml_config_menu_t *  config_menu
	)
{
#ifndef  USE_WIN32API
	kik_remove_sig_child_listener( config_menu , sig_child) ;
#endif

	return  1 ;
}

int
ml_config_menu_start(
	ml_config_menu_t *  config_menu ,
	char *  cmd_path ,			/* Not contains ".exe" in win32. */
	int  x ,
	int  y ,
	char *  display ,
	ml_pty_ptr_t  pty
	)
{
#ifdef  USE_WIN32API

	HANDLE  input_write_tmp ;
	HANDLE  input_read ;
	HANDLE  output_write ;
	HANDLE  error_write ;
	SECURITY_ATTRIBUTES  sa ;
	PROCESS_INFORMATION  pi ;
	STARTUPINFO  si ;
	char *  cmd_line ;
	char  geometry[] = "--geometry" ;
	DWORD  tid ;
	int  pty_fd ;

	if( config_menu->pid > 0)
	{
		/* configuration menu is active now */
		
		return  0 ;
	}

	input_read = output_write = error_write = 0 ;

	if( ( pty_fd = ml_pty_get_slave_fd( pty)) == -1)
	{
	#ifdef  USE_LIBSSH2
		ml_pty_use_loopback( pty) ;
		pty_fd = ml_pty_get_slave_fd( pty) ;
		config_menu->pty = pty ;
	#else
		return  0 ;
	#endif
	}

	/*
	 * pty_fd is not inheritable(see ml_pty_pipewin32.c).
	 * So it is necessary to duplicate inheritable handle.
	 */
	if( ! DuplicateHandle( GetCurrentProcess() , pty_fd ,
			       GetCurrentProcess() , &output_write , 0 ,
			       TRUE , DUPLICATE_SAME_ACCESS))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " DuplicateHandle() failed.\n") ;
	#endif

		return  0 ;
	}

	if( ! DuplicateHandle( GetCurrentProcess() , pty_fd ,
			       GetCurrentProcess() , &error_write , 0 ,
			       TRUE , DUPLICATE_SAME_ACCESS))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " DuplicateHandle() failed.\n") ;
	#endif

		goto  error1 ;
	}
	
	/* Set up the security attributes struct. */
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	/* Create the child input pipe. */
	if( ! CreatePipe( &input_read , &input_write_tmp , &sa , 0))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " CreatePipe() failed.\n") ;
	#endif

		goto  error1 ;
	}

	if( ! DuplicateHandle( GetCurrentProcess() , input_write_tmp ,
			GetCurrentProcess() , &config_menu->fd , /* Address of new handle. */
			0 ,
			FALSE , /* Make it uninheritable. */
			DUPLICATE_SAME_ACCESS))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " DuplicateHandle() failed.\n") ;
	#endif

		CloseHandle( input_write_tmp) ;

		goto  error1 ;
	}

	/*
	 * Close inheritable copies of the handles you do not want to be
	 * inherited.
	 */
	CloseHandle(input_write_tmp) ;

	/* Set up the start up info struct. */
	ZeroMemory(&si,sizeof(STARTUPINFO)) ;
	si.cb = sizeof(STARTUPINFO) ;
	si.dwFlags = STARTF_USESTDHANDLES ;
	si.hStdOutput = output_write ;
	si.hStdInput  = input_read ;
	si.hStdError  = error_write ;

	/*
	 * Use this if you want to hide the child:
	 *  si.wShowWindow = SW_HIDE;
	 * Note that dwFlags must include STARTF_USESHOWWINDOW if you want to
	 * use the wShowWindow flags.
	 */

	if( ( cmd_line = alloca( strlen( cmd_path) + 1 + sizeof( geometry) +
				(1 + DIGIT_STR_LEN(int)) * 2 + 1)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca failed.\n") ;
	#endif
	
		goto  error1 ;
	}

	sprintf( cmd_line , "%s %s +%d+%d" , cmd_path , geometry , x , y) ;

	if( ! CreateProcess( cmd_path , cmd_line , NULL , NULL , TRUE , CREATE_NO_WINDOW ,
		NULL, NULL, &si, &pi))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " CreateProcess() failed.\n") ;
	#endif

		goto  error1 ;
	}

	/* Set global child process handle to cause threads to exit. */
	config_menu->pid = (pid_t)pi.hProcess ;

	/* Close any unnecessary handles. */
	CloseHandle( pi.hThread) ;

	/*
	 * Close pipe handles (do not continue to modify the parent).
	 * You need to make sure that no handles to the write end of the
	 * output pipe are maintained in this process or else the pipe will
	 * not close when the child process exits and the ReadFile will hang.
	 */
	CloseHandle(output_write) ;
	CloseHandle(input_read) ;
	CloseHandle(error_write) ;

	if( ! CreateThread( NULL , 0 , wait_child_exited , config_menu , 0 , &tid))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " CreateThread() failed.\n") ;
	#endif

		goto  error2 ;
	}

	return  1 ;

error1:
	if( input_read)
	{
		CloseHandle( input_read) ;
	}

	if( output_write)
	{
		CloseHandle( output_write) ;
	}

	if( error_write)
	{
		CloseHandle( error_write) ;
	}

error2:
	if( config_menu->pid)
	{
		/*
		 * TerminateProcess must be called before CloseHandle(fd).
		 */
		TerminateProcess( config_menu->pid , 0) ;
		config_menu->pid = 0 ;
	}

	if( config_menu->fd)
	{
		CloseHandle( config_menu->fd) ;
		config_menu->fd = -1 ;
	}

	return  0 ;

#else	/* USE_WIN32API */

	pid_t  pid ;
	int  fds[2] ;
	int  pty_fd ;

	if( config_menu->pid > 0)
	{
		/* configuration menu is active now */
		
		return  0 ;
	}

	if( ( pty_fd = ml_pty_get_slave_fd( pty)) == -1)
	{
	#ifdef  USE_LIBSSH2
		ml_pty_use_loopback( pty) ;
		pty_fd = ml_pty_get_slave_fd( pty) ;
		config_menu->pty = pty ;
	#else
		return  0 ;
	#endif
	}

	if( ! kik_file_unset_cloexec( pty_fd))
	{
		/* configulators require an inherited pty. */
		return  0 ;
	}

	if( pipe( fds) == -1)
	{
		return  0 ;
	}

	pid = fork() ;
	if( pid == -1)
	{
		return  0 ;
	}

	if( pid == 0)
	{
		/* child process */

		char *  args[6] ;
		char  geom[2 + DIGIT_STR_LEN(int)*2 + 1] ;

		args[0] = cmd_path ;
		
		args[1] = "--display" ;
		args[2] = display ;

		sprintf( geom , "+%d+%d" , x , y) ;
		
		args[3] = "--geometry" ;
		args[4] = geom ;
		
		args[5] = NULL ;

		close( fds[1]) ;

		/* for configulators,
		 * STDIN => to read replys from mlterm
		 * STDOUT => to write the "master" side of pty
		 * STDERR => is retained to be the mlterm's STDERR
		 */
		if( dup2( fds[0] , STDIN_FILENO) == -1 || dup2( pty_fd , STDOUT_FILENO) == -1)
		{
			kik_msg_printf( "dup2 failed.\n") ;

			exit(1) ;
		}
		
	       	if( execv( cmd_path , args) == -1)
		{
			/* failed */

			/* specified program name without path. */
			if( strchr( cmd_path , '/') == NULL)
			{
				char *  p ;
				char  dir[] = LIBEXECDIR ;

				/* not freed, since this process soon execv() */
				if( ( p = malloc( sizeof(dir) + strlen( cmd_path) + 1)) == NULL)
				{
					exit(1) ;
				}

				sprintf( p , "%s/%s" , dir , cmd_path) ;

				args[0] = cmd_path = p ;

				if( execv( cmd_path , args) == -1)
				{
					kik_msg_printf( "%s is not found.\n" , cmd_path) ;

					exit(1) ;
				}
			}

			exit(1) ;
		}
	}

	/* parent process */

	close( fds[0]) ;

	config_menu->fd = fds[1] ;
	config_menu->pid = pid ;
	
	kik_file_set_cloexec( pty_fd) ;
	kik_file_set_cloexec( config_menu->fd) ;

	return  1 ;

#endif	/* USE_WIN32API */
}

int
ml_config_menu_write(
	ml_config_menu_t *  config_menu ,
	u_char *  buf ,
	size_t  len
	)
{
	ssize_t  write_len ;

#ifdef  USE_WIN32API
	if( config_menu->fd == 0)
#else
	if( config_menu->fd == -1)
#endif
	{
		return  0 ;
	}

#ifdef  USE_WIN32API
	if( ! WriteFile( config_menu->fd , buf , len , &write_len , NULL))
#else
	if( ( write_len = write( config_menu->fd , buf , len)) == -1)
#endif
	{
		return  0 ;
	}
	else
	{
		return  write_len ;
	}
}
