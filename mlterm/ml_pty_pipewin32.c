/*
 *	$Id$
 */

#include  "ml_pty_intern.h"

#include  <windows.h>
#include  <stdio.h>
#include  <unistd.h>		/* close */
#include  <string.h>		/* strchr/memcpy */
#include  <stdlib.h>		/* putenv/alloca */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* realloc/alloca */
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_pty.h>
#include  <kiklib/kik_sig_child.h>
#include  <kiklib/kik_path.h>


#if  0
#define  __DEBUG
#endif


typedef struct ml_pty_pipe
{
	ml_pty_t  pty ;

	HANDLE  master_input ;	/* master read(stdout,stderr) */
	HANDLE  master_output ;	/* master write */
	HANDLE  slave_stdout ;	/* slave write */
	HANDLE  child_proc ;
	int8_t  is_plink ;

	u_char  rd_ch ;
	int8_t  rd_ready ;
	HANDLE  rd_ev ;

} ml_pty_pipe_t ;


static HANDLE *  child_procs ;		/* Notice: The first element is "ADDED_CHILD" event */
static DWORD  num_of_child_procs ;


/* --- static functions --- */

static DWORD WINAPI
wait_child_exited(
	LPVOID thr_param
  	)
{
	DWORD  ev ;

#ifdef  __DEBUG
  	kik_debug_printf( "Starting wait_child_exited thread.\n") ;
#endif

	while( 1)
	{
		ev = WaitForMultipleObjects( num_of_child_procs, child_procs, FALSE, INFINITE) ;

		if( ev > WAIT_OBJECT_0 && ev < WAIT_OBJECT_0 + num_of_child_procs)
		{
		#ifdef  __DEBUG
			kik_debug_printf( "%dth child exited.\n", ev) ;
		#endif

			/* XXX regarding pid_t as HANDLE */
			kik_trigger_sig_child( child_procs[ev - WAIT_OBJECT_0]) ;

			CloseHandle( child_procs[ev - WAIT_OBJECT_0]) ;
			
			child_procs[ev - WAIT_OBJECT_0] = child_procs[ --num_of_child_procs] ;
		}

		if( num_of_child_procs == 1)
		{
			break ;
		}
	}

	free( child_procs) ;
	num_of_child_procs = 0 ;
	child_procs = NULL ;

#ifdef  __DEBUG
  	kik_debug_printf( "Exiting wait_child_exited thread.\n") ;
#endif

	ExitThread( 0) ;

	return  0 ;
}

/*
 * Monitors handle for input. Exits when child exits or pipe is broken.
 */
static DWORD WINAPI
wait_pty_read(
	LPVOID thr_param
  	)
{
	ml_pty_pipe_t *  pty = (ml_pty_pipe_t*)thr_param ;
	DWORD n_rd ;

#ifdef  __DEBUG
  	kik_debug_printf( "Starting wait_pty_read thread.\n") ;
#endif

	while( 1)
	{
		if( ! ReadFile( pty->master_input, &pty->rd_ch, 1, &n_rd, NULL) || n_rd == 0)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ReadFile() failed.") ;
		#endif

			if( GetLastError() == ERROR_BROKEN_PIPE)
			{
				/*
				 * If slave_stdout member is not necessary, wait_child_exited
				 * is called here and wait_child_exited thread becomes
				 * unnecessary. (But master_input is never broken until
				 * slave_stdout is closed even if child process exited, so
				 * wait_child_exited thread is necessary.) 
				 */
			#ifdef  DEBUG
				kik_msg_printf( " ==> ERROR_BROKEN_PIPE.") ;
			#endif
			}
			
		#ifdef  DEBUG
			kik_msg_printf( "\n") ;
		#endif

			break ;
		}

		if( pty->pty.pty_listener && pty->pty.pty_listener->read_ready)
		{
			(*pty->pty.pty_listener->read_ready)( pty->pty.pty_listener->self) ;
		}

		WaitForSingleObject( pty->rd_ev, INFINITE) ;
		
		if( ! pty->child_proc)
		{
			break ;
		}

	#ifdef  __DEBUG
		kik_debug_printf( "Exit WaitForSingleObject\n") ;
	#endif
	}

#ifdef  __DEBUG
  	kik_debug_printf( "Exiting wait_pty_read thread.\n") ;
#endif

	ExitThread(0) ;

	return  0 ;
}

static int
pty_open(
  	ml_pty_pipe_t *  pty,
	char *  cmd_path ,
	char **  cmd_argv
	)
{
	HANDLE  output_read_tmp , output_write ;
	HANDLE  input_write_tmp , input_read ;
	HANDLE  error_write ;
	SECURITY_ATTRIBUTES  sa ;
	PROCESS_INFORMATION  pi ;
	STARTUPINFO  si ;

	output_read_tmp = input_write_tmp = output_write = input_read = error_write = 0 ;
	
	/* Set up the security attributes struct. */
	sa.nLength= sizeof(SECURITY_ATTRIBUTES) ;
	sa.lpSecurityDescriptor = NULL ;
	sa.bInheritHandle = TRUE ;
	
	/* Create the child output pipe. */
	if( ! CreatePipe( &output_read_tmp , &output_write , &sa , 0))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " CreatePipe() failed.\n") ;
	#endif

		return  0 ;
	}

	/*
	 * Create a duplicate of the output write handle for the std error
	 * write handle. This is necessary in case the child application
	 * closes one of its std output handles.
	 */
	if( ! DuplicateHandle( GetCurrentProcess() , output_write ,
			       GetCurrentProcess() , &error_write , 0 ,
			       TRUE , DUPLICATE_SAME_ACCESS))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " DuplicateHandle() failed.\n") ;
	#endif

		goto  error1 ;
	}

	/* Create the child input pipe. */
	if( ! CreatePipe( &input_read , &input_write_tmp , &sa , 0))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " CreatePipe() failed.\n") ;
	#endif

		goto  error1 ;
	}

	/*
	 * Create new output read handle and the input write handles. Set
	 * the Properties to FALSE. Otherwise, the child inherits the
	 * properties and, as a result, non-closeable handles to the pipes
	 * are created.
	 */
	if( ! DuplicateHandle( GetCurrentProcess() , output_read_tmp ,
			GetCurrentProcess() , &pty->master_input , /* Address of new handle. */
			0 , FALSE , /* Make it uninheritable. */
			DUPLICATE_SAME_ACCESS))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " DuplicateHandle() failed.\n") ;
	#endif
	
		goto  error1 ;
	}
	else if( ! DuplicateHandle( GetCurrentProcess() , input_write_tmp ,
			GetCurrentProcess() , &pty->master_output , /* Address of new handle. */
			0 , FALSE , /* Make it uninheritable. */
			DUPLICATE_SAME_ACCESS))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " DuplicateHandle() failed.\n") ;
	#endif

		CloseHandle( pty->master_input) ;

		goto  error1 ;
	}

	/*
	 * Close inheritable copies of the handles you do not want to be
	 * inherited.
	 *
	 * !! Notice !!
	 * After here, goto error2 if error happens.
	 */
	CloseHandle(output_read_tmp) ;
	CloseHandle(input_write_tmp) ;

	/* Set up the start up info struct. */
	ZeroMemory(&si,sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = output_write;
	si.hStdInput  = input_read;
	si.hStdError  = error_write;

	/*
	 * Use this if you want to hide the child:
	 *  si.wShowWindow = SW_HIDE;
	 * Note that dwFlags must include STARTF_USESHOWWINDOW if you want to
	 * use the wShowWindow flags.
	 */

	if( cmd_argv)
	{
		int  count ;
		char *   cmd_line ;
		size_t   cmd_line_len ;

		/* Because cmd_path == cmd_argv[0], cmd_argv[0] is ignored. */

		cmd_line_len = strlen(cmd_path) + 1 ;
		for( count = 1 ; cmd_argv[count] != NULL ; count++)
		{
			cmd_line_len += (strlen(cmd_argv[count]) + 1) ;
		}

		if( ( cmd_line = alloca( sizeof(char) * cmd_line_len)) == NULL)
		{
			CloseHandle( pty->master_input) ;
			CloseHandle( pty->master_output) ;
			
			goto  error2 ;
		}

		strcpy( cmd_line, cmd_path) ;
		for( count = 1 ; cmd_argv[count] != NULL ; count ++)
		{
			strcat( cmd_line, " ") ;
			strcat( cmd_line, cmd_argv[count]) ;
		}
		
		if( ! CreateProcess( cmd_path , cmd_line , NULL , NULL , TRUE , CREATE_NO_WINDOW ,
					NULL , NULL , &si , &pi))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " CreateProcess() failed.\n") ;
		#endif

			CloseHandle( pty->master_input) ;
			CloseHandle( pty->master_output) ;
			
			goto  error2 ;
		}
	}
	else
	{
		if( ! CreateProcess( cmd_path , cmd_path , NULL , NULL , TRUE , CREATE_NO_WINDOW ,
					NULL , NULL , &si , &pi))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " CreateProcess() failed.\n") ;
		#endif

			CloseHandle( pty->master_input) ;
			CloseHandle( pty->master_output) ;
			
			goto  error2 ;
		}
	}

	pty->slave_stdout = output_write ;

	/* Set global child process handle to cause threads to exit. */
	pty->child_proc = pi.hProcess ;

	if( strstr( cmd_path, "plink"))
	{
		pty->is_plink = 1 ;
	}
	else
	{
		pty->is_plink = 0 ;
	}

	/* close unnecessary handles. */
	CloseHandle( pi.hThread) ;
	CloseHandle( input_read) ;
	CloseHandle( error_write) ;

	return  1 ;

error1:
	if( output_read_tmp)
	{
		CloseHandle( output_read_tmp) ;
	}

	if( input_write_tmp)
	{
		CloseHandle( input_write_tmp) ;
	}

error2:	
	if( output_write)
	{
		CloseHandle( output_write) ;
	}

	if( error_write)
	{
		CloseHandle( error_write) ;
	}

	if( input_read)
	{
		CloseHandle( input_read) ;
	}
	
	return  0 ;
}

static int
delete(
	ml_pty_t *  p
	)
{
	ml_pty_pipe_t *  pty ;
	int  count ;
	DWORD  size ;

	pty = (ml_pty_pipe_t*)p ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " ml_pty_delete is called for %p.\n" , pty) ;
#endif

	/*
	 * TerminateProcess must be called before CloseHandle.
	 * If pty->child_proc is not in child_procs, pty->child_proc is already
	 * closed in wait_child_exited, so TerminateProcess is not called.
	 */
	for( count = 0 ; count < num_of_child_procs ; count++)
	{
		if( pty->child_proc == child_procs[count])
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Terminate process %d\n" ,
				pty->child_proc) ;
		#endif
			TerminateProcess( pty->child_proc , 0) ;

			break ;
		}
	}

	/* Used to check if child process is dead or not in wait_pty_read. */
	pty->child_proc = 0 ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Trying to terminate wait_pty_read thread.") ;
#endif

	/*
	 * If wait_pty_read waits in ReadFile( pty->master_input),
	 * CloseHandle( pty->master_input) causes process to be blocked.
	 * (Even if child process exits, ReadFile in wait_pty_read doesn't
	 * exit by ERROR_BROKEN_PIPE, because pty->slave_stdout remains
	 * opened in parent process.)
	 * Write a dummy data to child process, and wait_pty_read waits
	 * in WaitForSingleObject(pty->rd_ev).
	 * Then close pty->slave_stdout and set pty->rd_ev.
	 * As a result, wait_pty_read thread exits.
	 */
	WriteFile( pty->slave_stdout, "", 1, &size, NULL) ;
	CloseHandle( pty->slave_stdout) ;
	SetEvent( pty->rd_ev) ;
	
#ifdef  __DEBUG
	kik_msg_printf( " => Finished.\n") ;
#endif

	CloseHandle( pty->master_input) ;
	CloseHandle( pty->master_output) ;
	CloseHandle( pty->rd_ev) ;

	free( pty) ;

	return  1 ;
}

static int
set_winsize(
	ml_pty_t *  pty ,
	u_int  cols ,
	u_int  rows
	)
{
	if( ((ml_pty_pipe_t*)pty)->is_plink)
	{
		/*
		 * XXX Hack
		 */
		 
		u_char  opt[5] ;

		opt[0] = 0xff ;
		opt[1] = (cols >> 8) & 0xff ;
		opt[2] = cols & 0xff ;
		opt[3] = (rows >> 8) & 0xff ;
		opt[4] = rows & 0xff ;

		ml_write_to_pty( pty, opt, 5) ;

		return  1 ;
	}

	return  0 ;
}

/*
 * Return size of lost bytes.
 */
static ssize_t
write_to_pty(
	ml_pty_t *  pty ,
	u_char *  buf ,
	size_t  len
	)
{
	DWORD  written_size ;

	if( ! WriteFile( ((ml_pty_pipe_t*)pty)->master_output , buf , len ,
			&written_size , NULL))
	{
                if( GetLastError() == ERROR_BROKEN_PIPE)
                {
			return  -1 ;
                }
        }

	FlushFileBuffers( ((ml_pty_pipe_t*)pty)->master_output) ;

	return  written_size ;
}

static ssize_t
read_pty(
	ml_pty_t *  p ,
	u_char *  buf ,
	size_t  len
	)
{
	ml_pty_pipe_t *  pty ;
	ssize_t  n_rd ;

	pty = (ml_pty_pipe_t*)p ;

	if( pty->rd_ch == '\0' && ! pty->rd_ready)
	{
		return  0 ;
	}

	if( pty->rd_ch != '\0')
	{
		buf[0] = pty->rd_ch ;
		n_rd = 1 ;
		len -- ;
		pty->rd_ch = '\0' ;
		pty->rd_ready = 1 ;
	}
	else
	{
		n_rd = 0 ;
	}

	while( len > 0)
	{
		DWORD  ret ;

		if( ! PeekNamedPipe( pty->master_input, NULL, 0, NULL, &ret, NULL) || ret == 0 ||
			! ReadFile( pty->master_input, &buf[n_rd], len, &ret, NULL) || ret == 0)
		{
			break ;
		}

		n_rd += ret ;
		len -= ret ;
	}

	if( n_rd == 0)
	{
		SetEvent( pty->rd_ev) ;
		pty->rd_ready = 0 ;
	}
#if  0
	else
	{
		int  i ;
		for( i = 0 ; i < n_rd ; i++)
		{
			if( buf[i] == 0xff)
			{
				kik_msg_printf( "Server Option => ") ;
				kik_msg_printf( "%d ", buf[i++]) ;
				if( i < n_rd) printf( "%d ", buf[i++]) ;
				if( i < n_rd) printf( "%d ", buf[i++]) ;
				if( i < n_rd) printf( "%d ", buf[i++]) ;
				if( i < n_rd) printf( "%d ", buf[i++]) ;
			}
		}
	}
#endif

	return  n_rd ;
}


/* --- global functions --- */

ml_pty_t *
ml_pty_pipe_new(
	char *  cmd_path ,	/* can be NULL */
	char **  cmd_argv ,	/* can be NULL(only if cmd_path is NULL) */
	char **  env ,		/* can be NULL */
	char *  uri ,
	char *  pass ,
	u_int  cols ,
	u_int  rows
	)
{
	ml_pty_pipe_t *  pty ;
	HANDLE  thrd ;
	DWORD  tid ;
	char  ev_name[25] ;
	void *  p ;
	char *  user ;
	char *  proto ;
	char *  host ;
	char *  port ;
	int  idx ;

	if( num_of_child_procs == 0)
	{
		/*
		 * Initialize child_procs array.
		 */

		if( ( child_procs = malloc( sizeof(HANDLE))) == NULL)
		{
			return  NULL ;
		}

		child_procs[0] = CreateEvent( NULL, FALSE, FALSE, "ADDED_CHILD") ;
		num_of_child_procs = 1 ;

		/* Launch the thread that wait for child exited. */
		if( ! ( thrd = CreateThread( NULL , 0 , wait_child_exited , NULL , 0 , &tid)))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " CreateThread() failed.\n") ;
		#endif

			return  NULL ;
		}

		CloseHandle( thrd) ;
	}

	if( ( pty = malloc( sizeof( ml_pty_pipe_t))) == NULL)
	{
		return  NULL ;
	}

	if( env)
	{
		while( *env)
		{
			char *  p ;
			char *  key ;
			char *  val ;

			if( ( key = kik_str_alloca_dup( *env)) && ( p = strchr( key, '=')))
			{
				*p = '\0' ;
				val = ++p ;
				SetEnvironmentVariable( key, val) ;
			#ifdef  __DEBUG
				kik_debug_printf( "Env: %s=%s\n" , key , val) ;
			#endif
			}

			env ++ ;
		}
	}

	if( /* cmd_path && */ cmd_argv)
	{
		goto  next_step ;
	}

	if( ! ( cmd_argv = alloca( sizeof(char*) * 8)) ||
	    ! kik_parse_uri( &proto , &user , &host , &port , NULL , NULL ,
		kik_str_alloca_dup( uri)))
	{
		free( pty) ;

		return  NULL ;
	}

	if( proto && ( p = alloca( strlen( proto) + 2)))
	{
		sprintf( p , "-%s" , proto) ;
		proto = p ;
	}

	cmd_path = "plink.exe" ;

	idx = 0 ;
	cmd_argv[idx++] = cmd_path ;
	cmd_argv[idx++] = proto ;
	if( user)
	{
		cmd_argv[idx++] = "-l" ;
		cmd_argv[idx++] = user ;
	}
	/* -pw option can only be used with SSH. */
	if( strcmp( proto , "-ssh") == 0)
	{
		cmd_argv[idx++] = "-pw" ;
		cmd_argv[idx++] = pass ;
	}
	cmd_argv[idx++] = host ;
	cmd_argv[idx++] = NULL ;

next_step:
	if( ! ( pty_open( pty, cmd_path, cmd_argv)))
	{
		free(pty) ;

		return  NULL ;
	}

	pty->rd_ch = '\0' ;
	pty->rd_ready = 0 ;

	snprintf( ev_name, sizeof(ev_name), "PTY_READ_READY%x", (int)pty->child_proc) ;
	pty->rd_ev = CreateEvent( NULL, FALSE, FALSE, ev_name) ;
#ifdef  __DEBUG
	kik_debug_printf( "Created pty read event: %s\n", ev_name) ;
#endif

	pty->pty.master = (int)pty->master_output ;	/* XXX Cast HANDLE => int */
	pty->pty.slave = (int)pty->slave_stdout ;	/* XXX Cast HANDLE => int */
	pty->pty.child_pid = (pid_t)pty->child_proc ;	/* Cast HANDLE => pid_t */
	pty->pty.buf = NULL ;
	pty->pty.left = 0 ;
	pty->pty.size = 0 ;
	pty->pty.delete = delete ;
	pty->pty.set_winsize = set_winsize ;
	pty->pty.write = write_to_pty ;
	pty->pty.read = read_pty ;

	pty->pty.pty_listener = NULL ;

	if( set_winsize( &pty->pty , cols , rows) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_set_pty_winsize() failed.\n") ;
	#endif
	}

	/* Launch the thread that read the child's output. */
	if( ! ( thrd = CreateThread( NULL , 0 , wait_pty_read , (LPVOID)pty , 0 , &tid)))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " CreateThread() failed.\n") ;
	#endif

		ml_pty_delete( &pty->pty) ;

		return  NULL ;
	}

	CloseHandle( thrd) ;

	/* Add to child_procs */

	if( ( p = realloc( child_procs, sizeof(HANDLE) * (num_of_child_procs + 1))) == NULL)
	{
		ml_pty_delete( &pty->pty) ;

		return  NULL ;
	}

	child_procs = p ;
	child_procs[num_of_child_procs++] = pty->child_proc ;

	/*
	 * Exit WaitForMultipleObjects in wait_child_proc and do WaitForMultipleObjects again
	 * with new child_procs
	 */
	SetEvent( child_procs[0]) ;

#ifdef  __DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " Added child procs NUM %d ADDED-HANDLE %d:%d.\n",
			num_of_child_procs, child_procs[num_of_child_procs - 1], pty->child_proc) ;
#endif

	return  &pty->pty ;
}
