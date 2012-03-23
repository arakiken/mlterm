/*
 *	$Id$
 */

#include  <sys/types.h>

#include  <kiklib/kik_def.h>		/* USE_WIN32API */
#include  <kiklib/kik_unistd.h>		/* kik_getuid/kik_getgid */
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_privilege.h>
#include  <kiklib/kik_debug.h>

#if  defined(USE_WIN32API) && defined(USE_LIBSSH2)
#include  <windows.h>
#endif
#include  <x.h>

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
