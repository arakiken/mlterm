/*
 *	$Id$
 */

#include  <sys/types.h>

#include  <kiklib/kik_def.h>		/* USE_WIN32API/HAVE_WINDOWS_H */
#include  <kiklib/kik_unistd.h>		/* kik_getuid/kik_getgid */
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_privilege.h>
#include  <kiklib/kik_debug.h>

#ifdef  HAVE_WINDOWS_H
#include  <windows.h>
#endif

#include  "main_loop.h"

#if  defined(USE_WIN32API)
#define CONFIG_PATH "."
#elif  defined(SYSCONFDIR)
#define CONFIG_PATH SYSCONFDIR
#else
#define CONFIG_PATH "/etc"
#endif

#ifdef  USE_WIN32API
#define  argv  __argv
#define  argc  __argc
#endif


/* --- static functions --- */

#if  defined(HAVE_WINDOWS_H) && ! defined(USE_WIN32API)

#include  <stdio.h>		/* sprintf */
#include  <sys/utsname.h>

#include  <kiklib/kik_str.h>
#include  <kiklib/kik_util.h>

static void
check_console(void)
{
	int  count ;
	HWND  conwin ;
	char  app_name[6 + DIGIT_STR_LEN(u_int) + 1] ;
	HANDLE  handle ;

	if( ! ( handle = GetStdHandle(STD_OUTPUT_HANDLE)) ||
	    handle == INVALID_HANDLE_VALUE)
	{
	#if  0
		struct utsname  name ;
		char *  rel ;

		if( uname( &name) == 0 &&
		    ( rel = kik_str_alloca_dup( name.release)))
		{
			char *  p ;

			if( ( p = strchr( rel , '.')))
			{
				int  major ;
				int  minor ;

				*p = '\0' ;
				major = atoi( rel) ;
				rel = p + 1 ;

				if( ( p = strchr( rel , '.')))
				{
					*p = '\0' ;
					minor = atoi( rel) ;

					if( major >= 2 || (major == 1 && minor >= 7))
					{
						/*
						 * Mlterm works without console
						 * in cygwin 1.7 or later.
						 */
						return ;
					}
				}
			}
		}

		/* AllocConsole() after starting mlterm doesn't work on MSYS. */
		if( ! AllocConsole())
	#endif
		{
			return ;
		}
	}

	/* Hide allocated console window */

	sprintf( app_name, "mlterm%08x", (unsigned int)GetCurrentThreadId()) ;
	LockWindowUpdate( GetDesktopWindow()) ;
	SetConsoleTitle( app_name) ;

	for( count = 0 ; count < 20 ; count ++)
	{
		if( ( conwin = FindWindow( NULL, app_name)))
		{
			ShowWindowAsync( conwin, SW_HIDE) ;
			break ;
		}

		Sleep(40) ;
	}

	LockWindowUpdate( NULL) ;
}

#else

#define  check_console()  (1)

#endif


/* --- global functions --- */

#ifdef  USE_WIN32API
int PASCAL
WinMain(
	HINSTANCE  hinst ,
	HINSTANCE  hprev ,
	char *  cmdline ,
	int  cmdshow
	)
#else
int
main(
	int  argc ,
	char **  argv
	)
#endif
{
#if  defined(USE_WIN32API) && defined(USE_LIBSSH2)
	WSADATA wsadata ;

	WSAStartup( MAKEWORD(2,0), &wsadata) ;
#endif

	check_console() ;

	/* normal user */
	kik_priv_change_euid( kik_getuid()) ;
	kik_priv_change_egid( kik_getgid()) ;

	kik_set_sys_conf_dir( CONFIG_PATH) ;

	if( ! main_loop_init( argc , argv))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_term_manager_init() failed.\n") ;
	#endif

		return  1 ;
	}

	main_loop_start() ;

	main_loop_final() ;

	/*
	 * Not reachable in unix.
	 * Reachable in win32.
	 */

#if  defined(USE_WIN32API) && defined(USE_LIBSSH2)
	WSACleanup() ;
#endif

	return  0 ;
}

