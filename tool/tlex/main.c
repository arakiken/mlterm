/*
 *	$Id$
 */

#include  <stdio.h>
#include  <stdarg.h>
#include  <stdlib.h>		/* getenv */
#include  <fcntl.h>		/* open */
#include  <unistd.h>		/* execve */
#include  <sys/select.h>	/* select */
#include  <libgen.h>		/* basename */
#include  <errno.h>
#include  <string.h>		/* memcpy */
#include  <signal.h>
#include  <sys/wait.h>
#include  <sys/socket.h>
#include  <sys/un.h>
#include  <netdb.h>
#include  <netinet/in.h>
#include  <time.h>
#include  <sys/stat.h>		/* mkdir */
#include  <termios.h>
#include  <limits.h>		/* PATH_MAX */

/* forkpty */
#if  defined(__linux__)
#include  <pty.h>
#elif  defined(__FreeBSD__)
#include  <libutil.h>
#else
#include  <util.h>
#endif

#define  IS_SERVER  (sock_fd != -1)

/* Output 64*1024 at most in re-attaching. */
#define  ATTACH_OUTPUT_SIZE  64*1024

#define  MAX_LEFT_LEN  1024


/* --- static variables --- */

static struct
{
	int  pty_fd ;
	int  connect_fd ;

	char  left[MAX_LEFT_LEN] ;
	ssize_t  left_len ;

	/* server data */
	pid_t  child_pid ;
	int  log_fd ;

} *  sessions ;
static u_int  num_of_sessions ;

static int  sock_fd = -1 ;
static char *  un_file ;

static struct termios  std_tio ;


/* --- static functions --- */

static void
help(void)
{
	printf( "Usage: tlex (-d /dev/...) command\n") ;
}

static char *
get_path(
	char *  file ,
	char *  suffix
	)
{
	char *  home ;
	char *  path ;

	if( ! ( home = getenv( "HOME")) ||
	    ! ( path = malloc( strlen( home) + 7 + strlen( file) + 11 + 1)))
	{
		return  NULL ;
	}

	if( suffix)
	{
		sprintf( path , "%s/.tlex/%s%s" , home , file , suffix) ;
	}
	else
	{
		sprintf( path , "%s/.tlex/%s" , home , file) ;
	}

	return  path ;
}

static void
debug_printf(
	const char *  format ,
	...
	)
{
	va_list  arg_list ;
	static char *  path ;
	FILE *  fp ;

	va_start( arg_list , format) ;

	if( ! path && ! ( path = get_path( "msg.log" , NULL)))
	{
		return ;
	}

	if( ( fp = fopen( path , "a")))
	{
		vfprintf( fp , format , arg_list) ;
		fclose( fp) ;
	}
}

static int
session_new(
	int  pty_fd ,
	int  connect_fd ,
	pid_t  child_pid ,
	int  log_fd
	)
{
	void *  p ;

	if( ! ( p = realloc( sessions , sizeof(*sessions) * (num_of_sessions + 1))))
	{
		return  0 ;
	}

	sessions = p ;
	sessions[num_of_sessions].pty_fd = pty_fd ;
	sessions[num_of_sessions].connect_fd = connect_fd ;
	sessions[num_of_sessions].left_len = 0 ;

	sessions[num_of_sessions].child_pid = child_pid ;
	sessions[num_of_sessions].log_fd = log_fd ;

	num_of_sessions ++ ;

#if  0
	debug_printf( "NEW SESSION (%s): PTY %d CONN %d\n" ,
		sock_fd == -1 ? "client" : "server" , pty_fd , connect_fd) ;
#endif

	return  1 ;
}

static void
session_delete(
	int  idx
	)
{
	if( sock_fd == -1)
	{
		tcsetattr( sessions[idx].pty_fd , TCSANOW , &std_tio) ;
	}

	close( sessions[idx].pty_fd) ;

	if( sessions[idx].connect_fd != -1)
	{
		close( sessions[idx].connect_fd) ;
	}

	if( sessions[idx].log_fd != -1)
	{
		close( sessions[idx].log_fd) ;
	}

	sessions[idx] = sessions[--num_of_sessions] ;
}

static void
sig_child(
	int  sig
	)
{
	pid_t  pid ;
	u_int  count ;

	while( ( pid = waitpid( -1 , NULL , WNOHANG)) == -1 && errno == EINTR)
	{
		errno = 0 ;
	}

	for( count = 0 ; count < num_of_sessions ; count++)
	{
		if( sessions[count].child_pid == pid)
		{
		#if  0
			debug_printf( "child exited (pid %d)\n" , pid) ;
		#endif

			session_delete( count) ;

			break ;
		}
	}

	signal( SIGCHLD , sig_child) ;
}

static void
sig_winch(
	int  sig
	)
{
	struct winsize  ws ;

	if( ioctl( sessions[0].pty_fd , TIOCGWINSZ , &ws) == 0)
	{
		char msg[5 + 11 * 2 + 1] ;

		sprintf( msg , "\x1b[<%d;%dz" , ws.ws_row , ws.ws_col) ;
		write( sessions[0].connect_fd , msg , strlen(msg)) ;
	}

	signal( SIGWINCH , sig_winch) ;
}

static void
set_winsize(
	char *  param ,
	int  w_fd
	)
{
	struct winsize  ws ;

	if( ( ws.ws_row = atoi( param)) > 0)
	{
		while( '0' <= *param && *param <= '9')
		{
			param ++ ;
		}

		if( *param == ';' && ( ws.ws_col = atoi( ++param)) > 0)
		{
			while( '0' <= *param && *param <= '9')
			{
				param ++ ;
			}

			ws.ws_xpixel = 0 ;
			ws.ws_ypixel = 0 ;

			ioctl( w_fd , TIOCSWINSZ , &ws) ;
		}
	}
}

static void
write_and_log(
	char *  seq ,
	char *  w_end ,		/* *w_end is not written. */
	char *  log_end ,	/* *log_end is not logged. */
	int  w_fd ,
	int  log_fd
	)
{
	if( w_fd != -1 && seq < w_end)
	{
		write( w_fd , seq , w_end - seq) ;
	}

	if( IS_SERVER && log_fd != -1 && seq < log_end)
	{
		write( log_fd , seq , log_end - seq) ;
	}
}

static int
increment_escseq(
	char **  current ,
	char *  restart ,
	ssize_t *  len
	)
{
	(*current)++ ;
	if(--(*len) == 0)
	{
		/* Wait next read */
		if( *current - restart <= MAX_LEFT_LEN)
		{
			*len = *current - restart ;
			*current = restart ;
		}

		return  0 ;
	}
	else
	{
		return  1 ;
	}
}

static int
parse_vtseq(
	char *  seq ,
	ssize_t  len ,
	int  w_fd ,
	int  log_fd ,
	char *  left ,
	ssize_t *  left_len
	)
{
	char *  seq_p ;

	if( ! IS_SERVER)
	{
		seq_p = seq + len ;
		len = 0 ;
	}
	else
	{
		seq_p = seq ;
	}

	while( len > 0)
	{
		if( *seq_p == '\x1b')
		{
		#define  INCREMENT  seq_p++ ; if(--len == 0) goto end
		#define  INCREMENT_ESCSEQ \
			if( ! increment_escseq( &seq_p , esc_beg , &len)) { goto  end ; }

			/* escape sequence */
			char *  esc_beg ;

			esc_beg = seq_p ++ ;
			if( --len == 0)
			{
				goto  end ;
			}

			if( *seq_p == '[')
			{
				/* CSI */

				char *  param ;

				INCREMENT_ESCSEQ ;

				/* Paramters */
				if( 0x30 <= *seq_p && *seq_p <= 0x3f)
				{
					param = seq_p ;

					do
					{
						INCREMENT_ESCSEQ ;
					}
					while( 0x30 <= *seq_p && *seq_p <= 0x3f) ;
				}
				else
				{
					param = NULL ;
				}

				/* Intermediates */
				if( 0x20 <= *seq_p && *seq_p <= 0x2f)
				{
					do
					{
						INCREMENT_ESCSEQ ;
					}
					while( 0x20 <= *seq_p && *seq_p <= 0x2f) ;
				}

				if( 0x40 <= *seq_p && *seq_p <= 0x7e)
				{
					/* final */

					if( *seq_p == 't' && param)
					{
						int  ps ;

						ps = atoi( param) ;
						if( ps == 14 || ps == 18)
						{
							/* window reporting (not logged) */

							write_and_log( seq , seq_p + 1 , esc_beg ,
								w_fd , log_fd) ;
							seq = seq_p + 1 ;
						}
					}
					else if( *seq_p == 'z' && *param == '<')
					{
						set_winsize( param + 1 , w_fd) ;

						write_and_log( seq , esc_beg , esc_beg ,
							w_fd , log_fd) ;
						seq = seq_p + 1 ;
					}
					else if(/* Primary DA(CSI c) or Secondary DA(CSI > c) */
						*seq_p == 'c' ||
						/* Device status report (CSI n) */
						*seq_p == 'n' ||
						/* Request terminal parameters (CSI x) */
						*seq_p == 'x')
					{
						write_and_log( seq , seq_p + 1 , esc_beg ,
							w_fd , log_fd) ;
						seq = seq_p + 1 ;
					}
				}
			}
			else if( *seq_p == 'Z')
			{
				/* device attribute (not logged) */
				write_and_log( seq , seq_p + 1 , esc_beg , w_fd , log_fd) ;
				seq = seq_p + 1 ;
			}
			else if( *seq_p == ']')
			{
				char *  text ;
				char  ch ;
				int  disable_logging ;

				disable_logging = 0 ;
				INCREMENT_ESCSEQ ;

				/* Parameters */
				if( 0x30 <= *seq_p && *seq_p <= 0x3f)
				{
					do
					{
						if( *seq_p == '?')
						{
							/* "OSC ] 10 ; ? BEL" etc */
							disable_logging = 1 ;
						}

						INCREMENT_ESCSEQ ;
					}
					while( 0x30 <= *seq_p && *seq_p <= 0x3f) ;
				}

				/* Text */

				text = seq_p ;

				while( 1)
				{
					if( *seq_p == '\x07')
					{
						break ;
					}
					else if( *seq_p == '\x1b' && *(seq_p + 1) == '\\')
					{
						INCREMENT_ESCSEQ ;
						break ;
					}

					INCREMENT_ESCSEQ ;
				}

				ch = *seq_p ;
				*seq_p = '\0' ;
				if( strstr( text , "mlclient"))
				{
					disable_logging = 1 ;
				}
				*seq_p = ch ;

				if( ! disable_logging)
				{
					write_and_log( seq , seq_p + 1 , esc_beg ,
						w_fd , log_fd) ;
					seq = seq_p + 1 ;
				}
			}
		}

		seq_p ++ ;
		len -- ;
	}
end:
	write_and_log( seq , seq_p , seq_p , w_fd , log_fd) ;

	if( len > 0)
	{
		memcpy( left , seq_p , len) ;
	}
	*left_len = len ;

	return  1 ;
}

static int
read_write(
	int  r_fd ,
	int  w_fd ,
	int  log_fd ,
	char *  left ,
	ssize_t *  left_len
	)
{
	ssize_t  len ;
	char  buf[4096] ;

#if  0
	debug_printf( "%s: read %d " , IS_SERVER ? "server" : "client" , r_fd) ;
#endif

	/* -1 is for buf[len] = '\0' */
	if( ( len = read( r_fd , buf + *left_len , sizeof(buf) - *left_len - 1)) > 0)
	{
		if( *left_len > 0)
		{
			memcpy( buf , left , *left_len) ;
		}

	#if  0
		buf[len] = '\0' ;
		debug_printf( " => %s(%d)\n" , buf , len) ;
	#endif

		parse_vtseq( buf , *left_len + len , w_fd , log_fd , left , left_len) ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

static int
set_fds(
	fd_set *  fds ,
	int  r_fd ,
	int  w_fd
	)
{
	if( r_fd >= 0)
	{
		FD_SET( r_fd , fds) ;
	}

	if( w_fd >= 0)
	{
		FD_SET( w_fd , fds) ;
	}

	if( r_fd > w_fd)
	{
		return  r_fd ;
	}
	else
	{
		return  w_fd ;
	}
}

static int
set_cloexec(
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

static void
end_daemon(void)
{
	close( sock_fd) ;
	unlink( un_file) ;
}

static int
start_daemon(void)
{
	char *  path ;
	size_t  path_len ;
	pid_t  pid ;
	int  fd ;
	struct sockaddr_un  servaddr ;

	/* The current process is not a process group leader before calling this function. */

	close( STDIN_FILENO) ;
	close( STDOUT_FILENO) ;
	close( STDERR_FILENO) ;

	/*
	 * This process becomes a session leader and purged from control terminal.
	 */
	if( setsid() == -1)
	{
		return  0 ;
	}

	/*
	 * SIGHUP signal when the child process exits must not be sent to
	 * the grandchild process.
	 */
	signal( SIGHUP , SIG_IGN) ;

	pid = fork() ;

	if( pid != 0)
	{
		exit(1) ;
	}

	/*
	 * grandchild
	 */

	if( ! ( path = get_path( "socket" , NULL)))
	{
		return  0 ;
	}

	path_len = strlen(path) ;
	path[path_len - 7] = '\0' ;
	if( path_len >= sizeof(servaddr.sun_path) ||
	    (mkdir( path , 0700) != 0 && errno != EEXIST))
	{
		free( path) ;

		return  0 ;
	}
	path[path_len - 7] = '/' ;

	memset( &servaddr , 0 , sizeof( servaddr)) ;
	servaddr.sun_family = AF_LOCAL ;
	strcpy( servaddr.sun_path , path) ;
	free( path) ;
	path = servaddr.sun_path ;

	if( ( fd = socket( PF_LOCAL , SOCK_STREAM , 0)) < 0)
	{
		return  0 ;
	}
	set_cloexec( fd);

	for( ;;)
	{
		int  ret ;
		int  saved_errno ;
		mode_t  mode ;

		mode = umask( 077) ;
		ret = bind( fd , (struct sockaddr *) &servaddr , sizeof( servaddr)) ;
		saved_errno = errno ;
		umask( mode) ;

		if( ret == 0)
		{
			break ;
		}
		else if( saved_errno == EADDRINUSE)
		{
			if( connect( fd , (struct sockaddr*) &servaddr , sizeof( servaddr)) == 0)
			{
				close( fd) ;

				return  0 ;
			}

			if( unlink( path) == 0)
			{
				continue ;
			}
		}
		else
		{
			close( fd) ;

			return  0 ;
		}
	}

	if( listen( fd , 1024) < 0)
	{
		close( fd) ;
		unlink( path) ;

		return  0 ;
	}

	un_file = strdup( path) ;
	sock_fd = fd ;

	signal( SIGCHLD , sig_child) ;

#if  0
	debug_printf( "Daemon started.\n") ;
#endif

	return  1 ;
}

static void
client_connected(void)
{
	struct sockaddr_un  addr ;
	socklen_t  sock_len ;
	int  connect_fd ;
	int  pty_fd ;
	char  slave_name[PATH_MAX] ;
	char  line[1024] ;
	size_t  line_len ;
	char  ch ;
	pid_t  pid ;
	u_int  count ;
	struct  winsize  ws ;
	char *  log_path ;

	sock_len = sizeof( addr) ;

	if( ( connect_fd = accept( sock_fd , (struct sockaddr *) &addr , &sock_len)) < 0)
	{
		return ;
	}

	/*
	 * Set the close-on-exec flag.
	 * If this flag off, this fd remained open until the child process forked in
	 * open_screen_intern()(ml_term_open_pty()) close it.
	 */
	set_cloexec( connect_fd) ;

	for( line_len = 0 ; line_len < sizeof(line) ; line_len ++)
	{
		if( read( connect_fd , &ch , 1) <= 0)
		{
			close( connect_fd) ;

			return ;
		}

		if( ch == '\n')
		{
			line[line_len++] = '\0' ;
			break ;
		}
		else
		{
			line[line_len] = ch ;
		}
	}

	for( count = 0 ; count < num_of_sessions ; count++)
	{
		if( sessions[count].connect_fd == -1)
		{
			char  buf[4096] ;
			ssize_t  size ;

		#if  0
			debug_printf( "PTY %d is revived.\n" , sessions[count].pty_fd) ;
		#endif

			sessions[count].connect_fd = connect_fd ;

			if( lseek( sessions[count].log_fd , -ATTACH_OUTPUT_SIZE , SEEK_CUR) == -1)
			{
				lseek( sessions[count].log_fd , 0 , SEEK_SET) ;
			}
			while( ( size = read( sessions[count].log_fd , buf , sizeof(buf) - 1)) > 0)
			{
				buf[size] = '\0' ;
				write( connect_fd , buf , size) ;
			}

			return ;
		}
	}

	if( ( pid = forkpty( &pty_fd , slave_name , NULL , &ws)) == 0)
	{
		char *  argv[10] ;
		int  count ;
		struct termios  tio ;

		tcgetattr(STDIN_FILENO , &tio) ;
		memcpy( tio.c_cc , std_tio.c_cc , sizeof(tio.c_cc)) ;
		tio.c_cc[VERASE] = '\177' ;
		tcsetattr(STDIN_FILENO , TCSANOW , &tio) ;

		/* reset signals */
		signal(SIGINT , SIG_DFL) ;
		signal(SIGQUIT , SIG_DFL) ;
		signal(SIGCHLD , SIG_DFL) ;
		signal(SIGPIPE , SIG_DFL) ;

		count = 0 ;

		if( *line == '\0')
		{
			argv[0] = getenv( "SHELL") ;
		}
		else
		{
			char *  p ;

			p = argv[0] = line ;

			while( line_len > 0)
			{
				p += (strlen(p) + 1) ;
				line_len -= (strlen(p) + 1) ;

				if( ++count >= sizeof(argv) / sizeof(argv[0]))
				{
					break ;
				}

				argv[count] = p ;
			}
		}

		argv[count + 1] = NULL ;

	#if  0
		debug_printf( "Command to be executed: %s %s\n" , argv[0] , argv[1]) ;
	#endif

		if( strchr( argv[0] , '/') ?
		      execv( argv[0] , argv) == -1 :
		      execvp( argv[0] , argv) == -1)
		{
		#if  0
			debug_printf( "exev() failed.\n") ;
		#endif
		}
	}

	if( ( log_path = get_path( "log-" , strrchr( slave_name , '/') + 1)))
	{
		unlink( log_path) ;
	}

	session_new( pty_fd , connect_fd , pid ,
		log_path ? open( log_path , O_RDWR|O_CREAT , 0644) : -1) ;
	free( log_path) ;
}

static int
connect_to_server(
	int  argc ,
	char **  argv
	)
{
	char *  path ;
	int  connect_fd ;
	int  pty_fd ;
	struct sockaddr_un  servaddr ;
	int  count ;
	struct termios  tio ;

	if( ! ( path = get_path( "socket" , NULL)))
	{
		return  0 ;
	}

	if( ( connect_fd = socket( AF_LOCAL , SOCK_STREAM , 0)) == -1)
	{
		free( path) ;

		return  0 ;
	}

	memset( &servaddr , 0 , sizeof( servaddr)) ;
	servaddr.sun_family = AF_LOCAL ;
	strcpy( servaddr.sun_path , path) ;
	free( path) ;
	path = servaddr.sun_path ;

	if( connect( connect_fd , (struct sockaddr*) &servaddr , sizeof( servaddr)) == -1)
	{
		close( connect_fd) ;

		return  0 ;
	}

	for( count = 0 ; count < argc ; count++)
	{
		write( connect_fd , argv[count] , strlen(argv[count])) ;
		write( connect_fd , "\0" , 1) ;
	}

	write( connect_fd , "\n" , 1) ;

	if( ( pty_fd = open( ttyname(STDIN_FILENO) , O_RDWR)) == -1)
	{
		close( connect_fd) ;

		return  0 ;
	}

	close(STDIN_FILENO) ;
	close(STDOUT_FILENO) ;
	close(STDERR_FILENO) ;

	tio = std_tio ;
	tio.c_iflag &= ~(IXON|IXOFF|ICRNL|INLCR|IGNCR|IMAXBEL|ISTRIP) ;
	tio.c_iflag |= IGNBRK ;
	tio.c_oflag &= ~(OPOST|ONLCR|OCRNL|ONLRET) ;
	tio.c_lflag &= ~(IEXTEN|ICANON|ECHO|ECHOE|ECHONL|ECHOCTL|ECHOPRT|ECHOKE|ECHOCTL|ISIG) ;
	tio.c_cc[VMIN] = 1 ;
	tio.c_cc[VTIME] = 0 ;

	tcsetattr( pty_fd , TCSANOW , &tio) ;

	session_new( pty_fd , connect_fd , -1 , -1) ;

	signal( SIGWINCH , sig_winch) ;
	sig_winch(SIGWINCH) ;

	return  1 ;
}

static void
check_log_file_size(
	int  fd
	)
{
	off_t  pos ;

	pos = lseek( fd , 0 , SEEK_CUR) ;

	if( pos >= 5*1024*1024)
	{
		/* Over 5MB */
		char  buf[ATTACH_OUTPUT_SIZE] ;

		lseek( fd , -ATTACH_OUTPUT_SIZE , SEEK_END) ;
		if( read( fd , buf , ATTACH_OUTPUT_SIZE) == ATTACH_OUTPUT_SIZE) ;
		{
			lseek( fd , 0 , SEEK_SET) ;
			ftruncate( fd , 0) ;
			write( fd , buf , ATTACH_OUTPUT_SIZE) ;
		}
	}

}


/* --- global functions --- */

int
main(
	int  argc ,
	char **  argv
	)
{
	fd_set  fds ;
	int  maxfd ;
	int  count ;

	if( argc == 2 && strcmp( argv[1] , "-h") == 0)
	{
		help() ;

		return  0 ;
	}

	tcgetattr( STDIN_FILENO , &std_tio) ;

	argv ++ ;
	argc -- ;

	if( ! connect_to_server( argc , argv))
	{
		pid_t  parent_pid ;
		pid_t  child_pid ;

		parent_pid = getpid() ;

		if( ( child_pid = fork()) == 0)
		{
			if( ! start_daemon())
			{
				kill( parent_pid , SIGTERM) ;
			}
		}
		else
		{
			waitpid( child_pid , NULL , 0) ;

			do
			{
				usleep( 100) ;
			}
			while( ! connect_to_server( argc , argv)) ;
		}
	}

	while( 1)
	{
		maxfd = 0 ;
		FD_ZERO(&fds) ;

		for( count = 0 ; count < num_of_sessions ; count++)
		{
			int  max ;

			if( ( max = set_fds( &fds , sessions[count].pty_fd ,
					sessions[count].connect_fd)) > maxfd)
			{
				maxfd = max ;
			}
		}

		if( sock_fd >= 0)
		{
			FD_SET( sock_fd , &fds) ;
			if( maxfd < sock_fd)
			{
				maxfd = sock_fd ;
			}
		}

		if( select( maxfd + 1 , &fds , NULL , NULL , NULL) < 0)
		{
			/* received signal */
			continue ;
		}

		for( count = num_of_sessions - 1 ; count >= 0 ; count--)
		{
			if( FD_ISSET( sessions[count].pty_fd , &fds))
			{
				if( ! read_write( sessions[count].pty_fd ,
						sessions[count].connect_fd ,
						sessions[count].log_fd ,
						sessions[count].left ,
						&sessions[count].left_len))
				{
				#if  0
					debug_printf( "%s reads nothing from pty.\n" ,
						IS_SERVER ? "server" : "client") ;
				#endif

					session_delete( count) ;
				}

				if( IS_SERVER && sessions[count].log_fd != -1)
				{
					check_log_file_size( sessions[count].log_fd) ;
				}
			}

			if( sessions[count].connect_fd != -1 &&
			    FD_ISSET( sessions[count].connect_fd , &fds))
			{
				if( ! read_write( sessions[count].connect_fd ,
						sessions[count].pty_fd , -1 ,
						sessions[count].left ,
						&sessions[count].left_len))
				{
				#if  0
					debug_printf( "%s reads nothing from network.\n" ,
						IS_SERVER ? "server" : "client") ;
				#endif

					if( IS_SERVER)
					{
						/* pty_fd survives. */
						close( sessions[count].connect_fd) ;
						sessions[count].connect_fd = -1 ;
					}
					else
					{
						session_delete( count) ;
					}
				}
			}
		}

		if( IS_SERVER && FD_ISSET( sock_fd , &fds))
		{
			client_connected() ;
		}

		if( num_of_sessions == 0)
		{
			break ;
		}
	}

	if( IS_SERVER)
	{
		end_daemon() ;
	}

	return  0 ;
}
