/*
 *	$Id$
 */

#include  "../ml_pty_intern.h"

#include  <libssh2.h>
#include  <kiklib/kik_def.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_str.h>
#include  <kiklib/kik_sig_child.h>
#include  <kiklib/kik_path.h>
#include  <kiklib/kik_unistd.h>		/* kik_usleep */
#include  <kiklib/kik_dialog.h>
#include  <kiklib/kik_net.h>		/* getaddrinfo/socket/connect/sockaddr_un */

#if  ! defined(USE_WIN32API) && defined(HAVE_PTHREAD)
#include  <pthread.h>
#endif
#include  <fcntl.h>		/* open */
#include  <unistd.h>		/* close/pipe */
#include  <stdio.h>		/* sprintf */


#ifndef  USE_WIN32API
#define  closesocket( sock)  close( sock)
#endif

#ifndef  NO_DYNAMIC_LOAD_SSH
/*
 * If NO_DYNAMIC_LOAD_SSH is defined, mlterm/libptyssh/ml_pty_ssh.c is included
 * from mlterm/ml_pty_ssh.c.
 */
#define  ml_write_to_pty( pty , buf , len)  (*(pty)->write)( pty , buf , len)
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

	int  doing_scp ;

	u_int  ref_count ;

} ssh_session_t ;

typedef struct ml_pty_ssh
{
	ml_pty_t  pty ;

	ssh_session_t *  session ;
	LIBSSH2_CHANNEL *  channel ;

} ml_pty_ssh_t ;

typedef struct scp
{
	LIBSSH2_CHANNEL *  remote ;
	int  local ;
	int  src_is_remote ;
	size_t  src_size ;

	ml_pty_ssh_t *  pty_ssh ;

} scp_t ;

#ifndef  USE_WIN32API
typedef struct x11_fd
{
	int  display ;
	int  channel ;

} x11_fd_t ;

typedef struct x11_channel
{
	LIBSSH2_CHANNEL *  channel ;
	ssh_session_t *  session ;

} x11_channel_t ;

#endif


/* --- static variables --- */

static char *  pass_response ;

static ssh_session_t **  sessions ;
static u_int  num_of_sessions = 0 ;

#ifdef  USE_WIN32API
static HANDLE  rd_ev ;
#endif

static const char *  cipher_list ;

static u_int  keepalive_msec ;
static u_int  keepalive_msec_left ;

static int  use_x11_forwarding ;
#ifndef  USE_WIN32API
static int  display_port = -1 ;
static x11_fd_t  x11_fds[10] ;
static u_int  num_of_x11_fds ;
static x11_channel_t  x11_channels[10] ;
#endif


/* --- static functions --- */

#ifdef  USE_WIN32API

static u_int __stdcall
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

	/* Not necessary if thread started by _beginthreadex */
#if  0
	ExitThread(0) ;
#endif

	return  0 ;
}

#endif	/* USE_WIN32API */


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

	const char *  hostkey ;
	size_t  hostkey_len ;
	int  hostkey_type ;

	char *  userauthlist ;
	int  auth_success = 0 ;

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
	libssh2_trace( session->obj , -1) ;
#endif

	libssh2_session_set_blocking( session->obj , 1) ;

	libssh2_session_flag( session->obj , LIBSSH2_FLAG_COMPRESS , 1) ;

	if( cipher_list)
	{
		libssh2_session_method_pref( session->obj ,
			LIBSSH2_METHOD_CRYPT_CS , cipher_list) ;
		libssh2_session_method_pref( session->obj ,
			LIBSSH2_METHOD_CRYPT_SC , cipher_list) ;
	}

	if( libssh2_session_startup( session->obj , session->sock) != 0)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " libssh2_session_startup failed.\n") ;
	#endif

		goto  error4 ;
	}

	/*
	 * Check ~/.ssh/knownhosts.
	 */

	if( ( hostkey = libssh2_session_hostkey( session->obj , &hostkey_len , &hostkey_type)))
	{
		char *  home ;
		char *  path ;
		LIBSSH2_KNOWNHOSTS *  nhs ;

		if( ( home = kik_get_home_dir()) &&
		    ( path = alloca( strlen(home) + 20)) &&
		    ( nhs = libssh2_knownhost_init( session->obj)) )
		{
			struct libssh2_knownhost *  nh ;

		#ifdef  USE_WIN32API
			sprintf( path , "%s\\mlterm\\known_hosts" , home) ;
		#else
			sprintf( path , "%s/.ssh/known_hosts" , home) ;
		#endif

			libssh2_knownhost_readfile( nhs , path ,
				LIBSSH2_KNOWNHOST_FILE_OPENSSH) ;

			if( libssh2_knownhost_checkp( nhs , host , atoi( port) ,
					hostkey , hostkey_len ,
					LIBSSH2_KNOWNHOST_TYPE_PLAIN|
					LIBSSH2_KNOWNHOST_KEYENC_RAW ,
					&nh) != LIBSSH2_KNOWNHOST_CHECK_MATCH)
			{
				const char *  hash ;
				size_t  count ;
				char *  msg ;
				char *  p ;

				hash = libssh2_hostkey_hash( session->obj ,
						LIBSSH2_HOSTKEY_HASH_SHA1) ;

				msg = alloca( strlen( host) + 31 + 3 * 20 + 1) ;

				sprintf( msg , "Connecting to unknown host: %s (" , host) ;
				p = msg + strlen( msg) ;
				for( count = 0 ; count < 20 ; count++)
				{
					sprintf( p + count * 3 , "%02x:" , (u_char)hash[count]) ;
				}
				msg[strlen(msg) - 1] = ')' ;	/* replace ':' with ')' */

				if( ! kik_dialog( KIK_DIALOG_OKCANCEL , msg))
				{
					libssh2_knownhost_free( nhs) ;

					goto  error4 ;
				}

				libssh2_knownhost_add( nhs , host , NULL ,
						hostkey , hostkey_len ,
						LIBSSH2_KNOWNHOST_TYPE_PLAIN|
						LIBSSH2_KNOWNHOST_KEYENC_RAW|
						LIBSSH2_KNOWNHOST_KEY_SSHRSA , NULL) ;

				libssh2_knownhost_writefile( nhs , path ,
					LIBSSH2_KNOWNHOST_FILE_OPENSSH) ;

				kik_msg_printf( "Add to %s and continue connecting.\n" , path) ;
			}

			libssh2_knownhost_free( nhs) ;
		}
	}

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
			kik_msg_printf( "Authentication by public key succeeded.\n") ;

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
			kik_msg_printf( "Authentication by keyboard-interactive succeeded.\n") ;

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
			kik_debug_printf( KIK_DEBUG_TAG
				" No supported authentication methods found.\n") ;
		#endif

			goto  error4 ;
		}

		if( libssh2_userauth_password( session->obj , user , pass) != 0)
		{
			kik_msg_printf( "Authentication by password failed.\n") ;

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

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Closed session.\n") ;
#endif

	return  1 ;
}

static int
final(
	ml_pty_t *  pty
	)
{
	libssh2_session_set_blocking( ((ml_pty_ssh_t*)pty)->session->obj , 1) ;
	libssh2_channel_free( ((ml_pty_ssh_t*)pty)->channel) ;
	ssh_disconnect( ((ml_pty_ssh_t*)pty)->session) ;

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

	if( ret == LIBSSH2_ERROR_SOCKET_SEND ||
	    libssh2_channel_eof( ((ml_pty_ssh_t*)pty)->channel))
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

	if( ret == LIBSSH2_ERROR_SOCKET_SEND ||
	    libssh2_channel_eof( ((ml_pty_ssh_t*)pty)->channel))
	{
		kik_trigger_sig_child( pty->child_pid) ;

		return  -1 ;
	}
	else
	{
		return  ret < 0 ? 0 : ret ;
	}
}

static int
scp_stop(
	ml_pty_ssh_t *  pty_ssh
	)
{
	pty_ssh->session->doing_scp = 0 ;

	return  1 ;
}

#ifdef  USE_WIN32API
static ssize_t
lo_recv_pty(
	ml_pty_t *  pty ,
	u_char *  buf ,
	size_t  len
	)
{
	return  recv( pty->master , buf , len , 0) ;
}

static ssize_t
lo_send_to_pty(
	ml_pty_t *  pty ,
	u_char *  buf ,
	size_t  len
	)
{
	if( len == 1 && buf[0] == '\x03')
	{
		/* ^C */
		scp_stop( pty) ;
	}

	return  send( pty->slave , buf , len , 0) ;
}

static int
_socketpair(
	int  af ,
	int  type ,
	int  proto ,
	SOCKET  sock[2]
	)
{
	SOCKET  listen_sock ;
	SOCKADDR_IN  addr ;
	int addr_len ;

	if( ( listen_sock = WSASocket( af , type , proto , NULL , 0 , 0)) == -1)
	{
		return  -1 ;
	}

	addr_len = sizeof(addr) ;

	memset( (void*)&addr , 0 , sizeof(addr)) ;
	addr.sin_family = af ;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK) ;
	addr.sin_port = 0 ;

	if( bind( listen_sock , (SOCKADDR*)&addr , addr_len) != 0)
	{
		goto  error1 ;
	}

	if( getsockname( listen_sock , (SOCKADDR*)&addr , &addr_len) != 0)
	{
		goto  error1 ;
	}

	if( listen( listen_sock , 1) != 0)
	{
		goto  error1 ;
	}

	if( ( sock[0] = WSASocket( af , type , proto , NULL , 0 , 0)) == -1)
	{
		goto  error1 ;
	}

	if( connect( sock[0] , (SOCKADDR*)&addr , addr_len) != 0)
	{
		goto  error2 ;
	}

	if( ( sock[1] = accept( listen_sock , 0 , 0)) == -1)
	{
		goto  error2 ;
	}

	closesocket( listen_sock) ;

	return  0 ;

error2:
	closesocket( sock[0]) ;

error1:
	closesocket( listen_sock) ;

	return  -1 ;
}
#endif	/* USE_WIN32API */

static ssize_t
lo_read_pty(
	ml_pty_t *  pty ,
	u_char *  buf ,
	size_t  len
	)
{
	return  read( pty->master , buf , len) ;
}

static ssize_t
lo_write_to_pty(
	ml_pty_t *  pty ,
	u_char *  buf ,
	size_t  len
	)
{
	if( len == 1 && buf[0] == '\x03')
	{
		/* ^C */
		scp_stop( ( ml_pty_ssh_t*)pty) ;
	}

	return  write( pty->slave , buf , len) ;
}

static int
use_loopback(
	ml_pty_t *  pty
	)
{
	int  fds[2] ;

	if( pty->stored)
	{
		pty->stored->ref_count ++ ;

		return  1 ;
	}

	if( ( pty->stored = malloc( sizeof( *(pty->stored)))) == NULL)
	{
		return  0 ;
	}

	pty->stored->master = pty->master ;
	pty->stored->slave = pty->slave ;
	pty->stored->read = pty->read ;
	pty->stored->write = pty->write ;

#ifdef  USE_WIN32API
	if( _socketpair( AF_INET , SOCK_STREAM , 0 , fds) == 0)
	{
		u_long  val ;

		val = 1 ;
		ioctlsocket( fds[0] , FIONBIO , &val) ;
		val = 1 ;
		ioctlsocket( fds[1] , FIONBIO , &val) ;

		pty->read = lo_recv_pty ;
		pty->write = lo_send_to_pty ;
	}
	else if( _pipe( fds , 256 , O_BINARY) == 0)
	{
		pty->read = lo_read_pty ;
		pty->write = lo_write_to_pty ;
	}
#else
	if( pipe( fds) == 0)
	{
		fcntl( fds[0] , F_SETFL , O_NONBLOCK|fcntl( pty->master , F_GETFL , 0)) ;
		fcntl( fds[1] , F_SETFL , O_NONBLOCK|fcntl( pty->slave , F_GETFL , 0)) ;

		pty->read = lo_read_pty ;
		pty->write = lo_write_to_pty ;
	}
#endif
	else
	{
		free( pty->stored) ;
		pty->stored = NULL ;

		return  0 ;
	}

	pty->master = fds[0] ;
	pty->slave = fds[1] ;

	pty->stored->ref_count = 1 ;

	return  1 ;
}

static int
unuse_loopback(
	ml_pty_t *  pty
	)
{
	if( ! pty->stored || --(pty->stored->ref_count) > 0)
	{
		return  1 ;
	}

#ifdef  USE_WIN32API
	if( pty->read == lo_recv_pty)
	{
		closesocket( pty->slave) ;
		closesocket( pty->master) ;
	}
	else
#endif
	{
		close( pty->slave) ;
		close( pty->master) ;
	}

	pty->master = pty->stored->master ;
	pty->slave = pty->stored->slave ;
	pty->read = pty->stored->read ;
	pty->write = pty->stored->write ;

	free( pty->stored) ;
	pty->stored = NULL ;

	return  1 ;
}

#ifdef  USE_WIN32API
static u_int __stdcall
#else
static void *
#endif
scp_thread(
	void *  p
	)
{
	scp_t *  scp ;
	size_t  rd_len ;
	char  buf[1024] ;
	int  progress ;
	char  msg1[] = "\x1b[?25l\r\nTransferring data.\r\n|" ;
	char  msg2[] = "**************************************************|\x1b[?25h\r\n" ;

#if  ! defined(USE_WIN32API) && defined(HAVE_PTHREAD)
	pthread_detach( pthread_self()) ;
#endif

	scp = p ;

	rd_len = 0 ;
	progress = 0 ;

	ml_write_to_pty( &scp->pty_ssh->pty , msg1 , sizeof(msg1) - 1) ;

	while( rd_len < scp->src_size && scp->pty_ssh->session->doing_scp)
	{
		int  new_progress ;
		ssize_t  len ;

		if( scp->src_is_remote)
		{
			if( ( len = libssh2_channel_read( scp->remote , buf , sizeof(buf))) < 0)
			{
				if( len == LIBSSH2_ERROR_EAGAIN)
				{
					kik_usleep(1) ;
					continue ;
				}
				else
				{
					break ;
				}
			}

			write( scp->local , buf , len) ;
		}
		else
		{
			if( ( len = read( scp->local , buf , sizeof(buf))) < 0)
			{
				break ;
			}

			while( libssh2_channel_write( scp->remote , buf , len) ==
				LIBSSH2_ERROR_EAGAIN)
			{
				kik_usleep(1) ;
			}
		}

		rd_len += len ;

		new_progress = 50 * rd_len / scp->src_size ;

		if( progress < new_progress && new_progress < 50)
		{
			int  count ;

			progress = new_progress ;

			for( count = 0 ; count < new_progress ; count++)
			{
				ml_write_to_pty( &scp->pty_ssh->pty , "*" , 1) ;
			}
			for( ; count < 50 ; count++)
			{
				ml_write_to_pty( &scp->pty_ssh->pty , " " , 1) ;
			}
			ml_write_to_pty( &scp->pty_ssh->pty , "|\r|" , 3) ;

		#ifdef  USE_WIN32API
			if( scp->pty_ssh->pty.pty_listener &&
			    scp->pty_ssh->pty.pty_listener->read_ready)
			{
				(*scp->pty_ssh->pty.pty_listener->read_ready)(
					scp->pty_ssh->pty.pty_listener->self) ;
			}
		#endif
		}
	}
	ml_write_to_pty( &scp->pty_ssh->pty , msg2 , sizeof(msg2) - 1) ;

#if  1
	kik_usleep( 100000) ;	/* Expect to switch to main thread and call ml_read_pty(). */
#else
	pthread_yield() ;
#endif

	while( libssh2_channel_free( scp->remote) == LIBSSH2_ERROR_EAGAIN) ;
	close( scp->local) ;

	unuse_loopback( &scp->pty_ssh->pty) ;

	scp->pty_ssh->session->doing_scp = 0 ;

	free( scp) ;

	/* Not necessary if thread started by _beginthreadex */
#if  0
	ExitThread(0) ;
#endif

	return  0 ;
}


#ifndef  USE_WIN32API

static int
setup_x11(
	LIBSSH2_CHANNEL *  channel
	)
{
	char *  display ;
	char *  display_port_str ;
	char *  p ;
	char *  cmd ;
	char  line[512] ;
	char *  proto ;
	char *  data ;
	FILE *  fp ;

	if( ! ( display = getenv( "DISPLAY")))
	{
		return  0 ;
	}

	if( strncmp( display , "unix:" , 5) == 0)
	{
		display_port_str = display + 5 ;
	}
	else if( display[0] == ':')
	{
		display_port_str = display + 1 ;
	}
	else
	{
		return  0 ;
	}

	if( ! ( display_port_str = kik_str_alloca_dup( display_port_str)))
	{
		return  0 ;
	}

	if( ( p = strrchr( display_port_str , '.')))
	{
		*p = '\0' ;
	}

	display_port = atoi( display_port_str) ;

	proto = NULL ;
	data = NULL ;

	if( ( cmd = alloca( 39 + strlen( display) + 1)))
	{
		sprintf( cmd , "xauth list %s 2> /dev/null" , display) ;
		if( ( fp = popen( cmd , "r")))
		{
			if( fgets( line , sizeof(line) , fp))
			{
				if( ( proto = strchr( line , ' ')))
				{
					proto += 2 ;

					if( ( data = strchr( proto , ' ')))
					{
						*data = '\0' ;
						data += 2 ;

						if( ( p = strchr( data , '\n')))
						{
							*p = '\0' ;
						}
					}
				}
			}

			fclose( fp) ;
		}
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " libssh2_channel_x11_req_ex (with xauth %s %s)\n" ,
			proto , data) ;
#endif

	return  libssh2_channel_x11_req_ex( channel , 0 , proto , data , 0) == 0 ;
}

static void
x11_callback(
	LIBSSH2_SESSION *  session ,
	LIBSSH2_CHANNEL *  channel ,
	char *  shost ,
	int  sport ,
	void **  abstract
	)
{
	u_int  count ;
	int  channel_sock ;
	struct sockaddr_un  addr ;
	int  display_sock ;

	if( display_port == -1 || num_of_x11_fds >= 10)
	{
		goto  error ;
	}

	for( count = 0 ; ; count++)
	{
		if( count == num_of_sessions)
		{
			goto  error ;
		}

		if( session == sessions[count]->obj)
		{
			channel_sock = sessions[count]->sock ;
			break ;
		}
	}

	if( ( display_sock = socket( AF_UNIX , SOCK_STREAM , 0)) < 0)
	{
		goto  error ;
	}

	memset( &addr , 0 , sizeof(addr)) ;
	addr.sun_family = AF_UNIX;
	snprintf( addr.sun_path , sizeof(addr.sun_path) ,
		"/tmp/.X11-unix/X%d" , display_port) ;

	if( connect( display_sock , (struct sockaddr *)&addr , sizeof(addr)) != -1)
	{
	#ifdef  USE_WIN32API
		u_long  val ;

		ioctlsocket( display_sock , FIONBIO , &val) ;
	#else
		fcntl( display_sock , F_SETFL , O_NONBLOCK|fcntl( display_sock , F_GETFL , 0)) ;
	#endif

		sessions[count]->ref_count ++ ;

		x11_channels[num_of_x11_fds].channel = channel ;
		x11_channels[num_of_x11_fds].session = sessions[count] ;
		x11_fds[num_of_x11_fds].display = display_sock ;
		x11_fds[num_of_x11_fds++].channel = channel_sock ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " x11 forwarding started.\n") ;
	#endif

		/* Successfully setup x11 forwarding. */

		return ;
	}
	else
	{
		closesocket( display_sock) ;
	}

error:
	libssh2_channel_free( channel) ;
}

static int  ssh_to_xserver( LIBSSH2_CHANNEL *  channel , int  display) ;

static int
xserver_to_ssh(
	LIBSSH2_CHANNEL *  channel ,
	int  display
	)
{
	char  buf[8192] ;
	ssize_t  len ;
	int  ret ;

	ret = -1 ;
	while( ( len = read( display , buf , sizeof(buf))) > 0)
	{
		ssize_t  w_len ;
		char *  p ;

		p = buf ;
		while( ( w_len = libssh2_channel_write( channel , p , len)) < len)
		{
			if( w_len > 0)
			{
				len -= w_len ;
				p += w_len ;
			}

			ssh_to_xserver( channel , display) ;
		}

		ret = 1 ;
	#if  0
		kik_debug_printf( "X SERVER -> CHANNEL %d\n" , len) ;
	#endif
	}

	if( len == 0)
	{
		return  0 ;
	}
	else
	{
		return  ret ;
	}
}

static int
ssh_to_xserver(
	LIBSSH2_CHANNEL *  channel ,
	int  display
	)
{
	char  buf[8192] ;
	ssize_t  len ;
	int  ret ;

	ret = -1 ;
	while( ( len = libssh2_channel_read( channel , buf , sizeof(buf))) > 0)
	{
		ssize_t  w_len ;
		char *  p ;

		p = buf ;
		while( ( w_len = write( display , p , len)) < len)
		{
			if( w_len > 0)
			{
				len -= w_len ;
				p += w_len ;
			}

			xserver_to_ssh( channel , display) ;
		}

		ret = 1 ;

	#if  0
		kik_debug_printf( "CHANNEL -> X SERVER %d\n" , len) ;
	#endif
	}

	if( libssh2_channel_eof( channel))
	{
		return  0 ;
	}
	else
	{
		return  ret ;
	}
}

#endif	/* USE_WIN32API */


/* --- global functions --- */

ml_pty_t *
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

	if( ( pty = calloc( 1 , sizeof( ml_pty_ssh_t))) == NULL)
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

#ifndef  USE_WIN32API
	if( use_x11_forwarding)
	{
		libssh2_session_callback_set( pty->session->obj ,
			LIBSSH2_CALLBACK_X11 , x11_callback) ;
	}
#endif

	/* Request a shell */
	if( ! ( pty->channel = libssh2_channel_open_session( pty->session->obj)))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Unable to open a session\n") ;
	#endif

		goto  error2 ;
	}

	pty->session->doing_scp = 0 ;

	term = NULL ;
	if( env)
	{
		while( *env)
		{
			char *  val ;
			size_t  key_len ;

			if( ( val = strchr( *env , '=')))
			{
				key_len = val - *env ;
				val ++ ;
			}
			else
			{
				key_len = strlen( *env) ;
				val = "" ;
			}

			libssh2_channel_setenv_ex( pty->channel , *env , key_len ,
				val , strlen( val)) ;

		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Env %s => key_len %d val %s\n" ,
				*env , key_len , val) ;
		#endif

			if( strncmp( *env , "TERM=" , 5) == 0)
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

#ifndef  USE_WIN32API
	if( use_x11_forwarding)
	{
		if( ! setup_x11( pty->channel))
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Failed requesting x11\n") ;
		#endif
		}
	}
#endif

	if( cmd_path)
	{
		int  count ;
		char *   cmd_line ;
		size_t   cmd_line_len ;

		/* Because cmd_path == cmd_argv[0], cmd_argv[0] is ignored. */

		/* 1 = NULL terminator */
		cmd_line_len = strlen(cmd_path) + 1 ;
		for( count = 1 ; cmd_argv[count] != NULL ; count++)
		{
			/* 3 = " " */
			cmd_line_len += (strlen(cmd_argv[count]) + 3) ;
		}

		if( ( cmd_line = alloca( sizeof(char) * cmd_line_len)) == NULL)
		{
			goto  error3 ;
		}

		strcpy( cmd_line, cmd_path) ;
		for( count = 1 ; cmd_argv[count] != NULL ; count ++)
		{
			sprintf( cmd_line + strlen(cmd_line) ,
				strchr( cmd_argv[count] , ' ') ? " \"%s\"" : " %s" ,
				cmd_argv[count]) ;
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
	pty->pty.final = final ;
	pty->pty.set_winsize = set_winsize ;
	pty->pty.write = write_to_pty ;
	pty->pty.read = read_pty ;

	if( set_winsize( &pty->pty , cols , rows) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_set_pty_winsize() failed.\n") ;
	#endif
	}

	/* Non-blocking in read/write. */
	libssh2_session_set_blocking( pty->session->obj , 0) ;

	if( keepalive_msec >= 1000)
	{
		libssh2_keepalive_config( pty->session->obj , 1 , keepalive_msec / 1000) ;
	}

#ifdef  USE_WIN32API
	if( ! rd_ev)
	{
		HANDLE  thrd ;
		u_int  tid ;

		rd_ev = CreateEvent( NULL , FALSE , FALSE , "PTY_READ_READY") ;
		if( GetLastError() != ERROR_ALREADY_EXISTS)
		{
			/* Launch the thread that wait for receiving data from pty. */
			if( ! ( thrd = _beginthreadex( NULL , 0 , wait_pty_read , pty , 0 , &tid)))
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " CreateThread() failed.\n") ;
			#endif

				goto  error3 ;
			}

			CloseHandle( thrd) ;
		}
		else
		{
			/* java/MLTerm.java has already watched pty. */
		}
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

int
ml_pty_set_use_loopback(
	ml_pty_t *  pty ,
	int  use
	)
{
	if( use)
	{
		return  use_loopback( pty) ;
	}
	else
	{
		return  unuse_loopback( pty) ;
	}
}

int
ml_pty_ssh_scp_intern(
	ml_pty_t *  pty ,
	int  src_is_remote ,
	char *  dst_path ,
	char *  src_path
	)
{
	scp_t *  scp ;
	struct stat  st ;

	/* Note that session is non-block mode in this context. */

	/* Check if pty is ml_pty_ssh_t or not. */
	if( pty->final != final)
	{
		return  0 ;
	}

	if( ((ml_pty_ssh_t*)pty)->session->doing_scp)
	{
		kik_msg_printf( "SCP: Another scp process is working.\n") ;

		return  0 ;
	}

	if( ! ( scp = malloc( sizeof(scp_t))))
	{
		return  0 ;
	}
	scp->pty_ssh = (ml_pty_ssh_t*)pty ;

	scp->pty_ssh->session->doing_scp = 1 ;

	if( src_is_remote)
	{
		while( ! ( scp->remote = libssh2_scp_recv( scp->pty_ssh->session->obj ,
						src_path , &st)) &&
		       libssh2_session_last_errno( scp->pty_ssh->session->obj)
						== LIBSSH2_ERROR_EAGAIN) ;
		if( ! scp->remote)
		{
			kik_msg_printf( "SCP: Failed to open remote:%s.\n" , src_path) ;

			goto  error ;
		}

		if( ( scp->local = open( dst_path , O_WRONLY|O_CREAT|O_TRUNC
				#ifdef  USE_WIN32API
					|O_BINARY
				#endif
					, st.st_mode)) < 0)
		{
			kik_msg_printf( "SCP: Failed to open local:%s.\n" , dst_path) ;

			while( libssh2_channel_free( scp->remote) == LIBSSH2_ERROR_EAGAIN) ;

			goto  error ;
		}
	}
	else
	{
		if( ( scp->local = open( src_path , O_RDONLY
				#ifdef  USE_WIN32API
					|O_BINARY
				#endif
					, 0644)) < 0)
		{
			kik_msg_printf( "SCP: Failed to open local:%s.\n" , src_path) ;

			goto  error ;
		}

		fstat( scp->local , &st) ;

		while( ! ( scp->remote = libssh2_scp_send( scp->pty_ssh->session->obj , dst_path ,
						st.st_mode & 0777 , (u_long)st.st_size)) &&
		       libssh2_session_last_errno( scp->pty_ssh->session->obj)
						== LIBSSH2_ERROR_EAGAIN) ;
		if( ! scp->remote)
		{
			kik_msg_printf( "SCP: Failed to open remote:%s.\n" , dst_path) ;

			close( scp->local) ;

			goto  error ;
		}
	}

	scp->src_is_remote = src_is_remote ;
	scp->src_size = st.st_size ;

	if( ! use_loopback( pty))
	{
		while( libssh2_channel_free( scp->remote) == LIBSSH2_ERROR_EAGAIN) ;

		goto  error ;
	}

#if  defined(USE_WIN32API)
	{
		HANDLE  thrd ;
		u_int  tid ;

		if( ( thrd = _beginthreadex( NULL , 0 , scp_thread , scp , 0 , &tid)))
		{
			CloseHandle( thrd) ;
		}
	}
#elif  defined(HAVE_PTHREAD)
	{
		pthread_t  thrd ;

		pthread_create( &thrd , NULL , scp_thread , scp) ;
	}
#else
	scp_thread( scp) ;
#endif

	return  1 ;

error:
	scp->pty_ssh->session->doing_scp = 0 ;
	free( scp) ;

	return  0 ;
}

void
ml_pty_ssh_set_cipher_list(
	const char *  list
	)
{
	cipher_list = list ;
}

void
ml_pty_ssh_set_keepalive_interval(
	u_int  interval_sec
	)
{
	keepalive_msec_left = keepalive_msec = interval_sec * 1000 ;
}

int
ml_pty_ssh_keepalive(
	u_int  spent_msec
	)
{
	if( keepalive_msec_left <= spent_msec)
	{
		u_int  count ;

		for( count = 0 ; count < num_of_sessions ; count++)
		{
			libssh2_keepalive_send( sessions[count]->obj , NULL) ;
		}

		keepalive_msec_left = keepalive_msec ;
	}
	else
	{
		keepalive_msec_left -= spent_msec ;
	}

	return  1 ;
}

void
ml_pty_ssh_set_use_x11_forwarding(
	int  use
	)
{
	use_x11_forwarding = use ;
}

u_int
ml_pty_ssh_get_x11_fds(
	int **  fds
	)
{
#ifndef  USE_WIN32API
	if( num_of_x11_fds > 0)
	{
		int  count ;

		for( count = num_of_x11_fds - 1 ; count >= 0 ; count--)
		{
			if( ! x11_channels[count].channel)
			{
				/* Already closed in ml_pty_ssh_send_recv_x11(). */
				x11_fds[count] = x11_fds[--num_of_x11_fds] ;
				x11_channels[count] = x11_channels[num_of_x11_fds] ;
			}
		#if  1
			else
			{
				/*
				 * Packets from ssh channels aren't necessarily
				 * detected by select(), so are received here
				 * in a certain interval.
				 * ( "+ 1" means x11_fd_t.channel.)
				 */
				ml_pty_ssh_send_recv_x11( count * 2 + 1) ;
			}
		#endif
		}

		*fds = x11_fds ;

		return  num_of_x11_fds * 2 ;
	}
	else
#endif
	{
		return  0 ;
	}
}

int
ml_pty_ssh_send_recv_x11(
	int  idx
	)
{
#ifndef  USE_WIN32API
	int  read_channel ;
	int  ret ;

	if( idx >= num_of_x11_fds * 2)
	{
		return  0 ;
	}

	read_channel = idx % 2 ;
	idx /= 2 ;

	if( read_channel)
	{
		ret = ssh_to_xserver( x11_channels[idx].channel , x11_fds[idx].display) ;
	}
	else
	{
		ret = xserver_to_ssh( x11_channels[idx].channel , x11_fds[idx].display) ;
	}

	if( ! ret)
	{
		closesocket( x11_fds[idx].display) ;

		libssh2_session_set_blocking( x11_channels[idx].session->obj , 1) ;
		libssh2_channel_free( x11_channels[idx].channel) ;
		ssh_disconnect( x11_channels[idx].session) ;

		x11_channels[idx].channel = NULL ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " x11 forwarding finished.\n") ;
	#endif
	}

	return  1 ;
#else
	return  0 ;
#endif
}
