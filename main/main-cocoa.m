/*
 *	$Id$
 */

#import  <Cocoa/Cocoa.h>
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_dlfcn.h>
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_unistd.h>
#include  "main_loop.h"


#if  defined(SYSCONFDIR)
#define CONFIG_PATH SYSCONFDIR
#else
#define CONFIG_PATH "/etc"
#endif


/* --- static functions --- */

static void
set_lang(void)
{
	char *  locale ;

	if( ( locale = [[[NSLocale currentLocale] objectForKey:NSLocaleIdentifier] UTF8String]))
	{
		char *  p ;

		if( ( p = alloca( strlen( locale) + 7)))
		{
			sprintf( p , "%s.UTF-8" , locale) ;
			kik_setenv( "LANG" , p , 1) ;
		}
	}
}


/* --- global functions --- */

int
main(
	int  argc ,
	const char *  argv[]
	)
{
	if( ! getenv( "LANG"))
	{
		set_lang() ;
	}

	kik_set_sys_conf_dir( CONFIG_PATH) ;
	kik_set_msg_log_file_name( "mlterm/msg.log") ;

	char *  myargv[] = { "mlterm" , NULL } ;
	main_loop_init( 1 , myargv) ;

	return  NSApplicationMain( argc , argv) ;
}
