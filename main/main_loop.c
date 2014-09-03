/*
 *	$Id$
 */

#include  "main_loop.h"

#include  <signal.h>
#include  <unistd.h>		/* getpid */
#include  <kiklib/kik_locale.h>
#include  <kiklib/kik_sig_child.h>
#include  <kiklib/kik_mem.h>	/* kik_alloca_garbage_collect */
#include  <kiklib/kik_str.h>	/* kik_str_alloca_dup */
#include  <kiklib/kik_def.h>	/* USE_WIN32API */

#include  <x_font.h>	/* x_use_cp932_ucs_fot_xft */
#include  <x_screen_manager.h>
#include  <x_event_source.h>
#if ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
#include  <xlib/x_xim.h>
#endif

#include  "version.h"
#include  "daemon.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static int8_t  is_genuine_daemon ;


/* --- static functions --- */

/*
 * signal handlers.
 */
#ifndef  USE_WIN32API
static void
sig_fatal( int  sig)
{
#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG "signal %d is received\n" , sig) ;
#endif

	/* Remove ~/.mlterm/socket. */
	daemon_final() ;

	/* reset */
	signal( sig , SIG_DFL) ;

	kill( getpid() , sig) ;
}
#endif	/* USE_WIN32API */

static int
get_font_size_range(
	u_int *  min ,
	u_int *  max ,
	const char *  str
	)
{
	char *  str_p ;
	char *  p ;

	if( ( str_p = kik_str_alloca_dup(str)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif

		return  0 ;
	}

	/* kik_str_sep() never returns NULL because str_p isn't NULL. */
	p = kik_str_sep( &str_p , "-") ;

	if( str_p  == NULL)
	{
		kik_msg_printf( "max font size is missing.\n");

		return 0;
	}
	
	if( ! kik_str_to_uint( min , p))
	{
		kik_msg_printf( "min font size %s is not valid.\n" , p) ;

		return  0 ;
	}

	
	if( ! kik_str_to_uint( max , str_p))
	{
		kik_msg_printf( "max font size %s is not valid.\n" , str_p) ;

		return  0 ;
	}

	return  1 ;
}

#ifdef  USE_LIBSSH2
static void
ssh_keepalive(void)
{
	ml_pty_ssh_keepalive( 100) ;
}
#endif


/* --- global functions --- */

int
main_loop_init(
	int  argc ,
	char **  argv
	)
{
	x_main_config_t  main_config ;
	kik_conf_t *  conf ;
	char *  value ;
#if ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
	int  use_xim ;
#endif
	u_int  max_screens_multiple ;
	u_int  num_of_startup_screens ;
	u_int  depth ;
	char *  invalid_msg = "%s %s is not valid.\n" ;
	char *  orig_argv ;

	if( ! kik_locale_init(""))
	{
		kik_msg_printf( "locale settings failed.\n") ;
	}

	kik_sig_child_init() ;

	kik_init_prog( argv[0] , DETAIL_VERSION) ;

	if( ( conf = kik_conf_new()) == NULL)
	{
		return  0 ;
	}
	
	x_prepare_for_main_config( conf) ;

	/*
	 * Same processing as vte_terminal_class_init().
	 * Following options are not possible to specify as arguments of mlclient.
	 * 1) Options which are used only when mlterm starts up and which aren't
	 *    changed dynamically. (e.g. "startup_screens")
	 * 2) Options which change status of all ptys or windows. (Including ones
	 *    which are possible to change dynamically.)
	 *    (e.g. "font_size_range")
	 */

	kik_conf_add_opt( conf , '@' , "screens" , 0 , "startup_screens" ,
		"number of screens to open in start up [1]") ;
	kik_conf_add_opt( conf , 'h' , "help" , 1 , "help" ,
		"show this help message") ;
	kik_conf_add_opt( conf , 'v' , "version" , 1 , "version" ,
		"show version message") ;
	kik_conf_add_opt( conf , 'R' , "fsrange" , 0 , "font_size_range" , 
		"font size range for GUI configurator [6-30]") ;
#if  ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
	kik_conf_add_opt( conf , 'Y' , "decsp" , 1 , "compose_dec_special_font" ,
		"compose dec special font [false]") ;
#endif
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
	kik_conf_add_opt( conf , 'c' , "cp932" , 1 , "use_cp932_ucs_for_xft" , 
		"use CP932-Unicode mapping table for JISX0208 [false]") ;
#endif
#if  ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
	kik_conf_add_opt( conf , 'i' , "xim" , 1 , "use_xim" , 
		"use XIM (X Input Method) [true]") ;
#endif
#ifndef  USE_WIN32API
	kik_conf_add_opt( conf , 'j' , "daemon" , 0 , "daemon_mode" ,
	#ifdef  USE_WIN32GUI
		/* 'genuine' is not supported in win32. */
		"start as a daemon (none/blend) [none]"
	#else
		"start as a daemon (none/blend/genuine) [none]"
	#endif
		) ;
#endif
	kik_conf_add_opt( conf , '\0' , "depth" , 0 , "depth" ,
		"visual depth") ;
	kik_conf_add_opt( conf , '\0' , "maxptys" , 0 , "max_ptys" ,
		"max ptys to open simultaneously (multiple of 32)") ;
#ifdef  USE_LIBSSH2
	kik_conf_add_opt( conf , '\0' , "keepalive" , 0 , "ssh_keepalive_interval" ,
		"interval seconds to send keepalive. [0 = not send]") ;
#endif
	kik_conf_add_opt( conf , '\0' , "metaprefix" , 0 , "mod_meta_prefix" ,
		"prefix characters in pressing meta key if mod_meta_mode = esc") ;
	kik_conf_add_opt( conf , '\0' , "deffont" , 0 , "default_font" , "default font") ;
	kik_conf_add_opt( conf , '\0' , "point" , 1 , "use_point_size" ,
		"treat fontsize as point instead of pixel") ;

	orig_argv = argv ;
	if( ! kik_conf_parse_args( conf , &argc , &argv , 0))
	{
		kik_conf_delete( conf) ;

		return  0 ;
	}

	if( ( value = kik_conf_get_value( conf , "font_size_range")))
	{
		u_int  min_font_size ;
		u_int  max_font_size ;

		if( get_font_size_range( &min_font_size , &max_font_size , value))
		{
			x_set_font_size_range( min_font_size , max_font_size) ;
		}
		else
		{
			kik_msg_printf( invalid_msg , "font_size_range" , value) ;
		}
	}

	is_genuine_daemon = 0 ;

#ifndef  USE_WIN32API
	if( ( value = kik_conf_get_value( conf , "daemon_mode")))
	{
		int  ret ;

		ret = 0 ;

	#ifndef  USE_WIN32GUI
		/* 'genuine' is not supported in win32. */
		if( strcmp( value , "genuine") == 0)
		{
			if( ( ret = daemon_init()) > 0)
			{
				is_genuine_daemon = 1 ;
			}
		}
		else
	#endif
		if( strcmp( value , "blend") == 0)
		{
			ret = daemon_init() ;
		}
	#if  0
		else if( strcmp( value , "none") == 0)
		{
		}
	#endif

		if( ret == -1)
		{
			execvp( "mlclient" , orig_argv) ;
		}
	}
#endif

#if ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
	use_xim = 1 ;

	if( ( value = kik_conf_get_value( conf , "use_xim")))
	{
		if( strcmp( value , "false") == 0)
		{
			use_xim = 0 ;
		}
	}

	x_xim_init( use_xim) ;
#endif

#if ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
	if( ( value = kik_conf_get_value( conf , "compose_dec_special_font")))
	{
		if( strcmp( value , "true") == 0)
		{
			x_compose_dec_special_font() ;
		}
	}
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
	if( ( value = kik_conf_get_value( conf , "use_cp932_ucs_for_xft")) == NULL ||
		strcmp( value , "true") == 0)
	{
		x_use_cp932_ucs_for_xft() ;
	}
#endif

	if( ( value = kik_conf_get_value( conf , "depth")))
	{
		kik_str_to_uint( &depth , value) ;
	}
	else
	{
		depth = 0 ;
	}
	
	max_screens_multiple = 1 ;

	if( ( value = kik_conf_get_value( conf , "max_ptys")))
	{
		u_int  max_ptys ;
		
		if( kik_str_to_uint( &max_ptys , value))
		{
			u_int  multiple ;

			multiple = max_ptys / 32 ;
			
			if( multiple * 32 != max_ptys)
			{
				kik_msg_printf( "max_ptys %s is not multiple of 32.\n" , value) ;
			}

			if( multiple > 1)
			{
				max_screens_multiple = multiple ;
			}
		}
		else
		{
			kik_msg_printf( invalid_msg , "max_ptys" , value) ;
		}
	}

	num_of_startup_screens = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "startup_screens")))
	{
		u_int  n ;
		
		if( ! kik_str_to_uint( &n , value) || ( ! is_genuine_daemon && n == 0))
		{
			kik_msg_printf( invalid_msg , "startup_screens" , value) ;
		}
		else
		{
			num_of_startup_screens = n ;
		}
	}

#ifdef  USE_LIBSSH2
	if( ( value = kik_conf_get_value( conf , "ssh_keepalive_interval")))
	{
		u_int  interval ;

		if( kik_str_to_uint( &interval , value) && interval > 0)
		{
			ml_pty_ssh_set_keepalive_interval( interval) ; 
			x_event_source_add_fd( -1 , ssh_keepalive) ;
		}
	}
#endif

#ifdef  USE_FRAMEBUFFER
#if  defined(__NetBSD__) && ! defined(USE_GRF)
	if( ( value = kik_conf_get_value( conf , "wskbd_repeat_1")))
	{
		extern int  wskbd_repeat_1 ;

		kik_str_to_int( &wskbd_repeat_1 , value) ;
	}

	if( ( value = kik_conf_get_value( conf , "wskbd_repeat_N")))
	{
		extern int  wskbd_repeat_N ;

		kik_str_to_int( &wskbd_repeat_N , value) ;
	}
#endif
#if  defined(__OpenBSD__) || defined(__NetBSD__)
	if( ( value = kik_conf_get_value( conf , "fb_resolution")))
	{
		extern u_int  fb_width ;
		extern u_int  fb_height ;
		extern u_int  fb_depth ;

		sscanf( value , "%dx%dx%d" , &fb_width , &fb_height , &fb_depth) ;
	}
#endif
#endif

	if( ( value = kik_conf_get_value( conf , "mod_meta_prefix")))
	{
		x_set_mod_meta_prefix( kik_str_unescape( value)) ;
	}

	x_main_config_init( &main_config , conf , argc , argv) ;

	if( ( value = kik_conf_get_value( conf , "default_font")))
	{
		x_customize_font_file(
			main_config.type_engine == TYPE_XCORE ? "font" : "aafont" ,
			"DEFAULT" , value , 0) ;
	}

	if( ( value = kik_conf_get_value( conf , "use_point_size")))
	{
		if( strcmp( value , "true") == 0)
		{
			x_font_use_point_size_for_fc( 1) ;
		}
	}

	kik_conf_delete( conf) ;

	x_screen_manager_init( "MLTERM=" VERSION , depth , max_screens_multiple ,
		num_of_startup_screens , &main_config) ;

	x_event_source_init() ;

	kik_alloca_garbage_collect() ;

#ifndef  USE_WIN32API
	signal( SIGHUP , sig_fatal) ;
	signal( SIGINT , sig_fatal) ;
	signal( SIGQUIT , sig_fatal) ;
	signal( SIGTERM , sig_fatal) ;
	signal( SIGPIPE , SIG_IGN) ;
#endif

	return  1 ;
}

int
main_loop_final(void)
{
	x_event_source_final() ;

	x_screen_manager_final() ;

	daemon_final() ;

	ml_free_word_separators() ;

	kik_set_msg_log_file_name( NULL) ;

#if ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
	x_xim_final() ;
#endif

	kik_sig_child_final() ;

	kik_locale_final() ;

	return  1 ;
}

int
main_loop_start(void)
{
	if( x_screen_manager_startup() == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " open_screen_intern() failed.\n") ;
	#endif

		if( ! is_genuine_daemon)
		{
			kik_msg_printf( "Unable to open screen.\n") ;

			return  0 ;
		}
	}

	while( 1)
	{
		kik_alloca_begin_stack_frame() ;

		if( ! x_event_source_process() && ! is_genuine_daemon)
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Exiting...\n") ;
		#endif

			break ;
		}

		kik_alloca_end_stack_frame() ;
	}

	return  1 ;
}
