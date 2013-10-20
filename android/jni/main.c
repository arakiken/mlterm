/*
 *	$Id$
 */

#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_dlfcn.h>
#include  <ml_term_manager.h>
#include  "xwindow/x_display.h"
#include  "xwindow/x_event_source.h"


#ifdef  SYSCONFDIR
#define CONFIG_PATH SYSCONFDIR
#else
#define CONFIG_PATH "/etc"
#endif


/* --- static functions --- */

static inline void
save_unifont(
	ANativeActivity *  activity
	)
{
	JNIEnv *  env ;
	JavaVM *  vm ;
	jobject  this ;

	vm = activity->vm ;
	(*vm)->AttachCurrentThread( vm , &env , NULL) ;

	this = activity->clazz ;
	(*env)->CallVoidMethod( env , this ,
		(*env)->GetMethodID( env , (*env)->GetObjectClass( env , this) ,
			"saveUnifont" , "()V")) ;

	(*vm)->DetachCurrentThread(vm) ;
}


/* --- global functions --- */

void
android_main(
	struct android_app *  app
	)
{
	int  argc = 1 ;
	char *  argv[] = { "mlterm" } ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " android_main started.\n") ;
#endif

	kik_set_sys_conf_dir( CONFIG_PATH) ;

	save_unifont( app->activity) ;

	if( x_display_init( app) &&		/* x_display_init() returns 1 only once. */
	    ! main_loop_init( argc , argv))	/* main_loop_init() is called once. */
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " main_loop_init() failed.\n") ;
	#endif

		return ;
	}

	main_loop_start() ;

	/* Only screen objects are closed. */
	x_screen_manager_suspend() ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " android_main finished.\n") ;
#endif
}
