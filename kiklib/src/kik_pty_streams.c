/*
 *	$Id$
 */

#include  "kik_pty.h"

#include  <fcntl.h>
#include  <grp.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <errno.h>
#include  <sys/param.h>
#include  <sys/ioctl.h>
#include  <sys/stat.h>
#include  <unistd.h>
#include  <sys/termios.h>
#ifdef  HAVE_STROPTS_H
#include  <stropts.h>
#endif
#ifdef  HAVE_SYS_STROPTS_H
#include  <sys/stropts.h>
#endif

#include  "kik_str.h"		/* strdup */
#include  "kik_debug.h"


/* Disable special character functions */
#ifdef  _POSIX_VDISABLE
#define  VDISABLE _POSIX_VDISABLE
#else
#define  VDISABLE 255
#endif


/* --- global functions --- */

/*
 * slave_name memory must be freed by a caller.
 */
pid_t
kik_pty_fork(
	int *  master ,
	char **  slave_name
	)
{
	int  slave ;
	pid_t pid ;
	char * ttydev;
	struct  termios  tio ;
	int fd;

	if( ( *master = open("/dev/ptmx", O_RDWR | O_NOCTTY, 0)) == -1)
	{
		return  -1;
	}
	if( grantpt(*master) < 0)
	{
	#if  0
		/* XXX this fails but it doesn't do harm in some environments */
		return  -1;
	#endif
	}
	if( unlockpt(*master) < 0)
	{
		return  -1;
	}
	if( ( ttydev = ptsname(*master)) == NULL)
	{
		return  -1;
	}

	fcntl(*master, F_SETFL, O_NDELAY);
	
	if( ( slave = open( ttydev, O_RDWR | O_NOCTTY, 0)) < 0)
	{
		return -1;
	}

	/*
	 * cygwin doesn't have isastream.
	 */
#ifdef  HAVE_ISASTREAM
	if( isastream(slave) == 1)
	{
		ioctl(slave, I_PUSH, "ptem");
		ioctl(slave, I_PUSH, "ldterm");
		ioctl(slave, I_PUSH, "ttcompat");
	}
#endif

	tio.c_iflag = BRKINT | IGNPAR | ICRNL | IXON ;
	tio.c_oflag = OPOST | ONLCR ;
	tio.c_cflag = CS8 | CREAD ;
	tio.c_lflag = ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK ;

	tio.c_cc[VEOF] = CEOF ;
	tio.c_cc[VEOL] = VDISABLE ;
	tio.c_cc[VERASE] = CERASE ;
	tio.c_cc[VINTR] = CINTR ;
	tio.c_cc[VKILL] = CKILL ;
	tio.c_cc[VQUIT] = CQUIT ;
	tio.c_cc[VSTART] = CSTART ;
	tio.c_cc[VSTOP] = CSTOP ;
	tio.c_cc[VSUSP] = CSUSP ;

#ifdef VDSUSP
	tio.c_cc[VDSUSP] = CDSUSP ;
#endif
#ifdef VREPRINT
	tio.c_cc[VREPRINT] = CRPRNT ;
#endif
#ifdef VRPRNT
	tio.c_cc[VRPRNT] = CRPRNT ;
#endif
#ifdef VDISCARD
	tio.c_cc[VDISCARD] = CFLUSH ;
#endif
#ifdef VFLUSHO
	tio.c_cc[VFLUSHO] = CFLUSH ;
#endif
#ifdef VWERASE
	tio.c_cc[VWERASE] = CWERASE ;
#endif
#ifdef VLNEXT
	tio.c_cc[VLNEXT] = CLNEXT ;
#endif
#ifdef VEOL2
	tio.c_cc[VEOL2] = VDISABLE ;
#endif
#ifdef VSWTC
	tio.c_cc[VSWTC] = VDISABLE ;
#endif
#ifdef VSWTCH
	tio.c_cc[VSWTCH] = VDISABLE ;
#endif
/*
 * VMIN == VEOF and VTIME == VEOL on old System V.
 */
#if VMIN != VEOF
	tio.c_cc[VMIN] = 1 ;
#endif
#if VTIME != VEOL
	tio.c_cc[VTIME] = 0 ;
#endif

	/*
	 * inheriting tty(0,1,2) settings ...
	 */

	for( fd = 0 ; fd <= 2 ; fd ++)
	{
		struct termios  def_tio ;

		if( tcgetattr( fd , &def_tio) == 0)
		{
			tio.c_cc[VEOF] = def_tio.c_cc[VEOF] ;
			tio.c_cc[VEOL] = def_tio.c_cc[VEOL] ;
			tio.c_cc[VERASE] = def_tio.c_cc[VERASE] ;
			tio.c_cc[VINTR] = def_tio.c_cc[VINTR] ;
			tio.c_cc[VKILL] = def_tio.c_cc[VKILL] ;
			tio.c_cc[VQUIT] = def_tio.c_cc[VQUIT] ;
			tio.c_cc[VSTART] = def_tio.c_cc[VSTART] ;
			tio.c_cc[VSTOP] = def_tio.c_cc[VSTOP] ;
			tio.c_cc[VSUSP] = def_tio.c_cc[VSUSP] ;

			break ;
		}
	}

	if( ( *slave_name = strdup( ttydev)) == NULL)
	{
		close( *master) ;
		close( slave) ;

		return  -1 ;
	}

	pid = fork() ;

	if( pid == -1)
	{
		/* fork failed */

		free( *slave_name) ;
		
		return  -1 ;
	}
	else if( pid == 0)
	{
		/* child */

		setsid() ;
		
		close(*master) ;
		
#ifdef TIOCNOTTY
		fd = open("/dev/tty", O_RDWR | O_NOCTTY);
		if (fd >= 0)
		{
			ioctl(fd, TIOCNOTTY, NULL);
			close(fd);
		}
#endif
		fd = open("/dev/tty", O_RDWR | O_NOCTTY);
		if (fd >= 0)
		{
			close(fd);
		}
		fd = open(ttydev, O_RDWR);
		if (fd >= 0) 
		{
			close(fd);
		}
		fd = open("/dev/tty", O_WRONLY);
		if (fd < 0)
		{
			return -1;
		}
		close(fd);

		dup2( slave , 0) ;
		dup2( slave , 1) ;
		dup2( slave , 2) ;

		if( slave > STDERR_FILENO)
		{
			close(slave) ;
		}

		cfsetispeed( &tio , B9600) ;
		cfsetospeed( &tio , B9600) ;

		if( tcsetattr(STDIN_FILENO, TCSANOW , &tio) < 0)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " tcsetattr() failed.\n") ;
		#endif
		}

		return  0 ;
	}

	close(slave) ;

	return  pid ;
}
