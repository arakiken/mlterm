/*
 *	$Id$
 */

#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_dlfcn.h>

#include  "xwindow/x_display.h"
#include  "xwindow/x_event_source.h"


#ifdef  SYSCONFDIR
#define CONFIG_PATH SYSCONFDIR
#else
#define CONFIG_PATH "/etc"
#endif


/* --- global functions --- */

void
android_main(
	struct android_app *  app
	)
{
	int  argc = 1 ;
	char *  argv[] = { "mlterm" } ;

	x_display_init( app) ;

	kik_set_sys_conf_dir( CONFIG_PATH) ;

	if( ! main_loop_init( argc , argv))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " main_loop_init() failed.\n") ;
	#endif

		return ;
	}

	main_loop_start() ;

#if  0
	main_loop_final() ;
	kik_dl_close_all() ;
#endif
}
