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


#if  0
#define  SAVE_DEFAULT_FONT
#endif


#ifdef  SAVE_DEFAULT_FONT

/* --- global variables --- */

char *  default_font_path ;


/* --- static functions --- */

static inline void
save_default_font(
	ANativeActivity *  activity
	)
{
	JNIEnv *  env ;
	JavaVM *  vm ;
	jobject  this ;
	jstring  jstr ;
	const char *  path ;

	if( default_font_path)
	{
		return ;
	}

	vm = activity->vm ;
	(*vm)->AttachCurrentThread( vm , &env , NULL) ;

	this = activity->clazz ;
	jstr = (*env)->CallObjectMethod( env , this ,
		(*env)->GetMethodID( env , (*env)->GetObjectClass( env , this) ,
			"saveDefaultFont" , "()Ljava/lang/String;")) ;

	path = (*env)->GetStringUTFChars( env , jstr , NULL) ;
	default_font_path = strdup( path) ;
	(*env)->ReleaseStringUTFChars( env , jstr , path) ;

	(*vm)->DetachCurrentThread(vm) ;
}

#endif	/* SAVE_DEFAULT_FONT */


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

#ifdef  SAVE_DEFAULT_FONT
	save_default_font( app->activity) ;
#endif

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
