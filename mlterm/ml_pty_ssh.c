/*
 *	$Id$
 */

#include  "ml_pty_intern.h"

#include  <libssh2.h>
#include  <kiklib/kik_config.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_str.h>
#include  <kiklib/kik_sig_child.h>
#include  <kiklib/kik_path.h>

#ifdef  USE_WIN32API
#undef  _WIN32_WINNT
#define  _WIN32_WINNT  0x0501	/* for getaddrinfo */
#include  <windows.h>
#include  <ws2tcpip.h>		/* addrinfo */
#else
#include  <sys/socket.h>
#include  <netdb.h>
#include  <netinet/in.h>
#endif
#include  <unistd.h>		/* close */
#include  <stdio.h>		/* sprintf */

#ifndef  USE_WIN32API
#define  closesocket( sock)  close( sock)
#endif

#if  0
#define  __DEBUG
#endif


typedef struct ssh_session
{
	char *  host ;
	char *  port ;
	char *  user ;
	LIBSSH2_SESSION *  obj ;
	int  sock ;

	u_int  ref_count ;

} ssh_session_t ;

typedef struct ml_pty_ssh
{
	ml_pty_t  pty ;

	ssh_session_t *  session ;
	LIBSSH2_CHANNEL *  channel ;

} ml_pty_ssh_t ;


/* --- static variables --- */

static char *  pass_response ;
static ssh_session_t **  sessions ;
static u_int  num_of_sessions = 0 ;
#ifdef  USE_WIN32API
static HANDLE  rd_ev ;
#endif


/* --- static functions --- */

#ifdef  USE_WIN32API

static DWORD WINAPI
wait_pty_read(
	LPVOID thr_param
  	)
{
	ml_pty_t *  pty = (ml_pty_t*)thr_param ;
	u_int  count ;
	struct timeval  tval ;
	fd_set  read_fds ;
	int  maxfd ;

	tval.tv_usec = 500000 ;	/* 0.5 sec */
	tval.tv_sec = 0 ;

#ifdef  __DEBUG
  	kik_debug_printf( "Starting wait_pty_read thread.\n") ;
#endif

	while( num_of_sessions > 0)
	{
		FD_ZERO( &read_fds) ;
		maxfd = 0 ;

		for( count = 0 ; count < num_of_sessions ; count++)
		{
			FD_SET( sessions[count]->sock , &read_fds) ;
			if( sessions[count]->sock > maxfd)
			{
				maxfd = sessions[count]->sock ;
			}
		}

		if( select( maxfd + 1 , &read_fds , NULL , NULL , &tval) > 0)
		{
			if( pty->pty_listener && pty->pty_listener->read_ready)
			{
				(*pty->pty_listener->read_ready)( pty->pty_listener->self) ;
			}

			WaitForSingleObject( rd_ev, INFINITE) ;
		}

	#ifdef  __DEBUG
		kik_debug_printf( "Select socket...\n") ;
	#endif
	}

#ifdef  __DEBUG
  	kik_debug_printf( "Exiting wait_pty_read thread.\n") ;
#endif

	CloseHandle( rd_ev) ;
	rd_ev = 0 ;

	ExitThread(0) ;

	return  0 ;
}

#endif


static void
kbd_callback(
	const char *  name ,
	int name_len ,
	const char *  instruction ,
	int instruction_len ,
	int num_prompts ,
	const LIBSSH2_USERAUTH_KBDINT_PROMPT *  prompts ,
	LIBSSH2_USERAUTH_KBDINT_RESPONSE *  responses ,
	void **  abstract
	)
{
	(void)name ;
	(void)name_len ;
	(void)instruction ;
	(void)instruction_len ;
	if( num_prompts == 1)
	{
		responses[0].text = strdup(pass_response) ;
		responses[0].length = strlen(pass_response) ;
	}
	(void)prompts ;
	(void)abstract ;
}

/*
 * Return session which is blocking mode.
 */
static ssh_session_t *
ssh_connect(
	const char *  host ,
	const char *  port ,
	const char *  user ,
	const char *  pass ,
	const char *  pubkey ,
	const char *  privkey
	)
{
	ssh_session_t *  session ;
	struct addrinfo  hints ;
	struct addrinfo *  addr ;
	struct addrinfo *  addr_p ;
	const char *  fingerprint ;
	char *  userauthlist ;
	int  auth_success = 0 ;
	int  count ;

	if( ( session = ml_search_ssh_session( host , port , user)))
	{
		session->ref_count ++ ;
		libssh2_session_set_blocking( session->obj , 1) ;

		return  session ;
	}

	if( ! ( session = malloc( sizeof(ssh_session_t))))
	{
		return  NULL ;
	}

	if( num_of_sessions == 0 && libssh2_init( 0) != 0)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " libssh2_init failed.\n") ;
	#endif

		goto  error1 ;
	}

	memset( &hints , 0 , sizeof(hints)) ;
	hints.ai_family = PF_UNSPEC ;
	hints.ai_socktype = SOCK_STREAM ;
	hints.ai_protocol = IPPROTO_TCP ;
	if( getaddrinfo( host , port , &hints , &addr) != 0)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " getaddrinfo failed.\n") ;
	#endif

		goto  error2 ;
	}

	addr_p = addr ;
	while( 1)
	{
		if( ( session->sock = socket( addr_p->ai_family ,
						addr_p->ai_socktype , addr_p->ai_protocol)) >= 0)
		{
			if( connect( session->sock , addr_p->ai_addr , addr_p->ai_addrlen) == 0)
			{
				break ;
			}
			else
			{
				closesocket( session->sock) ;
			}
		}

		if( ( addr_p = addr_p->ai_next) == NULL)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " socket/connect failed.\n");
		#endif

			freeaddrinfo( addr) ;

			goto  error2 ;
		}
	}

	freeaddrinfo( addr) ;

	if( ! ( session->obj = libssh2_session_init()))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " libssh2_session_init failed.\n") ;
	#endif

		goto  error3 ;
	}

#ifdef  DEBUG
	libssh2_trace( session->obj , LIBSSH2_TRACE_AUTH) ;
#endif

	libssh2_session_set_blocking( session->obj , 1) ;

	if( libssh2_session_startup( session->obj , session->sock) != 0)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " libssh2_session_startup failed.\n") ;
	#endif

		goto  error4 ;
	}

	fingerprint = libssh2_hostkey_hash( session->obj , LIBSSH2_HOSTKEY_HASH_SHA1) ;

	kik_msg_printf( "Fingerprint: ") ;
	for( count = 0 ; count < 20 ; count++)
	{
		kik_msg_printf( "%02X ", (u_char)fingerprint[count]) ;
	}
	kik_msg_printf("\n") ;

	if( ! ( userauthlist = libssh2_userauth_list( session->obj , user , strlen(user))))
	{
		goto  error4 ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Authentication methods: %s\n" , userauthlist) ;
#endif

	if( strstr( userauthlist , "publickey"))
	{
		char *  home ;
		char *  p ;

		if( ( home = kik_get_home_dir()) &&
		    ( ( p = alloca( strlen(home) * 2 + 38))) )
		{
			if( ! pubkey)
			{
			#ifdef  USE_WIN32API
				sprintf( p , "%s\\mlterm\\id_rsa.pub" , home) ;
			#else
				sprintf( p , "%s/.ssh/id_rsa.pub" , home) ;
			#endif

				pubkey = p ;
				p += (strlen( pubkey) + 1) ;
			}

			if( ! privkey)
			{
			#ifdef  USE_WIN32API
				sprintf( p , "%s\\mlterm\\id_rsa" , home) ;
			#else
				sprintf( p , "%s/.ssh/id_rsa" , home) ;
			#endif

				privkey = p ;
			}
		}
		else
		{
			if( ! pubkey)
			{
			#ifdef  USE_WIN32API
				pubkey = "mlterm\\ssh_host_rsa_key.pub" ;
			#else
				pubkey = "/etc/ssh/ssh_host_rsa_key.pub" ;
			#endif
			}

			if( ! privkey)
			{
			#ifdef  USE_WIN32API
				privkey = "mlterm\\ssh_host_rsa_key" ;
			#else
				privkey = "/etc/ssh/ssh_host_rsa_key" ;
			#endif
			}
		}

		if( libssh2_userauth_publickey_fromfile( session->obj , user ,
				pubkey , privkey , pass) == 0)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				" Authentication by public key succeeded.\n") ;
		#endif

			auth_success = 1 ;
		}
	#ifdef  DEBUG
		else
		{
			kik_debug_printf( KIK_DEBUG_TAG
				" Authentication by public key failed.\n") ;
		}
	#endif
	}

	if( ! auth_success && strstr( userauthlist , "keyboard-interactive"))
	{
		free( pass_response) ;
		pass_response = strdup( pass) ;

		if( libssh2_userauth_keyboard_interactive(
			session->obj , user , &kbd_callback) == 0)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				" Authentication by keyboard-interactive succeeded.\n") ;
		#endif

			auth_success = 1 ;
		}
	#ifdef  DEBUG
		else
		{
			kik_debug_printf( KIK_DEBUG_TAG
				" Authentication by keyboard-interactive failed.\n") ;
		}
	#endif
	}

	if( ! auth_success)
	{
		if( ! strstr( userauthlist , "password"))
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " No supported authentication methods found.\n") ;
		#endif

			goto  error4 ;
		}

		if( libssh2_userauth_password( session->obj , user , pass) != 0)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				" Authentication by password failed.\n") ;
		#endif
			goto  error4 ;
		}

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Authentication by password succeeded.\n") ;
	#endif
	}

	if( ! ( sessions = realloc( sessions , sizeof(ssh_session_t) * (++num_of_sessions))))
	{
		goto  error4 ;
	}

	session->host = strdup( host) ;
	session->port = strdup( port) ;
	session->user = strdup( user) ;
	session->ref_count = 1 ;
	sessions[num_of_sessions - 1] = session ;

	return  session ;

error4:
	libssh2_session_disconnect( session->obj , "Normal shutdown, Thank you for playing") ;
	libssh2_session_free( session->obj) ;

error3:
	closesocket( session->sock) ;

error2:
	if( num_of_sessions == 0)
	{
		libssh2_exit() ;
	}

error1:
	free( session) ;

	return  NULL ;
}

static int
ssh_disconnect(
	ssh_session_t *  session
	)
{
	u_int  count ;

	if( -- session->ref_count > 0)
	{
		/* In case this function is called from ml_pty_new. */
		libssh2_session_set_blocking( session->obj , 0) ;

		return  1 ;
	}

	for( count = 0 ; count < num_of_sessions ; count++)
	{
		if( sessions[count] == session)
		{
			sessions[count] = sessions[--num_of_sessions] ;

			if( num_of_sessions == 0)
			{
				free( sessions) ;
				sessions = NULL ;
			}

			break ;
		}
	}

	libssh2_session_disconnect( session->obj , "Normal shutdown, Thank you for playing") ;
	libssh2_session_free( session->obj) ;
	closesocket( session->sock) ;

	if( num_of_sessions == 0)
	{
		libssh2_exit() ;
	}

	free( session->host) ;
	free( session->port) ;
	free( session->user) ;
	free( session) ;

	return  1 ;
}

static int
delete(
	ml_pty_t *  pty
	)
{
	libssh2_session_set_blocking( ((ml_pty_ssh_t*)pty)->session->obj , 1) ;
	libssh2_channel_free( ((ml_pty_ssh_t*)pty)->channel) ;
	ssh_disconnect( ((ml_pty_ssh_t*)pty)->session) ;
	libssh2_exit() ;
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
	libssh2_channel_request_pty_size( ((ml_pty_ssh_t*)pty)->channel , cols , rows) ;

	return  1 ;
}

static ssize_t
write_to_pty(
	ml_pty_t *  pty ,
	u_char *  buf ,
	size_t  len
	)
{
	ssize_t  ret ;

	ret = libssh2_channel_write( ((ml_pty_ssh_t*)pty)->channel , buf , len) ;

	if( libssh2_channel_eof( ((ml_pty_ssh_t*)pty)->channel))
	{
		kik_trigger_sig_child( pty->child_pid) ;

		return  -1 ;
	}
	else
	{
		return  ret < 0 ? 0 : ret ;
	}
}

static ssize_t
read_pty(
	ml_pty_t *  pty ,
	u_char *  buf ,
	size_t  len
	)
{
	ssize_t  ret ;

	ret = libssh2_channel_read( ((ml_pty_ssh_t*)pty)->channel , buf , len) ;

#ifdef  USE_WIN32API
	SetEvent( rd_ev) ;
#endif

	if( libssh2_channel_eof( ((ml_pty_ssh_t*)pty)->channel))
	{
		kik_trigger_sig_child( pty->child_pid) ;

		return  -1 ;
	}
	else
	{
		return  ret < 0 ? 0 : ret ;
	}
}


/* --- global functions --- */

ml_pty_ptr_t
ml_pty_ssh_new(
	const char *  cmd_path ,	/* can be NULL */
	char **  cmd_argv ,	/* can be NULL(only if cmd_path is NULL) */
	char **  env ,		/* can be NULL */
	const char *  uri ,
	const char *  pass ,
	const char *  pubkey ,	/* can be NULL */
	const char *  privkey ,	/* can be NULL */
	u_int  cols ,
	u_int  rows
	)
{
	ml_pty_ssh_t *  pty ;
	char *  user ;
	char *  proto ;
	char *  host ;
	char *  port ;
	char *  term ;

	if( ( pty = malloc( sizeof( ml_pty_ssh_t))) == NULL)
	{
		return  NULL ;
	}

	if( ! kik_parse_uri( &proto , &user , &host , &port , NULL , NULL ,
			kik_str_alloca_dup( uri)))
	{
		goto  error1 ;
	}

	/* USER: unix , USERNAME: win32 */
	if( ! user && ! (user = getenv( "USER")) && ! (user = getenv( "USERNAME")))
	{
		goto  error1 ;
	}

	if( proto && strcmp( proto , "ssh") != 0)
	{
		goto  error1 ;
	}

	if( ( pty->session = ssh_connect( host , port ? port : "22" , user , pass ,
					pubkey , privkey)) == NULL)
	{
		goto  error1 ;
	}

	/* Request a shell */
	if( ! ( pty->channel = libssh2_channel_open_session( pty->session->obj)))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Unable to open a session\n") ;
	#endif

		goto  error2 ;
	}

	term = NULL ;
	if( env)
	{
		while( *env)
		{
			char *  val ;

			if( ( val = strchr( *env , '=')))
			{
				*(val ++) = '\0' ;
			}
			else
			{
				val = "" ;
			}

			libssh2_channel_setenv( pty->channel , *env , val) ;

			if( strcmp( *env , "TERM") == 0)
			{
				term = val ;
			}

			env ++ ;
		}
	}

	if( libssh2_channel_request_pty( pty->channel , term ? term : "xterm"))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Failed requesting pty\n") ;
	#endif

		goto  error3 ;
	}

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
			goto  error3 ;
		}

		strcpy( cmd_line, cmd_path) ;
		for( count = 1 ; cmd_argv[count] != NULL ; count ++)
		{
			strcat( cmd_line, " ") ;
			strcat( cmd_line, cmd_argv[count]) ;
		}

		if( libssh2_channel_exec( pty->channel , cmd_line))
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				" Unable to exec %s on allocated pty\n" , cmd_line) ;
		#endif

			goto  error3 ;
		}
	}
	else
	{
		/* Open a SHELL on that pty */
		if( libssh2_channel_shell( pty->channel))
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				" Unable to request shell on allocated pty\n") ;
		#endif

			goto  error3 ;
		}
	}

	pty->pty.master = pty->session->sock ;
	pty->pty.slave = -1 ;
	pty->pty.child_pid = (pid_t) pty->channel ;	/* XXX regarding pid_t as channel */
	pty->pty.buf = NULL ;
	pty->pty.left = 0 ;
	pty->pty.size = 0 ;
	pty->pty.delete = delete ;
	pty->pty.set_winsize = set_winsize ;
	pty->pty.write = write_to_pty ;
	pty->pty.read = read_pty ;
	pty->pty.pty_listener = NULL ;

	if( ml_set_pty_winsize( &pty->pty , cols , rows) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_set_pty_winsize() failed.\n") ;
	#endif
	}
	
	/* Non-blocking in read/write. */
	libssh2_session_set_blocking( pty->session->obj , 0) ;

#ifdef  USE_WIN32API
	if( ! rd_ev)
	{
		HANDLE  thrd ;
		DWORD  tid ;

		rd_ev = CreateEvent( NULL , FALSE , FALSE , "PTY_READ_READY") ;

		/* Launch the thread that wait for receiving data from pty. */
		if( ! ( thrd = CreateThread( NULL , 0 , wait_pty_read , (LPVOID)pty , 0 , &tid)))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " CreateThread() failed.\n") ;
		#endif

			goto  error3 ;
		}

		CloseHandle( thrd) ;
	}
#endif

	return  &pty->pty ;

error3:
	libssh2_channel_free( pty->channel) ;

error2:
	ssh_disconnect( pty->session) ;

error1:
	free( pty) ;

	return  NULL ;
}

void *
ml_search_ssh_session(
	const char *  host ,
	const char *  port ,	/* can be NULL */
	const char *  user	/* can be NULL */
	)
{
	u_int  count ;

	for( count = 0 ; count < num_of_sessions ; count++)
	{
		if( strcmp( sessions[count]->host , host) == 0 &&
		    (port == NULL || strcmp( sessions[count]->port , port) == 0) &&
		    (user == NULL || strcmp( sessions[count]->user , user) == 0))
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Find cached session for %s %s %s.\n" ,
				host , port , user) ;
		#endif

			return  sessions[count] ;
		}
	}

	return  NULL ;
}
