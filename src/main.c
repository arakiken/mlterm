/*
 *	$Id$
 */

#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_locale.h>

#include  "ml_term_manager.h"


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
	ml_term_manager_t  term_man ;

	if( ! kik_locale_init(""))
	{
		kik_msg_printf( "locale settings failed.\n") ;
	}

	kik_set_sys_conf_dir( CONFIG_PATH) ;

	if( ! ml_term_manager_init( &term_man , argc , argv))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_term_manager_init() failed.\n") ;
	#endif

		return  1 ;
	}

	ml_term_manager_event_loop( &term_man) ;

	/*
	 * not reachable.
	 */

	ml_term_manager_final( &term_man) ;	
	kik_locale_final() ;

	return  0 ;
}
