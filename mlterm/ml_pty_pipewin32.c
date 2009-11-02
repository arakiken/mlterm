/*
 *	$Id$
 */

#include  "ml_pty.h"

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


#if  0
#define  __DEBUG
#endif

typedef struct ml_pty
{
  	HANDLE  master_input ;	/* master read(stdout,stderr) */
  	HANDLE  master_output ;	/* master write */
  	HANDLE  slave_stdout ;	/* slave write */
	HANDLE  child_proc ;
  	int8_t  is_plink ;

  	u_char  rd_ch ;
  	int8_t  rd_ready ;

	/* model to be written */
	u_char *  buf ;
	size_t  left ;
	size_t  size ;

	HANDLE  rd_ev ;

  	ml_pty_event_listener_t *  pty_listener ;

} ml_pty_t ;


static HANDLE *  child_procs ;		/* Notice: The first element is "ADDED_CHILD" event */
static DWORD  num_of_child_procs ;


/* --- static functions --- */

static DWORD WINAPI
wait_child_exited(
	LPVOID thr_param
  	)
{
	DWORD  ev ;

#if  0
  	kik_debug_printf( "wait_child_exited thread.\n") ;
#endif

  	while( 1)
        {
          	ev = WaitForMultipleObjects( num_of_child_procs, child_procs, FALSE, INFINITE) ;

	#if  0
          	kik_debug_printf( "WaitForMultipleObjects %dth event signaled.\n", ev) ;
	#endif
                
          	if( ev > WAIT_OBJECT_0 && ev < WAIT_OBJECT_0 + num_of_child_procs)
                {
                  	/* XXX regarding pid_t as HANDLE */
          		kik_trigger_sig_child( (pid_t)child_procs[ev - WAIT_OBJECT_0]) ;
                
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
 
  	ExitThread( 0) ;

  	return  0 ;
}

/*
 * Monitors handle for input. Exits when child exits or pipe breaks.
 */
static DWORD WINAPI
wait_pty_read(
	LPVOID thr_param
  	)
{
  	ml_pty_t *  pty = (ml_pty_t*)thr_param ;
	DWORD n_rd ;

  	while( 1)
	{
          	if( ! ReadFile( pty->master_input, &pty->rd_ch, 1, &n_rd, NULL) || n_rd == 0)
                {
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ReadFile() failed.\n") ;
		#endif
                  	
                	if( GetLastError() == ERROR_BROKEN_PIPE)
                        {
                          	/*
                                 * XXX
                                 * If slave_stdout member is not necessary, do procedure in
				 * wait_child_exited here.
                                 * Then, wait_child_exited thread becomes unnecessary.
                                 */
			#ifdef  DEBUG
                          	kik_warn_printf( KIK_DEBUG_TAG " ==> ERROR_BROKEN_PIPE.\n") ;
			#endif
                        }

                  	break ;
         	}

	#ifdef  __DEBUG
  		kik_debug_printf( "ReadFile\n") ;
	#endif

          	if( pty->pty_listener && pty->pty_listener->read_ready)
                {
          		(*pty->pty_listener->read_ready)( pty->pty_listener->self) ;
                }
          
          	WaitForSingleObject( pty->rd_ev, INFINITE) ;

	#ifdef  __DEBUG
          	kik_debug_printf( "Exit WaitForSingleObject\n") ;
	#endif
     	}

	ExitThread(0) ;

  	return  0 ;
}

static int
pty_open(
  	ml_pty_t *  pty,
	char *  cmd_path ,
	char **  cmd_argv
	)
{
      	HANDLE ouput_read_tmp,output_write;
      	HANDLE input_write_tmp,input_read;
      	HANDLE error_write;
      	SECURITY_ATTRIBUTES sa;
      	PROCESS_INFORMATION pi;
      	STARTUPINFO si;
  	char *  cmd_line ;
  	size_t  cmd_line_len ;

     	/* Set up the security attributes struct. */
      	sa.nLength= sizeof(SECURITY_ATTRIBUTES);
      	sa.lpSecurityDescriptor = NULL;
      	sa.bInheritHandle = TRUE;

      	/* Create the child output pipe. */
      	if( ! CreatePipe(&ouput_read_tmp,&output_write,&sa,0))
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
	if( ! DuplicateHandle(GetCurrentProcess(),output_write,
                              GetCurrentProcess(),&error_write,0,
                              TRUE,DUPLICATE_SAME_ACCESS))
        {
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " DuplicateHandle() failed.\n") ;
	#endif

          	/* XXX Close already opened pipe etc. */
        	return  0 ;
        }
  
      	/* Create the child input pipe. */
      	if( ! CreatePipe(&input_read,&input_write_tmp,&sa,0))
        {
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " CreatePipe() failed.\n") ;
	#endif
          
          	/* XXX Close already opened pipe etc. */
        	return  0 ;
        }
  
	/*
	 * Create new output read handle and the input write handles. Set
      	 * the Properties to FALSE. Otherwise, the child inherits the
         * properties and, as a result, non-closeable handles to the pipes
      	 * are created.
         */
      	if( ! DuplicateHandle(GetCurrentProcess(),ouput_read_tmp,
                           GetCurrentProcess(),&pty->master_input, /* Address of new handle. */
                           0,FALSE, /* Make it uninheritable. */
                           DUPLICATE_SAME_ACCESS) ||
      	    ! DuplicateHandle(GetCurrentProcess(),input_write_tmp,
                           GetCurrentProcess(),
                           &pty->master_output, /* Address of new handle. */
                           0,FALSE, /* Make it uninheritable. */
                           DUPLICATE_SAME_ACCESS))
        {
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " DuplicateHandle() failed.\n") ;
	#endif
          	/* XXX Close already opened pipe etc. */
        	return  0 ;
        }

      	/*
	 * Close inheritable copies of the handles you do not want to be
      	 * inherited.
         */
      	if( !CloseHandle(ouput_read_tmp) || !CloseHandle(input_write_tmp))
        {
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " CloseHandle() failed.\n") ;
	#endif
          	/* XXX Close already opened pipe etc. */
        	return  0 ;
        }

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

		/* Because cmd_path == cmd_argv[0], cmd_argv[0] is ignored. */
		
		cmd_line_len = strlen(cmd_path) + 1 ;
  		for( count = 1 ; cmd_argv[count] != NULL ; count++)
        	{
          		cmd_line_len += (strlen(cmd_argv[count]) + 1) ;
        	}

  		if( ( cmd_line = alloca( sizeof(char) * cmd_line_len)) == NULL)
        	{
          		return  0 ;
        	}

  		strcpy( cmd_line, cmd_path) ;
		for( count = 1 ; cmd_argv[count] != NULL ; count ++)
        	{
          		strcat( cmd_line, " ") ;
          		strcat( cmd_line, cmd_argv[count]) ;
        	}
        }
  	else
        {
          	cmd_line = cmd_path ;
        }

	if( ! CreateProcess(cmd_path, cmd_line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        {
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " CreateProcess() failed.\n") ;
	#endif

	 	/* XXX Close already opened pipe etc. */
        	return  0 ;
        }
        
  	/* Set global child process handle to cause threads to exit. */
  	pty->child_proc = pi.hProcess;

  	if( strstr( cmd_line, "plink"))
        {
          	pty->is_plink = 1 ;
        }
  	else
        {
          	pty->is_plink = 0 ;
        }

	/* Close any unnecessary handles. */
  	if( !CloseHandle(pi.hThread))
        {
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " CloseHandle() failed.\n") ;
	#endif

          	/* XXX Close already opened pipe etc. */
        	return  0 ;
        }

      	/*
	 * Close pipe handles (do not continue to modify the parent).
      	 * You need to make sure that no handles to the write end of the
      	 * output pipe are maintained in this process or else the pipe will
      	 * not close when the child process exits and the ReadFile will hang.
         */
      	if( !CloseHandle(output_write) || !CloseHandle(input_read) || !CloseHandle(error_write))
        {
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " CloseHandle() failed.\n") ;
	#endif

          	/* XXX Close already opened pipe etc. */
        	return  0 ;
        }
  
	return  1 ;
}


/* --- global functions --- */

ml_pty_t *
ml_pty_new(
	char *  cmd_path ,
	char **  cmd_argv ,
	char **  env ,
	char *  host ,
	u_int  cols ,
	u_int  rows
	)
{
	ml_pty_t *  pty ;
      	DWORD  tid ;
  	char  ev_name[25] ;
  	void *  p ;

  	if( num_of_child_procs == 0)
        {
          	/*
                 * Initialize child_procs array.
                 */
          
          	if( ( child_procs = malloc( sizeof(HANDLE))) == NULL)
                {
                  	return  NULL ;
                }

		child_procs[0] = CreateEvent(NULL, FALSE, FALSE, "ADDED_CHILD") ;
          	num_of_child_procs = 1 ;
        
   		/* Launch the thread that wait for child exited. */
      		if( ! CreateThread(NULL,0,wait_child_exited,NULL,0,&tid))
      		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " CreateThread() failed.\n") ;
		#endif

        		return  NULL ;
      		}
	}

	if( ( pty = malloc( sizeof( ml_pty_t))) == NULL)
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

  	if( ! ( pty_open( pty, cmd_path, cmd_argv)))
        {
          	free(pty) ;
        
          	return  NULL ;
        }
  
  	/* Launch the thread that read the child's output. */
      	if( ! CreateThread(NULL,0,wait_pty_read,(LPVOID)pty,0,&tid))
      	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " CreateThread() failed.\n") ;
	#endif

  		ml_pty_delete(pty) ;

        	return  NULL ;
      	}

  	pty->rd_ch = '\0' ;
  	pty->rd_ready = 0 ;

	pty->buf = NULL ;
	pty->left = 0 ;
	pty->size = 0 ;

        snprintf( ev_name, sizeof(ev_name), "PTY_READ_READY%x", (int)pty->child_proc) ;
  	pty->rd_ev = CreateEvent(NULL, FALSE, FALSE, ev_name) ;

#ifdef  __DEBUG
  	kik_debug_printf( "pty read event name %s\n", ev_name) ;
#endif

  	pty->pty_listener = NULL ;

	if( ml_set_pty_winsize( pty , cols , rows) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_set_pty_winsize() failed.\n") ;
	#endif
	}

	/* Add to child_procs */
  
        if( ( p = realloc( child_procs, sizeof(HANDLE) * (num_of_child_procs + 1))) == NULL)
        {
                ml_pty_delete( pty) ;

                return  NULL ;
        }

        child_procs = p ;
        child_procs[num_of_child_procs++] = pty->child_proc ;

        SetEvent( child_procs[0]) ;

#ifdef  __DEBUG
        kik_warn_printf( KIK_DEBUG_TAG " Added child procs NUM %d ADDED-HANDLE %d:%d.\n",
                	num_of_child_procs, child_procs[num_of_child_procs - 1], pty->child_proc) ;
#endif

	return  pty ;
}

int
ml_pty_delete(
	ml_pty_t *  pty
	)
{
  	if( pty->pty_listener && pty->pty_listener->closed)
        {
          	(*pty->pty_listener->closed)( pty->pty_listener->self) ;
        }

  	CloseHandle( pty->master_input) ;
  	CloseHandle( pty->master_output) ;
  	CloseHandle( pty->slave_stdout) ;
  	CloseHandle( pty->rd_ev) ;
	
	TerminateProcess( pty->child_proc , 0) ;
	
	free( pty->buf) ;
	free( pty) ;

	return  1 ;
}

int
ml_pty_set_listener(
  	ml_pty_t *  pty,
  	ml_pty_event_listener_t *  pty_listener
	)
{
  	pty->pty_listener = pty_listener ;

  	return  1 ;
}

int
ml_set_pty_winsize(
	ml_pty_t *  pty ,
	u_int  cols ,
	u_int  rows
	)
{
  	if( pty->is_plink)
        {
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
size_t
ml_write_to_pty(
	ml_pty_t *  pty ,
	u_char *  buf ,
	size_t  len
	)
{
	u_char *  w_buf ;
	size_t  w_buf_size ;
	DWORD  written_size ;
	void *  p ;

	w_buf_size = pty->left + len ;
	if( w_buf_size == 0)
	{
		return  0 ;
	}
#if  0
	/*
	 * Little influence without this buffering.
	 */
	else if( len > 0 && w_buf_size < 16)
	{
		/*
		 * Buffering until 16 bytes.
		 */

		if( pty->size < 16)
		{
			if( ( p = realloc( pty->buf , 16)) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" realloc failed. %d characters not written.\n" , len) ;
			#endif

				return  len ;
			}
			
			pty->size = 16 ;
			pty->buf = p ;
		}

		memcpy( &pty->buf[pty->left] , buf , len) ;
		pty->left = w_buf_size ;

	#if  0
		kik_debug_printf( "buffered(not written) %d characters.\n" , pty->left) ;
	#endif
	
		return  0 ;
	}
#endif

	if( /* pty->buf && */ len == 0)
	{
		w_buf = pty->buf ;
	}
  	else if( pty->buf == NULL && pty->left == 0)
        {
          	w_buf = buf ;
        }
  	else if( ( w_buf = alloca( w_buf_size)))
        {
          	memcpy( w_buf , pty->buf , pty->left) ;
		memcpy( &w_buf[pty->left] , buf , len) ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" alloca() failed. %d characters not written.\n" , len) ;
	#endif
	
		return  len ;
	}

#ifdef  __DEBUG
	{
		int  i ;
		for( i = 0 ; i < w_buf_size ; i++)
		{
			kik_msg_printf( "%.2x" , w_buf[i]) ;
		}
		kik_msg_printf( "\n") ;
	}
#endif

	if( ! WriteFile( pty->master_output, w_buf, w_buf_size, &written_size, NULL))
	{
                if( GetLastError() == ERROR_BROKEN_PIPE)
                {
			pty->left = 0 ;
			
                        return  0 ; /* pipe done - normal exit path. */
                }
		
		kik_warn_printf( KIK_DEBUG_TAG " WriteFile() failed.\n") ;
		written_size = 0 ;
        }

	FlushFileBuffers( pty->master_output) ;

	if( written_size == w_buf_size)
	{
		pty->left = 0 ;
	
		return  0 ;
	}

	/* w_buf_size - written_size == not_written_size */
	if( w_buf_size - written_size > pty->size)
	{
		if( ( p = realloc( pty->buf , w_buf_size - written_size)) == NULL)
		{
			size_t  lost ;
			
			if( pty->size == 0)
			{
				lost = w_buf_size - written_size ;
				pty->left = 0 ;
			}
			else
			{
				lost = w_buf_size - written_size - pty->size ;
				memcpy( pty->buf , &w_buf[written_size] , pty->size) ;
				pty->left = pty->size ;
			}

		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" realloc failed. %d characters are not written.\n" , lost) ;
		#endif

			return  lost ;
		}
		else
		{
			pty->size = pty->left = w_buf_size - written_size ;
			pty->buf = p ;
		}
	}
	else
	{
		pty->left = w_buf_size - written_size ;
	}
	
	memcpy( pty->buf , &w_buf[written_size] , pty->left) ;

	kik_debug_printf( "%d is not written.\n" , pty->left) ;

	return  0 ;
}

/*
 * Flush pty->buf/pty->left.
 */
size_t
ml_flush_pty(
	ml_pty_t *  pty
	)
{
	return  ml_write_to_pty( pty, NULL, 0) ;
}

size_t
ml_read_pty(
	ml_pty_t *  pty ,
	u_char *  buf ,
	size_t  left		/* buffer length */
	)
{
  	size_t  n_rd ;

  	if( pty->rd_ch == '\0' && ! pty->rd_ready)
        {
        	return  0 ;
        }

  	if( pty->rd_ch != '\0')
        {
		buf[0] = pty->rd_ch ;
          	n_rd = 1 ;
          	left -- ;
          	pty->rd_ch = '\0' ;
          	pty->rd_ready = 1 ;
        }
  	else
        {
            	n_rd = 0 ;
        }

	while( left > 0)
	{
		DWORD  ret ;

          	if( ! PeekNamedPipe( pty->master_input, NULL, 0, NULL, &ret, NULL) || ret == 0 ||
                	! ReadFile( pty->master_input, &buf[n_rd], left, &ret, NULL) || ret == 0)
                {
                	break ;
                }
          
		n_rd += ret ;
		left -= ret ;
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

/*
 * Return child process HANDLE as pid_t.
 * Don't trust return value as valid pid_t.
 */
pid_t
ml_pty_get_pid(
  	ml_pty_t *  pty
  	)
{
  	/* Cast HANDLE => pid_t */
  	return  (pid_t)pty->child_proc ;
}

/*
 * DUMMY
 */
int
ml_pty_get_master_fd(
	ml_pty_t *  pty
	)
{
  	/* _open_osfhandle( pty->master_output, 0) */
	return  0 ;
}

/*
 * DUMMY
 */
int
ml_pty_get_slave_fd(
	ml_pty_t *  pty
	)
{
  	/* _open_osfhandle( pty->slave_stdout, 0) */
	return  0 ;
}

/*
 * DUMMY
 */
char *
ml_pty_get_slave_name(
	ml_pty_t *  pty
	)
{
	return  "" ;
}
