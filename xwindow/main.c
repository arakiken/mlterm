/*
 *	$Id$
 */

#include  <sys/types.h>
#include  <unistd.h>		/* getuid/getgid */
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_locale.h>
#include  <kiklib/kik_privilege.h>
#include  <kiklib/kik_sig_child.h>
#include  <kiklib/kik_debug.h>

#include  "x_term_manager.h"


#ifndef  SYSCONFDIR
#define  CONFIG_PATH  "/etc"
#else
#define  CONFIG_PATH  SYSCONFDIR
#endif


/* --- global functions --- */

int
main(
	int  argc ,
	char **  argv
	)
{
	kik_sig_child_init() ;
	
	/* normal user */
	kik_priv_change_euid( getuid()) ;
	kik_priv_change_egid( getgid()) ;

	if( ! kik_locale_init(""))
	{
		kik_msg_printf( "locale settings failed.\n") ;
	}

	kik_set_sys_conf_dir( CONFIG_PATH) ;

	if( ! x_term_manager_init( argc , argv))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_term_manager_init() failed.\n") ;
	#endif

		return  1 ;
	}

	x_term_manager_event_loop() ;

	/*
	 * not reachable.
	 */

	x_term_manager_final() ;
	kik_locale_final() ;

	return  0 ;
}
