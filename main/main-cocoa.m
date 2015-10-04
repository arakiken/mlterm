/*
 *	$Id$
 */

#import  <Cocoa/Cocoa.h>
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_dlfcn.h>
#include  "main_loop.h"


#if  defined(SYSCONFDIR)
#define CONFIG_PATH SYSCONFDIR
#else
#define CONFIG_PATH "/etc"
#endif


/* --- global functions --- */

int
main(
	int  argc ,
	const char *  argv[]
	)
{
	kik_set_sys_conf_dir( CONFIG_PATH) ;
	kik_set_msg_log_file_name( "mlterm/msg.log") ;

	char *  myargv[] = { "mlterm" , NULL } ;
	main_loop_init( 1 , myargv) ;

	return  NSApplicationMain( argc , argv) ;
}