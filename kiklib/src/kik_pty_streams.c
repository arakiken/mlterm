/*
 *	$Id$
 */

/*
 * _GNU_SOURCE must be defined before including <features.h> to take effect.
 * since standard headers, kik_types.h and kik_def.h include features.h indirectly,
 * ecplicitly evaluate only the autoconf's result here.
 */
#include  "kik_config.h"
#ifdef HAVE_GNU_SOURCE
#define _GNU_SOURCE
#endif

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
#ifdef  __ANDROID__
#include  <termios.h>
#else
#include  <sys/termios.h>
#endif
#ifdef  HAVE_STROPTS_H
#include  <stropts.h>
#endif
#ifdef  HAVE_SYS_STROPTS_H
#include  <sys/stropts.h>
#endif

#include  "kik_str.h"		/* strdup */
#include  "kik_file.h"
#include  "kik_debug.h"
#include  "kik_sig_child.h"

/* Disable special character functions */
#ifdef  _POSIX_VDISABLE
#define  VDISABLE _POSIX_VDISABLE
#else
#define  VDISABLE 255
#endif

/* For Android */
#ifndef  CEOF
#define  CEOF  ('d' & 0x1f)
#endif
#ifndef  CERASE
#define  CERASE  0x7f
#endif
#ifndef  CINTR
#define  CINTR  ('c' & 0x1f)
#endif
#ifndef  CKILL
#define  CKILL  ('u' & 0x1f)
#endif
#ifndef  CQUIT
#define  CQUIT  0x1c
#endif
#ifndef  CSTART
#define  CSTART  ('q' & 0x1f)
#endif
#ifndef  CSTOP
#define  CSTOP  ('s' & 0x1f)
#endif
#ifndef  CSUSP
#define  CSUSP  ('z' & 0x1f)
#endif
#ifndef  CDSUSP
#define  CDSUSP  ('y' & 0x1f)
#endif
#ifndef  CRPRNT
#define  CRPRNT  ('r' & 0x1f)
#endif
#ifndef  CFLUSH
#define  CFLUSH  ('o' & 0x1f)
#endif
#ifndef  CWERASE
#define  CWERASE  ('w' & 0x1f)
#endif
#ifndef  CLNEXT
#define  CLNEXT  ('v' & 0x1f)
#endif


/* --- global functions --- */

pid_t
kik_pty_fork(
	int *  master ,
	int *  slave
	)
{
	int mode;
	pid_t pid;
	char * ttydev;
	struct  termios  tio;
	int fd;

#ifdef  HAVE_POSIX_OPENPT 
	*master = posix_openpt( O_RDWR | O_NOCTTY);
#else
	*master = open( "/dev/ptmx", O_RDWR | O_NOCTTY, 0);
#endif
	if( *master < 0)
	{
		kik_error_printf( "Couldn't open a master pseudo-terminal device.\n") ;
		return  -1;
	}
	kik_file_set_cloexec( *master) ;
	/*
	 * The behaviour of the grantpt() function is unspecified
	 * if the application has installed a signal handler to catch SIGCHLD signals.
	 * (see http://www.opengroup.org/onlinepubs/007908799/xsh/grantpt.html)
	 */
	kik_sig_child_suspend() ;
	if( grantpt(*master) < 0)
	{
	#if  0
		/* XXX this fails but it doesn't do harm in some environments */
		kik_sig_child_resume() ;
		return  -1;
	#endif
	}
	kik_sig_child_resume() ;
	if( unlockpt(*master) < 0)
	{
		close(*master) ;
		
		return  -1;
	}
	if( ( ttydev = ptsname(*master)) == NULL)
	{
		kik_error_printf( "Couldn't open a slave pseudo-terminal device.\n") ;
	#ifdef  __linux__
		kik_msg_printf( "If your operating system is Linux, make sure your kernel was compiled with\n"
			        "'CONFIG_UNIX98_PTYS=y' and devpts file system was mounted.\n");
	#endif

		close(*master) ;
		
		return  -1;
	}

	if( ( mode = fcntl( *master, F_GETFL, 0)) == -1 ||
	    ( (mode & O_NDELAY) == 0 && fcntl( *master, F_SETFL, mode|O_NDELAY) == -1) )
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Failed to set pty master non-blocking.\n") ;
	#endif
	}

	if( ( *slave = open( ttydev, O_RDWR | O_NOCTTY, 0)) < 0)
	{
		close(*master) ;
		
		return  -1 ;
	}

	/*
	 * cygwin doesn't have isastream.
	 */
#if  defined(HAVE_ISASTREAM) && defined(I_PUSH)
	if( isastream(*slave) == 1)
	{
		ioctl(*slave, I_PUSH, "ptem");
		ioctl(*slave, I_PUSH, "ldterm");
		ioctl(*slave, I_PUSH, "ttcompat");
	}
#endif
	memset( &tio, 0, sizeof( struct termios)) ;

	tio.c_iflag = BRKINT | IGNPAR | ICRNL | IXON ;
	tio.c_oflag = OPOST | ONLCR ;
	tio.c_cflag = CS8 | CREAD ;
	tio.c_lflag = ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK ;
#ifdef  ECHOKE
	tio.c_lflag |= ECHOKE ;
#endif
#ifdef  ECHOCTL
	tio.c_lflag |= ECHOCTL ;
#endif

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

	pid = fork() ;

	if( pid == -1)
	{
		/* fork failed */

		close( *master) ;
		close( *slave) ;

		return  -1 ;
	}
	else if( pid == 0)
	{
		/* child */

	#ifdef  __MSYS__
		/*
		 * XXX
		 * I don't know why but following hack doesn't effect in MSYS/Windows7.
		 * Set "CYGWIN=tty" environmental variable in msys.bat instead.
		 */
	#if  0
		/*
		 * XXX
		 * I don't know why but FreeConsole() is called in setsid()
		 * unless following codes(close(0...2)) are placed between
		 * fork() and setsid() (before dup2()) in msys.
		 * If FreeConsole() is called in setsid(), each non-msys program
		 * which is started by shell in mlterm opens command prompt
		 * window...
		 */
		for( fd = 0 ; fd <= 2 ; fd++)
		{
			if( fd != *slave)
			{
				close( fd) ;
			}
		}
	#endif
	#endif

		close(*master) ;

	#ifdef HAVE_SETSID
		setsid() ;
	#else /* HAVE_SETSID */
	#ifdef TIOCNOTTY
		fd = open("/dev/tty", O_RDWR | O_NOCTTY);
		if (fd >= 0)
		{
			ioctl(fd, TIOCNOTTY, NULL);
			close(fd);
		}
	#endif /* TIOCNOTTY */
	#endif /* HAVE_SETSID */

	#ifdef TIOCSCTTY /* BSD (in addition Linux also knows TIOCSCTTY) */
		if(ioctl(*slave, TIOCSCTTY, NULL) < 0)
		{
			return -1 ;
		}
	#else /* no TIOCSCTTY (SysV) */
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
	#endif /* no TIOCSCTTY (SysV) */

		dup2( *slave , 0) ;
		dup2( *slave , 1) ;
		dup2( *slave , 2) ;

		if( *slave > STDERR_FILENO)
		{
			close(*slave) ;
		}

	#ifdef  B38400
		cfsetispeed( &tio , B38400) ;
		cfsetospeed( &tio , B38400) ;
	#else
		cfsetispeed( &tio , B9600) ;
		cfsetospeed( &tio , B9600) ;
	#endif

		if( tcsetattr(STDIN_FILENO, TCSANOW , &tio) < 0)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " tcsetattr() failed.\n") ;
		#endif
		}

		return  0 ;
	}

	kik_file_set_cloexec( *slave) ;

	return  pid ;
}

int
kik_pty_close(
	int  master
	)
{
	close( master) ;

	return  0 ;
}

void
kik_pty_helper_set_flag(
	int  lastlog ,
	int  utmp ,
	int  wtmp
	)
{
}
