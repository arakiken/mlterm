/*
 * $Id$
 */

#include  <windows.h>
#include  <kiklib/kik_mem.h>
#include  "ml_pty.h"

#define  IO_BUFSIZE 4096

typedef struct io_thread_parm
{
	/* following data shared in main and I/O thread. */
	u_char  buf[IO_BUFSIZE] ;
	HANDLE  pipe ;
	HANDLE  ev_start ; 	/* This event is set by main thread. */
	HANDLE  ev_ready ; 	/* This event is set by I/O thread. */
	int  is_exit_request ;	/* 0: start I/O, 1: exit */
	int  io_failed ;	/* ReadFile()/WriteFile() failed? */
	DWORD  left ;		/* size of data in buf[] */
	DWORD  lasterr ;	/* hold last I/O error number */
} io_thread_parm_t ;

typedef struct ml_win32_pty
{
	ml_pty_t  ml_pty ;

	/*
	 * w(write): pearent --> client(stdin)
	 * r(read) : pearent <-- client(stdout)
	 * e(error): pearent <-- client(stderr)
	 */
	HANDLE  wthread ;
	HANDLE  rthread ;
	HANDLE  ethread ;
	io_thread_parm_t  wparm ;
	io_thread_parm_t  rparm ;
	io_thread_parm_t  eparm ;
	HANDLE  child ;
	HANDLE  child_thread ;
} ml_pty_win32_t ;

/*
 * Mlterm-win32 uses pty helper programs instead of UNIX pty devices. If the
 * helper program is an ssh client like Plink(PuTTY Link), mlterm will also
 * become an ssh client, along with a multilingual terminal.
 *
 * This file 'ml_pty_win32.c' provides the internal interfaces to access helper
 * programs, and work with abstract pty structure ml_pty_t. These interfaces
 * are same as ml_pty.c handling the UNIX pty, so that both interfaces can be
 * called in the same way through the higher terminal interfaces in ml_term.c.
 * Exceptionally, the handles in ml_pty_win32_t are directlly used by Win32 GUI
 * layer, not through the common interfaces.
 *
 * Also when a helper program is invoked as a child process, three threads
 * will be spawned in mlterm, and standard I/O handles(stdin/stdout/stderr) of
 * the helper program are redirected to each thread. Therefore, the helper
 * program must be a console-based program which has standard I/O handles, as
 * shown in below figure.
 *
 *  .-----------,                .-----------,             .------------,
 *  |           |--- ev_start -->| io thread |             |            |
 *  |           |<-- ev_ready ---|  (write)  |--- stdin -->|            |
 *  |           |                `-----------'             |            |
 *  | primary   |                .-----------,             |            |
 *  |   thread  |--- ev_start -->| io thread |             | child      |
 *  |           |<-- ev_ready ---|  (read)   |<-- stdout --|   process  |
 *  |           |                `-----------'             |            |
 *  |           |                .-----------,             |            |
 *  |           |--- ev_start -->| io thread |             |            |
 *  |           |<-- ev_ready ---|  (error)  |<-- stderr --|            |
 *  `-----------'                `-----------'             `------------'
 *               \_______.______/             \______.____/
 *                       |                           |
 *                     event                  anonymous pipe
 */
/*
 * TODO list
 * =========
 *
 * Write helper progrmas
 * ---------------------
 *
 *   I have following plans for helper programs.
 *
 *   mlcygpty.exe:
 *   A program to allow mlterm to use pty devices on Cygwin.
 *
 *   mlserial.exe:
 *   Allow to use mlterm as serial console.
 *
 *
 * Hide win32-specific handles
 * ---------------------------
 *
 *  I think win32-specific handles such as process and thread should be hidden
 *  in ml_pty_t, but I still have no idea how I can do it.
 *
 *
 * Signal handling
 * ---------------
 *
 *  To send signals like Ctrl-C to the child process, CREATE_NEW_PROCESS_GROUP
 *  flag must be specified in CreateProcess().
 *
 *
 * Implement ml_set_pty_winsize()
 * ------------------------------
 *
 *  I will use SetConsoleWindowInfo(), but I don't know if it can be used
 *  against the child process. 
 *
 *
 * Set pty slave name
 * ------------------
 *
 *  Command name and arguments?
 *  e.g. "c:\path\to\plink.exe -ssh example.com [2]"
 */

/* --- static functions --- */

static int
create_pty_proc(
	char *  cmd_path ,
	HANDLE *  pipe_write ,
	HANDLE *  pipe_read ,
	HANDLE *  pipe_err ,
	PROCESS_INFORMATION  *pi
	)
{
	HANDLE  pearent_write ;
	HANDLE  pearent_read ;
	HANDLE  pearent_readerr ;
	HANDLE  child_stdout ;
	HANDLE  child_stdin ;
	HANDLE  child_stderr ;
	HANDLE  hold_stdin ;
	HANDLE  hold_stdout ;
	HANDLE  hold_stderr ;
	HANDLE  me ;
	HANDLE  child ;
	STARTUPINFO  si ;
	SECURITY_ATTRIBUTES  sa ;

	me = GetCurrentProcess() ;
	hold_stdout = GetStdHandle(STD_OUTPUT_HANDLE) ;
	hold_stdin = GetStdHandle(STD_INPUT_HANDLE) ;
	hold_stderr = GetStdHandle(STD_ERROR_HANDLE) ;

	sa.nLength = sizeof( SECURITY_ATTRIBUTES) ;
	sa.lpSecurityDescriptor = NULL ;
	sa.bInheritHandle = TRUE ;

	/* pearent(pipe_write) --> child(stdin) */
	if( ! CreatePipe( &child_stdin , &pearent_write , &sa , 0))
	{
	#ifdef  DEBUG 
		kik_warn_printf( KIK_DEBUG_TAG "CreatePipe() failed. "
				 "[error: 0x%x]\n", GetLastError()) ;
	#endif
		exit(1) ;
	}
	if( ! DuplicateHandle( me , pearent_write , me , pipe_write , 0 ,
			       FALSE , DUPLICATE_SAME_ACCESS))
	{
	#ifdef  DEBUG 
		kik_warn_printf( KIK_DEBUG_TAG "DuplicateHandle() failed. "
				 "[error: 0x%x]\n", GetLastError()) ;
	#endif
		exit(1) ;
	}
	CloseHandle( pearent_write) ;

	/* parent(pipe_read) <-- child(stdout) */
	if( ! CreatePipe( &pearent_read , &child_stdout , &sa , 0))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG "CreatePipe() failed. "
				 "[error: 0x%x]\n", GetLastError()) ;
	#endif
		exit(1) ;
	}
	if( ! DuplicateHandle( me , pearent_read , me , pipe_read , 0 ,
			       FALSE , DUPLICATE_SAME_ACCESS))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG "DuplicateHandle() failed. "
				 "[error: 0x%x]\n", GetLastError()) ;
	#endif
		exit(1) ;
	}
	CloseHandle( pearent_read) ;

	/* parent(pipe_err) <-- child(stderr) */
	if( ! CreatePipe( &pearent_readerr , &child_stderr , &sa , 0))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG "CreatePipe() failed. "
				 "[error: 0x%x]\n", GetLastError()) ;
	#endif
		exit(1) ;
	}
	if( ! DuplicateHandle( me , pearent_readerr , me , pipe_err , 0 ,
			       FALSE , DUPLICATE_SAME_ACCESS))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG "DuplicateHandle() failed. "
				 "[error: 0x%x]\n", GetLastError()) ;
	#endif
		exit(1) ;
	}
	CloseHandle( pearent_readerr) ;

	ZeroMemory( pi , sizeof( PROCESS_INFORMATION)) ;
	ZeroMemory( &si , sizeof( STARTUPINFO)) ;
	si.cb = sizeof( STARTUPINFO) ;

	SetStdHandle( STD_INPUT_HANDLE , child_stdin) ;
	SetStdHandle( STD_OUTPUT_HANDLE , child_stdout) ;
	SetStdHandle( STD_ERROR_HANDLE , child_stderr) ;

	if( ! CreateProcess( NULL , cmd_path , NULL , NULL , TRUE ,
			     NORMAL_PRIORITY_CLASS , NULL , NULL , &si , pi))
	{
		SetStdHandle( STD_INPUT_HANDLE , hold_stdin) ;
		SetStdHandle( STD_OUTPUT_HANDLE , hold_stdout) ;
		SetStdHandle( STD_ERROR_HANDLE , child_stderr) ;
		kik_msg_printf( "Unable to create process(%s). [error: 0x%x]\n",
				cmd_path, GetLastError()) ;
		CloseHandle( child_stdin) ;
		CloseHandle( child_stdout) ;
		CloseHandle( child_stderr) ;
		CloseHandle( *pipe_write) ;
		CloseHandle( *pipe_read) ;
		CloseHandle( *pipe_err) ;
		return  0 ;
	}

	SetStdHandle( STD_INPUT_HANDLE , hold_stdin) ;
	SetStdHandle( STD_OUTPUT_HANDLE , hold_stdout) ;
	SetStdHandle( STD_ERROR_HANDLE , hold_stderr) ;
	CloseHandle( child_stdin) ;
	CloseHandle( child_stdout) ;
	CloseHandle( child_stderr) ;

	return  1 ;
}

static DWORD WINAPI
read_thread(
	void *  thread_parm
	)
{
	io_thread_parm_t *  parm ;
	DWORD  read ;
	DWORD  ret ;

	parm = (io_thread_parm_t*) thread_parm ;

	parm->io_failed = 0 ;
	parm->lasterr = 0 ;
	parm->is_exit_request = 0 ;

	while( 1)
	{
		if( ! ReadFile( parm->pipe , &parm->buf[parm->left] ,
				IO_BUFSIZE - parm->left , &read , NULL))
		{
			parm->io_failed = 1 ;
			parm->lasterr = GetLastError() ;
			parm->left = 0 ; 

			/* exit this thread */
			break ;
		}
		
		parm->left += read ; 

		SetEvent( parm->ev_ready) ;

		/* waiting for the request from main thread */
		ret = WaitForSingleObject( parm->ev_start , INFINITE) ;
		if( ( ret == WAIT_OBJECT_0 && parm->is_exit_request) ||
		    ret == WAIT_ABANDONED_0)
		{
			/* exit this thread */
			break ;
		}
	}

	return  0 ;
}

static DWORD WINAPI
write_thread(
	void *  thread_parm
	)
{
	io_thread_parm_t *  parm ;
	DWORD  written ;
	DWORD  ret ;

	parm = (io_thread_parm_t*) thread_parm ;

	parm->io_failed = 0 ;
	parm->lasterr = 0 ;
	parm->is_exit_request = 0 ;

	while( 1)
	{
		/* waiting for the request from main thread */
		ret = WaitForSingleObject( parm->ev_start , INFINITE) ;
		if( ( ret == WAIT_OBJECT_0 && parm->is_exit_request) ||
		    ret == WAIT_ABANDONED_0)
		{
			/* exit */
			break ;
		}

		if( ! WriteFile( parm->pipe , parm->buf , parm->left ,
				 &written , NULL))
		{
			parm->io_failed = 1 ;
			parm->lasterr = GetLastError() ;
			parm->left = 0 ;

			/* exit */
			break ;
		}

		parm->left -= written ;

		SetEvent( parm->ev_ready) ;
	}

	return  0 ;
}

/* --- global functions --- */

ml_pty_t *
ml_pty_new(
	char *  cmd_path ,
	char **  unused0 ,
	char **  unused1 ,
	char *  unused2 ,
	u_int  cols ,
	u_int  rows
	)
{
	ml_pty_win32_t *  win32_pty ;
	HANDLE  pipe_write ;
	HANDLE  pipe_read ;
	HANDLE  pipe_err ;
	DWORD  id;
	PROCESS_INFORMATION  pi ;

	if ( ! create_pty_proc( cmd_path ,  &pipe_write , &pipe_read ,
				&pipe_err , &pi))
	{
		return  NULL;
	}

	if( ( win32_pty = malloc( sizeof( ml_pty_win32_t))) == NULL)
	{
		return  NULL ;
	}

	win32_pty->child = pi.hProcess ;
	win32_pty->child_thread = pi.hThread ;
	win32_pty->ml_pty.child_pid = pi.dwProcessId ;
	win32_pty->ml_pty.buf = NULL ;
	win32_pty->ml_pty.left = 0 ;

	win32_pty->wparm.ev_start = CreateEvent( NULL , FALSE , FALSE , NULL) ;
	win32_pty->wparm.ev_ready = CreateEvent( NULL , FALSE , FALSE , NULL) ;
	win32_pty->wparm.pipe = pipe_write ;
	win32_pty->wthread = CreateThread( NULL , 0 , write_thread ,
					   &win32_pty->wparm , 0 , &id) ;
	if( win32_pty->wthread == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG "CreateThread() failed. "
				 "[error: 0x%x]\n", GetLastError()) ;
	#endif
		exit(1) ;
	}

	win32_pty->rparm.ev_start = CreateEvent( NULL , FALSE , FALSE , NULL) ;
	win32_pty->rparm.ev_ready = CreateEvent( NULL , FALSE , FALSE , NULL) ;
	win32_pty->rparm.pipe = pipe_read ;
	win32_pty->rthread = CreateThread( NULL , 0 , read_thread ,
					   &win32_pty->rparm , 0 , &id) ;
	if( win32_pty->rthread == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG "CreatePipe() failed. "
				 "[error: 0x%x]\n", GetLastError()) ;
	#endif
		exit(1) ;
	}

	win32_pty->eparm.ev_start = CreateEvent( NULL , FALSE , FALSE , NULL) ;
	win32_pty->eparm.ev_ready = CreateEvent( NULL , FALSE , FALSE , NULL) ;
	win32_pty->eparm.pipe = pipe_err ;
	win32_pty->ethread = CreateThread( NULL , 0 , read_thread ,
					   &win32_pty->eparm , 0 , &id) ;
	if( win32_pty->ethread == NULL)
	{
	#ifdef  DEBUG 
		kik_warn_printf( KIK_DEBUG_TAG "CreatePipe() failed. " 
				 "[error: 0x%x]\n", GetLastError()) ;
	#endif
		exit(1) ;
	}

	if( ml_set_pty_winsize( (ml_pty_t*) win32_pty , cols , rows) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_set_pty_winsize() failed.\n") ;
	#endif
	}
	return  (ml_pty_t*) win32_pty ;
}

int
ml_pty_delete(
	ml_pty_t *  pty
	)
{
	ml_pty_win32_t *  win32_pty ;

	win32_pty = (ml_pty_win32_t*) pty ;

	CloseHandle( win32_pty->wparm.pipe) ;
	CloseHandle( win32_pty->rparm.pipe) ;
	CloseHandle( win32_pty->eparm.pipe) ;

	CloseHandle( win32_pty->child) ;
	CloseHandle( win32_pty->child_thread) ;

	win32_pty->wparm.is_exit_request = 1 ;
	SetEvent( win32_pty->wparm.ev_start) ;
	WaitForSingleObject( win32_pty->wthread , INFINITE) ;

	win32_pty->rparm.is_exit_request = 1 ;
	SetEvent( win32_pty->rparm.ev_start) ;
	WaitForSingleObject( win32_pty->rthread , INFINITE) ;

	win32_pty->eparm.is_exit_request = 1 ;
	SetEvent( win32_pty->eparm.ev_start) ;
	WaitForSingleObject( win32_pty->ethread , INFINITE) ;

	CloseHandle( win32_pty->wthread) ;
	CloseHandle( win32_pty->rthread) ;
	CloseHandle( win32_pty->ethread) ;
	CloseHandle( win32_pty->wparm.ev_start) ;
	CloseHandle( win32_pty->rparm.ev_start) ;
	CloseHandle( win32_pty->eparm.ev_start) ;
	CloseHandle( win32_pty->wparm.ev_ready) ;
	CloseHandle( win32_pty->rparm.ev_ready) ;
	CloseHandle( win32_pty->eparm.ev_ready) ;

#if 0 /* XXX */
	free( pty->slave_name) ;
#endif
	free( win32_pty) ;

	return  0 ;
}

int
ml_set_pty_winsize(
	ml_pty_t *  pty ,
	u_int  cols ,
	u_int  rows
	)
{
	/* not written yet */
	return  0 ;
}

size_t
ml_write_to_pty(
	ml_pty_t *  pty ,
	u_char *  buf ,
	size_t  len
	)
{
	ml_pty_win32_t *  win32_pty ;
	DWORD  ret ;
	u_char *  w_buf ;
	size_t  w_buf_size ;
	ssize_t  written_size ;
	void *  p ;

	win32_pty = (ml_pty_win32_t*) pty ;

	if( ( w_buf_size = pty->left + len) == 0)
	{
		return  0 ;
	}

	if( ( w_buf = alloca( w_buf_size)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed and , "
				 "bytes to be written to pty are lost.\n") ;
	#endif

		return  pty->left ;
	}

	if( pty->buf && pty->left > 0)
	{
		memcpy( w_buf , pty->buf , pty->left) ;
	}

	if( len > 0)
	{
		memcpy( &w_buf[pty->left] , buf , len) ;
	}

	pty->left = w_buf_size ;

	ret = WaitForSingleObject( win32_pty->wparm.ev_ready , 0) ;
	if( ret == WAIT_OBJECT_0)
	{
		if( w_buf_size <= IO_BUFSIZE)
		{
			memcpy( win32_pty->wparm.buf , w_buf , w_buf_size) ;
			win32_pty->wparm.left = w_buf_size ;
			SetEvent( win32_pty->wparm.ev_start) ;
			if( pty->buf)
			{
				/* reset */
				free( pty->buf) ;
				pty->buf = NULL ;
				pty->left = 0 ;
			}

			return  0 ;
		}
		else /* w_buf_size > IO_BUFSIZE*/
		{
			memcpy( win32_pty->wparm.buf , w_buf , IO_BUFSIZE) ;
			pty->left = w_buf_size - IO_BUFSIZE ;
			SetEvent( win32_pty->wparm.ev_start) ;
		}
	}
#if 0
	else if( ret == WAIT_TIMEOUT)
	{
		/* io thread is busy. do nothing. */
	}
#endif

	if( ( p = realloc( pty->buf , pty->left)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " realloc failed.\n") ;
	#endif
	
		return  0 ;
	}

	pty->buf = p ;
	
	memcpy( pty->buf , &w_buf[written_size] , pty->left) ;

	return  pty->left ;
}

int
ml_flush_pty(
	ml_pty_t *  pty
	)
{
	return  ml_write_to_pty( pty , NULL , 0) ;
}

size_t
ml_read_pty(
	ml_pty_t *  pty ,
	u_char *  bytes ,
	size_t  left
	)
{
	ml_pty_win32_t *  win32_pty ;
	DWORD  ret ;
	size_t  read_size ;

	win32_pty = (ml_pty_win32_t*) pty ;

	ret = WaitForSingleObject( win32_pty->rparm.ev_ready , 0) ;
	if( ret == WAIT_OBJECT_0)
	{
		if( win32_pty->rparm.left <= left)
		{
			left = win32_pty->rparm.left ;
			memcpy( bytes , win32_pty->rparm.buf , left) ;
			win32_pty->rparm.left = 0 ;
			SetEvent( win32_pty->rparm.ev_start) ;

			return  left ;
		}
		else
		{
			memcpy( bytes , win32_pty->rparm.buf , left) ;
			win32_pty->rparm.left -= left ;
			memmove( bytes , &bytes[win32_pty->rparm.left] ,
				 win32_pty->rparm.left) ;
			SetEvent( win32_pty->rparm.ev_start) ;

			return  left ;
		}
	}
#if 0
	else( ret == WAIT_TIMEOUT)
	{
		/* io thread is busy. do nothing. */
	}
#endif

	return  0 ;
}

