/*
 *	$Id$
 */

#include  "x_term_manager.h"

#include  <stdio.h>		/* sprintf */
#include  <string.h>		/* memset/memcpy */
#include  <pwd.h>		/* getpwuid */
#include  <sys/time.h>		/* timeval */
#include  <unistd.h>		/* getpid/select/unlink */
#include  <sys/wait.h>		/* wait */
#include  <signal.h>		/* kill */
#include  <stdlib.h>		/* getenv */
#include  <errno.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* kik_str_sep/kik_str_to_int/kik_str_alloca_dup/strdup */
#include  <kiklib/kik_path.h>	/* kik_basename */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */
#include  <kiklib/kik_mem.h>	/* alloca/kik_alloca_garbage_collect/malloc/free */
#include  <kiklib/kik_conf.h>
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_locale.h>	/* kik_get_codeset */
#include  <kiklib/kik_net.h>	/* socket/bind/listen/sockaddr_un */
#include  <kiklib/kik_types.h>	/* u_int */
#include  <kiklib/kik_sig_child.h>

#include  "version.h"
#include  "x_xim.h"
#include  "x_term.h"
#include  "x_sb_screen.h"
#include  "x_termcap.h"


#define  MAX_TERMS  32		/* dead_mask is 32 bit */


typedef struct main_config
{
	int  x ;
	int  y ;
	int  geom_hint ;
	u_int  cols ;
	u_int  rows ;
	u_int  screen_width_ratio ;
	u_int  screen_height_ratio ;
	u_int  font_size ;
	u_int  num_of_log_lines ;
	u_int  line_space ;
	u_int  tab_size ;
	ml_iscii_lang_type_t  iscii_lang_type ;
	x_mod_meta_mode_t  mod_meta_mode ;
	x_bel_mode_t  bel_mode ;
	x_sb_mode_t  sb_mode ;
	u_int  col_size_a ;
	ml_char_encoding_t  encoding ;
	x_font_present_t  font_present ;
	ml_vertical_mode_t  vertical_mode ;
	ml_bs_mode_t  bs_mode ;

	char *  disp_name ;
	char *  app_name ;
	char *  title ;
	char *  icon_name ;
	char *  term_type ;
	char *  scrollbar_view_name ;
	char *  pic_file_path ;
	char *  conf_menu_path ;
	char *  fg_color ;
	char *  bg_color ;
	char *  cursor_fg_color ;
	char *  cursor_bg_color ;
	char *  sb_fg_color ;
	char *  sb_bg_color ;
	char *  mod_meta_key ;
	char *  icon_path ;
	char *  cmd_path ;
	char **  cmd_argv ;
	
	u_int8_t  step_in_changing_font_size ;
	u_int16_t  brightness ;
	u_int16_t  contrast ;
	u_int16_t  gamma ;
	u_int8_t  fade_ratio ;
	int8_t  use_scrollbar ;
	int8_t  use_login_shell ;
	int8_t  xim_open_in_startup ;
	int8_t  use_bidi ;
	int8_t  big5_buggy ;
	int8_t  iso88591_font_for_usascii ;
	int8_t  not_use_unicode_font ;
	int8_t  only_use_unicode_font ;
	int8_t  receive_string_via_ucs ;
	int8_t  use_transbg ;
	int8_t  use_char_combining ;
	int8_t  use_multi_col_char ;
	int8_t  use_vertical_cursor ;
	int8_t  use_extended_scroll_shortcut ;
	int8_t  use_dynamic_comb ;

} main_config_t ;


/* --- static variables --- */

static x_term_t *  xterms ;
static u_int  num_of_xterms ;
static u_int32_t  dead_mask ;
static u_int  num_of_startup_xterms ;

static x_system_event_listener_t  system_listener ;

static char *  version ;

static main_config_t  main_config ;

static x_font_custom_t  normal_font_custom ;
static x_font_custom_t  v_font_custom ;
static x_font_custom_t  t_font_custom ;
#ifdef  ANTI_ALIAS
static x_font_custom_t  aa_font_custom ;
static x_font_custom_t  vaa_font_custom ;
static x_font_custom_t  taa_font_custom ;
#endif
static x_color_custom_t  color_custom ;
static x_shortcut_t  shortcut ;
static x_termcap_t  termcap ;

static int  sock_fd ;
static char *  un_file ;
static int8_t  is_genuine_daemon ;


/* --- static functions --- */

static int
get_font_size_range(
	u_int *  min ,
	u_int *  max ,
	char *  str
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

	if( ( p = kik_str_sep( &str_p , "-")) == NULL)
	{
		return  0 ;
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

static int
open_pty_intern(
	ml_term_t *  term ,
	char *  cmd_path ,
	char **  cmd_argv ,
	char *  display ,
	Window  window ,
	char *  term_type ,
	int  use_login_shell
	)
{
	char *  env[5] ;	/* MLTERM,TERM,WINDOWID,DISPLAY,NULL */
	char **  env_p ;
	char  wid_env[9 + DIGIT_STR_LEN(Window) + 1] ;	/* "WINDOWID="(9) + [32bit digit] + NULL(1) */
	char *  disp_env ;
	char *  term_env ;
	
	env_p = env ;

	if( version)
	{
		*(env_p ++) = version ;
	}
	
	sprintf( wid_env , "WINDOWID=%ld" , window) ;
	*(env_p ++) = wid_env ;
	
	/* "DISPLAY="(8) + NULL(1) */
	if( ( disp_env = alloca( 8 + strlen( display) + 1)))
	{
		sprintf( disp_env , "DISPLAY=%s" , display) ;
		*(env_p ++) = disp_env ;
	}

	/* "TERM="(5) + NULL(1) */
	if( ( term_env = alloca( 5 + strlen( term_type) + 1)))
	{
		sprintf( term_env , "TERM=%s" , term_type) ;
		*(env_p ++) = term_env ;
	}

	/* NULL terminator */
	*env_p = NULL ;

	if( ! cmd_path)
	{
		struct passwd *  pw ;

		/*
		 * SHELL env var -> /etc/passwd -> /bin/sh
		 */
		if( ( cmd_path = getenv( "SHELL")) == NULL || *cmd_path == '\0')
		{
			if( ( pw = getpwuid(getuid())) == NULL ||
				*( cmd_path = pw->pw_shell) == '\0')
			{
				cmd_path = "/bin/sh" ;
			}
		}
	}

	if( ! cmd_argv)
	{
		char *  cmd_file ;
		
		if( ( cmd_argv = alloca( sizeof( char*) * 2)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
		#endif

			return  0 ;
		}

		cmd_file = kik_basename( cmd_path) ;

		/* 2 = `-' and NULL */
		if( ( cmd_argv[0] = alloca( strlen( cmd_file) + 2)) == NULL)
		{
			return  0 ;
		}

		if( use_login_shell)
		{
			sprintf( cmd_argv[0] , "-%s" , cmd_file) ;
		}
		else
		{
			strcpy( cmd_argv[0] , cmd_file) ;
		}

		cmd_argv[1] = NULL ;
	}

	return  ml_term_open_pty( term , cmd_path , cmd_argv , env , display) ;
}
	
static int
open_term(void)
{
	ml_term_t *  term ;
	x_display_t *  disp ;
	x_screen_t *  screen ;
	x_sb_screen_t *  sb_screen ;
	x_font_manager_t *  font_man ;
	x_color_manager_t *  color_man ;
	x_window_t *  root ;
	mkf_charset_t  usascii_font_cs ;
	int  usascii_font_cs_changable ;
	x_termcap_entry_t *  tent ;
	void *  p ;
	
	/*
	 * these are dynamically allocated.
	 */
	disp = NULL ;
	font_man = NULL ;
	color_man = NULL ;
	screen = NULL ;
	sb_screen = NULL ;
	term = NULL ;

	if( MAX_TERMS <= num_of_xterms)
	{
		return  0 ;
	}

	tent = x_termcap_get_entry( &termcap , main_config.term_type) ;

	if( ( term = ml_pop_term( main_config.cols , main_config.rows ,
			main_config.tab_size , main_config.num_of_log_lines ,
			main_config.encoding , main_config.not_use_unicode_font ,
			main_config.only_use_unicode_font , main_config.col_size_a ,
			main_config.use_char_combining , main_config.use_multi_col_char ,
			x_termcap_get_bool_field( tent , ML_BCE) , main_config.bs_mode)) == NULL)
	{
		return  0 ;
	}
	
	if( ( disp = x_display_open( main_config.disp_name)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_display_open failed.\n") ;
	#endif
	
		return  0 ;
	}

	if( main_config.not_use_unicode_font || main_config.iso88591_font_for_usascii)
	{
		usascii_font_cs = x_get_usascii_font_cs( ML_ISO8859_1) ;
		usascii_font_cs_changable = 0 ;
	}
	else if( main_config.only_use_unicode_font)
	{
		usascii_font_cs = x_get_usascii_font_cs( ML_UTF8) ;
		usascii_font_cs_changable = 0 ;
	}
	else
	{
		usascii_font_cs = x_get_usascii_font_cs( ml_term_get_encoding( term)) ;
		usascii_font_cs_changable = 1 ;
	}
	
	if( ( font_man = x_font_manager_new( disp->display ,
		&normal_font_custom , &v_font_custom , &t_font_custom ,
	#ifdef  ANTI_ALIAS
		&aa_font_custom , &vaa_font_custom , &taa_font_custom ,
	#else
		NULL , NULL , NULL ,
	#endif
		main_config.font_present , main_config.font_size ,
		usascii_font_cs , usascii_font_cs_changable ,
		main_config.use_multi_col_char ,
		main_config.step_in_changing_font_size)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_font_manager_new() failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( ( color_man = x_color_manager_new( disp->display ,
				DefaultScreen( disp->display) , &color_custom ,
				main_config.fg_color , main_config.bg_color ,
				main_config.cursor_fg_color , main_config.cursor_bg_color)) == NULL)
	{
		goto  error ;
	}

	if( ( screen = x_screen_new( term , font_man , color_man , tent ,
			main_config.brightness , main_config.contrast , main_config.gamma ,
			main_config.fade_ratio , &shortcut ,
			main_config.screen_width_ratio , main_config.screen_height_ratio ,
			main_config.xim_open_in_startup , main_config.mod_meta_key ,
			main_config.mod_meta_mode , main_config.bel_mode ,
			main_config.receive_string_via_ucs , main_config.pic_file_path ,
			main_config.use_transbg , main_config.use_bidi ,
			main_config.vertical_mode , main_config.use_vertical_cursor ,
			main_config.big5_buggy , main_config.conf_menu_path ,
			main_config.iscii_lang_type , main_config.use_extended_scroll_shortcut ,
			main_config.use_dynamic_comb , main_config.line_space)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_screen_new() failed.\n") ;
	#endif

		goto  error ;
	}

	if( ! x_set_system_listener( screen , &system_listener))
	{
		goto  error ;
	}

	if( main_config.use_scrollbar)
	{
		if( ( sb_screen = x_sb_screen_new( screen ,
					main_config.scrollbar_view_name ,
					main_config.sb_fg_color , main_config.sb_bg_color ,
					main_config.sb_mode)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " x_sb_screen_new() failed.\n") ;
		#endif

			goto  error ;
		}

		root = &sb_screen->window ;
	}
	else
	{
		sb_screen = NULL ;
		root = &screen->window ;
	}

	if( ! x_window_manager_show_root( &disp->win_man , root ,
		main_config.x , main_config.y , main_config.geom_hint))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_window_manager_show_root() failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( main_config.app_name)
	{
		x_set_window_name( &screen->window , main_config.app_name) ;
		x_set_icon_name( &screen->window , main_config.icon_name) ;
	}
	else
	{
		if( main_config.title)
		{
			x_set_window_name( &screen->window , main_config.title) ;
		}
		
		if( main_config.icon_name)
		{
			x_set_icon_name( &screen->window , main_config.icon_name) ;
		}
	}

	if( main_config.icon_path)
	{
		x_window_set_icon( &screen->window , main_config.icon_path);
	}

	if( ! open_pty_intern( term , main_config.cmd_path , main_config.cmd_argv ,
		XDisplayString( disp->display) , root->my_window ,
		main_config.term_type , main_config.use_login_shell))
	{
		goto  error ;
	}

	if( ( p = realloc( xterms , sizeof( x_term_t) * (num_of_xterms + 1))) == NULL)
	{
		goto  error ;
	}

	xterms = p ;
	
	if( ! x_term_init( &xterms[num_of_xterms] ,
		disp , root , font_man , color_man , term))
	{
		goto  error ;
	}

	num_of_xterms ++ ;
	
	return  1 ;
	
error:
	if( disp)
	{
		x_display_close( disp) ;
	}
	
	if( font_man)
	{
		x_font_manager_delete( font_man) ;
	}

	if( color_man)
	{
		x_color_manager_delete( color_man) ;
	}

	if( screen)
	{
		x_screen_delete( screen) ;
	}

	if( sb_screen)
	{
		x_sb_screen_delete( sb_screen) ;
	}

	if( term)
	{
		ml_put_back_term( term) ;
	}

	return  0 ;
}


/*
 * callbacks of x_system_event_listener_t
 */
 
/*
 * EXIT_PROGRAM shortcut calls this at last.
 * this is for debugging.
 */
static void
__exit(
	void *  p ,
	int  status
	)
{
#ifdef  KIK_DEBUG
	x_term_manager_final() ;
	
	kik_locale_final() ;

	kik_alloca_garbage_collect() ;

	kik_msg_printf( "reporting unfreed memories --->\n") ;
	kik_mem_free_all() ;
#endif
	
	if( un_file)
	{
		unlink( un_file) ;
	}
	
	exit(status) ;
}

static void
open_pty(
	void *  p ,
	x_screen_t *  screen
	)
{
	x_window_t *  root ;
	int  count ;
	
	root = x_get_root_window( &screen->window) ;
	
	for( count = 0 ; count < num_of_xterms ; count ++)
	{
		if( root == xterms[count].root_window)
		{
			ml_term_t *  new ;
			
			if( ( new = ml_pop_term( main_config.cols , main_config.rows ,
				main_config.tab_size , main_config.num_of_log_lines ,
				main_config.encoding , main_config.not_use_unicode_font ,
				main_config.only_use_unicode_font , main_config.col_size_a ,
				main_config.use_char_combining , main_config.use_multi_col_char ,
				0 , main_config.bs_mode))
				== NULL)
			{
				return  ;
			}
			
			if( ! open_pty_intern( new , main_config.cmd_path , main_config.cmd_argv ,
				XDisplayString( screen->window.display) ,
				x_get_root_window( &screen->window)->my_window ,
				main_config.term_type , main_config.use_login_shell))
			{
				return ;
			}
			
			ml_put_back_term( xterms[count].term) ;
			x_screen_detach( screen) ;
			
			xterms[count].term = new ;
			x_screen_attach( screen , new) ;

			return ;
		}
	}
}

static void
pty_closed(
	void *  p ,
	x_screen_t *  screen
	)
{
	int  count ;
	x_window_t *  root ;

	root = x_get_root_window( &screen->window) ;

	for( count = num_of_xterms - 1 ; count >= 0 ; count --)
	{
		if( root == xterms[count].root_window)
		{
			/* ml_term_t is already deleted */
			xterms[count].term = NULL ;
			x_term_final( &xterms[count]) ;

			xterms[count] = xterms[--num_of_xterms] ;

			return ;
		}
	}
}

static void
open_screen(
	void *  p
	)
{
	if( ! open_term())
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " open_term failed.\n") ;
	#endif
	}
}
	
static void
close_screen(
	void *  p ,
	x_window_t *  root_window
	)
{
	int  count ;
	
	for( count = 0 ; count < num_of_xterms ; count ++)
	{
		if( root_window == xterms[count].root_window)
		{
			dead_mask |= (1 << count) ;
			
			return ;
		}
	}
}


/*
 * signal handlers.
 */
 
static void
sig_fatal( int  sig)
{
#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG "signal %d is received\n" , sig) ;
#endif

	if( un_file)
	{
		unlink( un_file) ;
	}

	/* reset */
	signal( sig , SIG_DFL) ;
	
	kill( getpid() , sig) ;
}


static int
start_daemon(void)
{
	pid_t  pid ;
	int  sock_fd ;
	struct sockaddr_un  servaddr ;
	char * const path = servaddr.sun_path ;

	memset( &servaddr , 0 , sizeof( servaddr)) ;
	servaddr.sun_family = AF_LOCAL ;
	kik_snprintf( path , sizeof( servaddr.sun_path) - 1 , "/tmp/.mlterm-%d.unix" , getuid()) ;

	if( ( sock_fd = socket( AF_LOCAL , SOCK_STREAM , 0)) < 0)
	{
		return  -1 ;
	}

	while( bind( sock_fd , (struct sockaddr *) &servaddr , sizeof( servaddr)) < 0)
	{
		if( errno == EADDRINUSE)
		{
			if( connect( sock_fd , (struct sockaddr*) &servaddr , sizeof( servaddr)) == 0)
			{
				kik_msg_printf( "daemon is already running.\n") ;
				return  -1 ;
			}

			kik_msg_printf( "removing stale lock file %s.\n" , path) ;
			if( unlink( path) == 0) continue ;
		}

		close( sock_fd) ;

		kik_msg_printf( "failed to lock file %s: %s\n" , path , strerror(errno)) ;

		return  -1 ;
	}

	pid = fork() ;

	if( pid == -1)
	{
		return  -1 ;
	}

	if( pid != 0)
	{
		exit(0) ;
	}
	
	/*
	 * child
	 */

	/*
	 * This process becomes a session leader and purged from control terminal.
	 */
	setsid() ;

	/*
	 * SIGHUP signal when the child process exits must not be sent to
	 * the grandchild process.
	 */
	signal( SIGHUP , SIG_IGN) ;

	pid = fork() ;

	if( pid == -1)
	{
		exit(1) ;
	}

	if( pid != 0)
	{
		exit(0) ;
	}

	/*
	 * grandchild
	 */

	if( listen( sock_fd , 1024) < 0)
	{
		close( sock_fd) ;
		unlink( path) ;
		
		return  -1 ;
	}

	un_file = strdup( path) ;

	return  sock_fd ;
}

static kik_conf_t *
get_min_conf(
	int  argc ,
	char **  argv
	)
{
	kik_conf_t *  conf ;
	char *  rcpath ;
	
	if( ( conf = kik_conf_new( "mlterm" ,
		MAJOR_VERSION , MINOR_VERSION , REVISION , PATCH_LEVEL)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " kik_conf_new() failed.\n") ;
	#endif
	
		return  NULL ;
	}

	/*
	 * XXX
	 * "mlterm/core" is for backward compatibility with 1.9.44
	 */
	 
	if( ( rcpath = kik_get_sys_rc_path( "mlterm/core")))
	{
		kik_conf_read( conf , rcpath) ;

		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( "mlterm/core")))
	{
		kik_conf_read( conf , rcpath) ;

		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_sys_rc_path( "mlterm/main")))
	{
		kik_conf_read( conf , rcpath) ;

		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( "mlterm/main")))
	{
		kik_conf_read( conf , rcpath) ;

		free( rcpath) ;
	}

	kik_conf_add_opt( conf , '1' , "wscr" , 0 , "screen_width_ratio" ,
		"screen width in percent against font width [default = 100]") ;
	kik_conf_add_opt( conf , '2' , "hscr" , 0 , "screen_height_ratio" ,
		"screen height in percent against font height [100]") ;
#if defined(USE_IMLIB) || defined(USE_GDK_PIXBUF)
	kik_conf_add_opt( conf , '3' , "contrast" , 0 , "contrast" ,
		"contrast of background image in percent [100]") ;
	kik_conf_add_opt( conf , '4' , "gamma" , 0 , "gamma" ,
		"gamma of background image in percent [100]") ;
#endif
	kik_conf_add_opt( conf , '5' , "big5bug" , 1 , "big5_buggy" ,
		"manage buggy Big5 CTEXT in XFree86 4.1 or earlier [false]") ;
	kik_conf_add_opt( conf , '6' , "stbs" , 1 , "static_backscroll_mode" ,
		"screen is static under backscroll mode [false]") ;
	kik_conf_add_opt( conf , '7' , "bel" , 0 , "bel_mode" , 
		"bel (0x07) mode [none/sound/visual, default = none]") ;
	kik_conf_add_opt( conf , '8' , "88591" , 1 , "iso88591_font_for_usascii" ,
		"use ISO-8859-1 font for ASCII part of any encoding [false]") ;
	kik_conf_add_opt( conf , '9' , "crfg" , 0 , "cursor_fg_color" ,
		"cursor foreground color") ;
	kik_conf_add_opt( conf , '0' , "crbg" , 0 , "cursor_bg_color" ,
		"cursor background color") ;
#ifdef  ANTI_ALIAS
	kik_conf_add_opt( conf , 'A' , "aa" , 1 , "use_anti_alias" , 
		"use anti-alias font by using Xft [false]") ;
#endif
	kik_conf_add_opt( conf , 'B' , "sbbg" , 0 , "sb_bg_color" , 
		"scrollbar background color") ;
#ifdef  USE_IND
	kik_conf_add_opt( conf , 'C' , "iscii" , 0 , "iscii_lang" , 
		"language to be used in ISCII encoding") ;
#endif
#ifdef  USE_FRIBIDI
	kik_conf_add_opt( conf , 'D' , "bi" , 1 , "use_bidi" , 
		"use bidi (bi-directional text) [false]") ;
#endif
	kik_conf_add_opt( conf , 'E' , "km" , 0 , "ENCODING" , 
		"character encoding [AUTO/ISO-8859-*/EUC-*/UTF-8/...]") ;
	kik_conf_add_opt( conf , 'F' , "sbfg" , 0 , "sb_fg_color" , 
		"scrollbar foreground color") ;
	kik_conf_add_opt( conf , 'G' , "vertical" , 0 , "vertical_mode" ,
		"vertical mode [none/cjk/mongol]") ;
#if defined(USE_IMLIB) || defined(USE_GDK_PIXBUF)
	kik_conf_add_opt( conf , 'H' , "bright" , 0 , "brightness" ,
		"brightness of background image in percent [100]") ;
#endif
	kik_conf_add_opt( conf , 'I' , "icon" , 0 , "icon_name" , 
		"icon name") ;
	kik_conf_add_opt( conf , 'J' , "dyncomb" , 1 , "use_dynamic_comb" ,
		"use dynamic combining [false]") ;
	kik_conf_add_opt( conf , 'K' , "metakey" , 0 , "mod_meta_key" ,
		"specify meta key [none]") ;
	kik_conf_add_opt( conf , 'L' , "ls" , 1 , "use_login_shell" , 
		"turn on login shell [false]") ;
	kik_conf_add_opt( conf , 'M' , "menu" , 0 , "conf_menu_path" ,
		"command path of mlconfig (GUI configurator)") ;
	kik_conf_add_opt( conf , 'N' , "name" , 0 , "app_name" , 
		"application name") ;
	kik_conf_add_opt( conf , 'O' , "sbmod" , 0 , "scrollbar_mode" ,
		"scrollbar mode [none/left/right]") ;
	kik_conf_add_opt( conf , 'Q' , "vcur" , 1 , "use_vertical_cursor" ,
		"rearrange cursor key for vertical mode [false]") ;
	kik_conf_add_opt( conf , 'S' , "sbview" , 0 , "scrollbar_view_name" , 
		"scrollbar view name [simple/sample/athena/motif/...]") ;
	kik_conf_add_opt( conf , 'T' , "title" , 0 , "title" , 
		"title name") ;
	kik_conf_add_opt( conf , 'U' , "viaucs" , 1 , "receive_string_via_ucs" ,
		"process received (pasted) strings via Unicode [false]") ;
	kik_conf_add_opt( conf , 'V' , "varwidth" , 1 , "use_variable_column_width" ,
		"variable column width (for proportional/ISCII) [false]") ;
	kik_conf_add_opt( conf , 'X' , "openim" , 1 , "xim_open_in_startup" , 
		"open XIM (X Input Method) in starting up [true]") ;
	kik_conf_add_opt( conf , 'Z' , "multicol" , 1 , "use_multi_column_char" ,
		"fullwidth character occupies two logical columns [true]") ;
	kik_conf_add_opt( conf , 'a' , "ac" , 0 , "col_size_of_width_a" ,
		"columns for Unicode \"EastAsianAmbiguous\" character [1]") ;
	kik_conf_add_opt( conf , 'b' , "bg" , 0 , "bg_color" , 
		"background color") ;
	kik_conf_add_opt( conf , 'd' , "display" , 0 , "display" , 
		"X server to connect") ;
	kik_conf_add_opt( conf , 'f' , "fg" , 0 , "fg_color" , 
		"foreground color") ;
	kik_conf_add_opt( conf , 'g' , "geometry" , 0 , "geometry" , 
		"size (in characters) and position [80x24]") ;
	kik_conf_add_opt( conf , 'k' , "meta" , 0 , "mod_meta_mode" , 
		"mode in pressing meta key [none/esc/8bit]") ;
	kik_conf_add_opt( conf , 'l' , "sl" , 0 , "logsize" , 
		"number of backlog (scrolled lines to save) [128]") ;
	kik_conf_add_opt( conf , 'm' , "comb" , 1 , "use_combining" , 
		"use combining characters [true]") ;
	kik_conf_add_opt( conf , 'n' , "noucsfont" , 1 , "not_use_unicode_font" ,
		"use non-Unicode fonts even in UTF-8 mode [false]") ;
	kik_conf_add_opt( conf , 'o' , "lsp" , 0 , "line_space" ,
		"extra space between lines in pixels [0]") ;
#if defined(USE_IMLIB) || defined(USE_GDK_PIXBUF)
	kik_conf_add_opt( conf , 'p' , "pic" , 0 , "wall_picture" , 
		"path for wallpaper (background) image") ;
#endif
	kik_conf_add_opt( conf , 'q' , "extkey" , 1 , "use_extended_scroll_shortcut" ,
		"use extended scroll shortcut keys [false]") ;
	kik_conf_add_opt( conf , 'r' , "fade" , 0 , "fade_ratio" , 
		"fade ratio in percent when window unfocued [100]") ;
	kik_conf_add_opt( conf , 's' , "sb" , 1 , "use_scrollbar" , 
		"use scrollbar [false]") ;
	kik_conf_add_opt( conf , 't' , "transbg" , 1 , "use_transbg" , 
		"use transparent background [false]") ;
	kik_conf_add_opt( conf , 'u' , "onlyucsfont" , 1 , "only_use_unicode_font" ,
		"use a Unicode font even in non-UTF-8 modes [false]") ;
	kik_conf_add_opt( conf , 'w' , "fontsize" , 0 , "fontsize" , 
		"font size in pixels [16]") ;
	kik_conf_add_opt( conf , 'x' , "tw" , 0 , "tabsize" , 
		"tab width in columns [8]") ;
	kik_conf_add_opt( conf , 'y' , "term" , 0 , "termtype" , 
		"terminal type for TERM variable [xterm]") ;
	kik_conf_add_opt( conf , 'z' ,  "largesmall" , 0 , "step_in_changing_font_size" ,
		"step in changing font size in GUI configurator [1]") ;

	kik_conf_set_end_opt( conf , 'e' , NULL , "exec_cmd" , 
		"execute external command") ;

	return  conf ;	
}

static int
config_init(
	kik_conf_t *  conf ,
	int  argc ,
	char **  argv
	)
{
	char *  value ;
	
	if( ( value = kik_conf_get_value( conf , "display")) == NULL)
	{
		value = "" ;
	}

	if( ( main_config.disp_name = strdup( value)) == NULL)
	{
		return  0 ;
	}
	
	if( ( value = kik_conf_get_value( conf , "fontsize")) == NULL)
	{
		main_config.font_size = 16 ;
	}
	else if( ! kik_str_to_uint( &main_config.font_size , value))
	{
		kik_msg_printf( "font size %s is not valid.\n" , value) ;

		/* default value is used. */
		main_config.font_size = 16 ;
	}

	if( main_config.font_size > normal_font_custom.max_font_size)
	{
		kik_msg_printf( "font size %d is too large. %d is used.\n" ,
			main_config.font_size , normal_font_custom.max_font_size) ;
		
		main_config.font_size = normal_font_custom.max_font_size ;
	}
	else if( main_config.font_size < normal_font_custom.min_font_size)
	{
		kik_msg_printf( "font size %d is too small. %d is used.\n" ,
			main_config.font_size , normal_font_custom.min_font_size) ;
			
		main_config.font_size = normal_font_custom.min_font_size ;
	}

	main_config.app_name = NULL ;

	if( ( value = kik_conf_get_value( conf , "app_name")))
	{
		main_config.app_name = strdup( value) ;
	}

	main_config.title = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "title")))
	{
		main_config.title = strdup( value) ;
	}

	main_config.icon_name = NULL ;

	if( ( value = kik_conf_get_value( conf , "icon_name")))
	{
		main_config.icon_name = strdup( value) ;
	}
	
	main_config.conf_menu_path = NULL ;

	if( ( value = kik_conf_get_value( conf , "conf_menu_path")))
	{
		main_config.conf_menu_path = strdup( value) ;
	}

	/* use default value */
	main_config.scrollbar_view_name = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "scrollbar_view_name")))
	{
		main_config.scrollbar_view_name = strdup( value) ;
	}

	main_config.use_char_combining = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "use_combining")))
	{
		if( strcmp( value , "false") == 0)
		{
			main_config.use_char_combining = 0 ;
		}
	}

	main_config.use_dynamic_comb = 0 ;
	if( ( value = kik_conf_get_value( conf , "use_dynamic_comb")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config.use_dynamic_comb = 1 ;
		}
	}

	main_config.font_present = 0 ;

	if( ( value = kik_conf_get_value( conf , "use_variable_column_width")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config.font_present |= FONT_VAR_WIDTH ;
		}
	}

	main_config.step_in_changing_font_size = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "step_in_changing_font_size")))
	{
		u_int  size ;
		
		if( kik_str_to_uint( &size , value))
		{
			main_config.step_in_changing_font_size = size ;
		}
		else
		{
			kik_msg_printf( "step in changing font size %s is not valid.\n" , value) ;
		}
	}

#ifdef  ANTI_ALIAS	
	if( ( value = kik_conf_get_value( conf , "use_anti_alias")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config.font_present |= FONT_AA ;
		}
	}
#endif

	main_config.vertical_mode = 0 ;
	
	if( ( value = kik_conf_get_value( conf , "vertical_mode")))
	{
		if( ( main_config.vertical_mode = ml_get_vertical_mode( value)))
		{
			/*
			 * vertical font is automatically used under vertical mode.
			 * similler processing is done in x_screen.c:change_vertical_mode.
			 */
			main_config.font_present |= FONT_VERTICAL ;
			main_config.font_present &= ~FONT_VAR_WIDTH ;
		}
	}

	main_config.fg_color = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "fg_color")))
	{
		main_config.fg_color = strdup( value) ;
	}

	main_config.bg_color = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "bg_color")))
	{
		main_config.bg_color = strdup( value) ;
	}

	main_config.cursor_fg_color = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "cursor_fg_color")))
	{
		main_config.cursor_fg_color = strdup( value) ;
	}

	main_config.cursor_bg_color = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "cursor_bg_color")))
	{
		main_config.cursor_bg_color = strdup( value) ;
	}
	
	main_config.sb_fg_color = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "sb_fg_color")))
	{
		main_config.sb_fg_color = strdup( value) ;
	}

	main_config.sb_bg_color = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "sb_bg_color")))
	{
		main_config.sb_bg_color = strdup( value) ;
	}
	
	if( ( value = kik_conf_get_value( conf , "termtype")))
	{
		main_config.term_type = strdup( value) ;
	}
	else
	{
		main_config.term_type = strdup( "xterm") ;
	}

	main_config.x = 0 ;
	main_config.y = 0 ;
	main_config.cols = 80 ;
	main_config.rows = 24 ;
	if( ( value = kik_conf_get_value( conf , "geometry")))
	{
		/* For each value not found, the argument is left unchanged.(see man XParseGeometry(3)) */
		main_config.geom_hint = XParseGeometry( value , &main_config.x , &main_config.y ,
						&main_config.cols , &main_config.rows) ;

		if( main_config.cols == 0 || main_config.rows == 0)
		{
			kik_msg_printf( "geometry option %s is illegal.\n" , value) ;
			
			main_config.cols = 80 ;
			main_config.rows = 24 ;
		}
	}
	else
	{
		main_config.geom_hint = 0 ;
	}

	main_config.screen_width_ratio = 100 ;
	
	if( ( value = kik_conf_get_value( conf , "screen_width_ratio")))
	{
		u_int  ratio ;

		if( kik_str_to_uint( &ratio , value))
		{
			main_config.screen_width_ratio = ratio ;
		}
	}

	main_config.screen_height_ratio = 100 ;
	
	if( ( value = kik_conf_get_value( conf , "screen_height_ratio")))
	{
		u_int  ratio ;

		if( kik_str_to_uint( &ratio , value))
		{
			main_config.screen_height_ratio = ratio ;
		}
	}
	
	main_config.use_multi_col_char = 1 ;

	if( ( value = kik_conf_get_value( conf , "use_multi_column_char")))
	{
		if( strcmp( value , "false") == 0)
		{
			main_config.use_multi_col_char = 0 ;
		}
	}

	main_config.line_space = 0 ;

	if( ( value = kik_conf_get_value( conf , "line_space")))
	{
		u_int  size ;

		if( kik_str_to_uint( &size , value))
		{
			main_config.line_space = size ;
		}
		else
		{
			kik_msg_printf( "line space %s is not valid.\n" , value) ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "logsize")) == NULL)
	{
		main_config.num_of_log_lines = 128 ;
	}
	else if( ! kik_str_to_uint( &main_config.num_of_log_lines , value))
	{
		kik_msg_printf( "log size %s is not valid.\n" , value) ;

		/* default value is used. */
		main_config.num_of_log_lines = 128 ;
	}

	if( ( value = kik_conf_get_value( conf , "tabsize")) == NULL)
	{
		/* default value is used. */
		main_config.tab_size = 8 ;
	}
	else if( ! kik_str_to_uint( &main_config.tab_size , value))
	{
		kik_msg_printf( "tab size %s is not valid.\n" , value) ;

		/* default value is used. */
		main_config.tab_size = 8 ;
	}
	
	main_config.use_login_shell = 0 ;
	
	if( ( value = kik_conf_get_value( conf , "use_login_shell")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config.use_login_shell = 1 ;
		}
	}

	main_config.big5_buggy = 0 ;

	if( ( value = kik_conf_get_value( conf , "big5_buggy")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config.big5_buggy = 1 ;
		}
	}

	main_config.use_scrollbar = 1 ;

	if( ( value = kik_conf_get_value( conf , "use_scrollbar")))
	{
		if( strcmp( value , "false") == 0)
		{
			main_config.use_scrollbar = 0 ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "scrollbar_mode")))
	{
		main_config.sb_mode = x_get_sb_mode( value) ;
	}
	else
	{
		main_config.sb_mode = SB_LEFT ;
	}

	main_config.iso88591_font_for_usascii = 0 ;

	if( ( value = kik_conf_get_value( conf , "iso88591_font_for_usascii")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config.iso88591_font_for_usascii = 1 ;
		}
	}

	main_config.not_use_unicode_font = 0 ;

	if( ( value = kik_conf_get_value( conf , "not_use_unicode_font")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config.not_use_unicode_font = 1 ;
		}
	}

	main_config.only_use_unicode_font = 0 ;

	if( ( value = kik_conf_get_value( conf , "only_use_unicode_font")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config.only_use_unicode_font = 1 ;
		}
	}

	if( main_config.only_use_unicode_font && main_config.not_use_unicode_font)
	{
		kik_msg_printf(
			"only_use_unicode_font and not_use_unicode_font options cannot be used "
			"at the same time.\n") ;

		/* default values are used */
		main_config.only_use_unicode_font = 0 ;
		main_config.not_use_unicode_font = 0 ;
	}

	main_config.receive_string_via_ucs = 0 ;

	if( ( value = kik_conf_get_value( conf , "receive_string_via_ucs")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config.receive_string_via_ucs = 1 ;
		}
	}
	
	/* default value is used */
	main_config.col_size_a = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "col_size_of_width_a")))
	{
		u_int  col_size_a ;
		
		if( kik_str_to_uint( &col_size_a , value))
		{
			main_config.col_size_a = col_size_a ;
		}
		else
		{
			kik_msg_printf( "col size of width a %s is not valid.\n" , value) ;
		}
	}

	main_config.pic_file_path = NULL ;

	if( ( value = kik_conf_get_value( conf , "wall_picture")))
	{
		main_config.pic_file_path = strdup( value) ;
	}

	main_config.use_transbg = 0 ;

	if( ( value = kik_conf_get_value( conf , "use_transbg")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config.use_transbg = 1 ;
		}
	}

	if( main_config.pic_file_path && main_config.use_transbg)
	{
		kik_msg_printf(
			"wall picture and transparent background cannot be used at the same time.\n") ;

		/* using wall picture */
		main_config.use_transbg = 0 ;
	}

	main_config.brightness = 100 ;

	if( ( value = kik_conf_get_value( conf , "brightness")))
	{
		u_int  brightness ;
		
		if( kik_str_to_uint( &brightness , value))
		{
			main_config.brightness = brightness ;
		}
		else
		{
			kik_msg_printf( "shade ratio %s is not valid.\n" , value) ;
		}
	}
	
	main_config.contrast = 100 ;

	if( ( value = kik_conf_get_value( conf , "contrast")))
	{
		u_int  contrast ;
		
		if( kik_str_to_uint( &contrast , value))
		{
			main_config.contrast = contrast ;
		}
		else
		{
			kik_msg_printf( "contrast ratio %s is not valid.\n" , value) ;
		}
	}
	
	main_config.gamma = 100 ;

	if( ( value = kik_conf_get_value( conf , "gamma")))
	{
		u_int  gamma ;
		
		if( kik_str_to_uint( &gamma , value))
		{
			main_config.gamma = gamma ;
		}
		else
		{
			kik_msg_printf( "gamma ratio %s is not valid.\n" , value) ;
		}
	}
	
	main_config.fade_ratio = 100 ;
	
	if( ( value = kik_conf_get_value( conf , "fade_ratio")))
	{
		u_int  fade_ratio ;
		
		if( kik_str_to_uint( &fade_ratio , value) && fade_ratio <= 100)
		{
			main_config.fade_ratio = fade_ratio ;
		}
		else
		{
			kik_msg_printf( "fade ratio %s is not valid.\n" , value) ;
		}
	}

	if( ( main_config.encoding = ml_get_char_encoding( kik_get_codeset())) == ML_UNKNOWN_ENCODING)
	{
		main_config.encoding = ML_ISO8859_1 ;
	}
	
	if( ( value = kik_conf_get_value( conf , "ENCODING")))
	{
		ml_char_encoding_t  encoding ;

		if( strcasecmp( value , "AUTO") != 0)
		{
			if( ( encoding = ml_get_char_encoding( value)) == ML_UNKNOWN_ENCODING)
			{
				kik_msg_printf(
					"%s encoding is not supported. Auto detected encoding is used.\n" ,
					value) ;
			}
			else
			{
				main_config.encoding = encoding ;
			}
		}
	}

	main_config.xim_open_in_startup = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "xim_open_in_startup")))
	{
		if( strcmp( value , "false") == 0)
		{
			main_config.xim_open_in_startup = 0 ;
		}
	}

	main_config.use_bidi = 1 ;

	if( ( value = kik_conf_get_value( conf , "use_bidi")))
	{
		if( strcmp( value , "false") == 0)
		{
			main_config.use_bidi = 0 ;
		}
	}

	/* If value is "none" or not is also checked in x_screen.c */
	if( ( value = kik_conf_get_value( conf , "mod_meta_key")) &&
		strcmp( value , "none") != 0)
	{
		main_config.mod_meta_key = strdup( value) ;
	}
	else
	{
		main_config.mod_meta_key = NULL ;
	}
	
	if( ( value = kik_conf_get_value( conf , "mod_meta_mode")))
	{
		main_config.mod_meta_mode = x_get_mod_meta_mode( value) ;
	}
	else
	{
		main_config.mod_meta_mode = MOD_META_SET_MSB ;
	}

	if( ( value = kik_conf_get_value( conf , "bel_mode")))
	{
		main_config.bel_mode = x_get_bel_mode( value) ;
	}
	else
	{
		main_config.bel_mode = BEL_SOUND ;
	}

	main_config.use_vertical_cursor = 0 ;

	if( ( value = kik_conf_get_value( conf , "use_vertical_cursor")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config.use_vertical_cursor = 1 ;
		}
	}
	
	main_config.iscii_lang_type = ISCIILANG_MALAYALAM ;
	
	if( ( value = kik_conf_get_value( conf , "iscii_lang")))
	{
		ml_iscii_lang_type_t  type ;
		
		if( ( type = ml_iscii_get_lang( strdup( value))) != ISCIILANG_UNKNOWN)
		{
			main_config.iscii_lang_type = type ;
		}
	}

	main_config.use_extended_scroll_shortcut = 0 ;
	
	if( ( value = kik_conf_get_value( conf , "use_extended_scroll_shortcut")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config.use_extended_scroll_shortcut = 1 ;
		}
	}

	main_config.bs_mode = BSM_VOLATILE ;

	if( ( value = kik_conf_get_value( conf , "static_backscroll_mode")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config.bs_mode = BSM_STATIC ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "exec_cmd")) && strcmp( value , "true") == 0)
	{
		if( ( main_config.cmd_argv = malloc( sizeof( char*) * (argc + 1))) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
		#endif
		
			main_config.cmd_path = NULL ;
			main_config.cmd_argv = NULL ;
		}
		else
		{
			/*
			 * !! Notice !!
			 * cmd_path and strings in cmd_argv vector should be allocated
			 * by the caller.
			 */
			 
			main_config.cmd_path = argv[0] ;
			
			memcpy( &main_config.cmd_argv[0] , argv , sizeof( char*) * argc) ;
			main_config.cmd_argv[argc] = NULL ;
		}
	}
	else
	{
		main_config.cmd_path = NULL ;
		main_config.cmd_argv = NULL ;
	}
	
	if( ( value = kik_conf_get_value( conf , "icon_path")))
	{
		main_config.icon_path = strdup( value) ;
	}
	else
	{
		main_config.icon_path = NULL ;
	}
	
	return  1 ;
}

static int
config_final(void)
{
	free( main_config.disp_name) ;
	free( main_config.app_name) ;
	free( main_config.title) ;
	free( main_config.icon_name) ;
	free( main_config.term_type) ;
	free( main_config.conf_menu_path) ;
	free( main_config.pic_file_path) ;
	free( main_config.scrollbar_view_name) ;
	free( main_config.fg_color) ;
	free( main_config.bg_color) ;
	free( main_config.cursor_fg_color) ;
	free( main_config.cursor_bg_color) ;
	free( main_config.sb_fg_color) ;
	free( main_config.sb_bg_color) ;
	free( main_config.icon_path) ;
	free( main_config.mod_meta_key) ;
	free( main_config.cmd_argv) ;

	return  1 ;
}

static void
client_connected(void)
{
	struct sockaddr_un  addr ;
	socklen_t  sock_len ;
	int  fd ;
	FILE *  fp ;
	kik_file_t *  from ;
	char *  line ;
	size_t  line_len ;
	char *  args ;
	char **  argv ;
	int  argc ;
	kik_conf_t *  conf ;
	main_config_t  orig_conf ;

	if( ( fd = accept( sock_fd , (struct sockaddr *) &addr , &sock_len)) < 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " accept failed.\n") ;
	#endif
	
		return ;
	}

	if( ( fp = fdopen( fd , "r")) == NULL)
	{
		close( fd) ;
		
		return ;
	}

	if( ( from = kik_file_new( fp)) == NULL)
	{
		fclose( fp) ;
	
		return ;
	}

	if( ( line = kik_file_get_line( from , &line_len)) == NULL)
	{
		kik_file_delete( from) ;
		fclose( fp) ;

		return ;
	}

	if( ( args = alloca( line_len)) == NULL)
	{
		return ;
	}

	strncpy( args , line , line_len - 1) ;
	args[line_len - 1] = '\0' ;

	kik_file_delete( from) ;
	fclose( fp) ;


	/*
	 * parsing options.
	 */

	argc = 0 ;
	if( ( argv = alloca( sizeof( char*) * line_len)) == NULL)
	{
		return ;
	}

	while( ( argv[argc] = kik_str_sep( &args , " \t")))
	{
		if( *argv[argc] != '\0')
		{
			argc ++ ;
		}
	}

	if( ( conf = get_min_conf( argc , argv)) == NULL)
	{
		return ;
	}

#ifdef  __DEBUG
	{
		int  i ;

		for( i = 0 ; i < argc ; i ++)
		{
			kik_msg_printf( "%s\n" , argv[i]) ;
		}
	}
#endif

	if( ! kik_conf_parse_args( conf , &argc , &argv))
	{
		kik_conf_delete( conf) ;

		return ;
	}
	
	orig_conf = main_config ;

	config_init( conf , argc , argv) ;

	kik_conf_delete( conf) ;
	
	if( ! open_term())
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " open_term() failed.\n") ;
	#endif
	}

	config_final() ;

	main_config = orig_conf ;
}

static void
receive_next_event(void)
{
	int  count ;
	int  xfd ;
	int  ptyfd ;
	int  maxfd ;
	int  ret ;
	fd_set  read_fds ;
	struct timeval  tval ;
	x_display_t **  displays ;
	u_int  num_of_displays ;
	ml_term_t **  terms ;
	u_int  num_of_terms ;

	num_of_terms = ml_get_all_terms( &terms) ;
	
	/*
	 * flush buffer
	 */

	for( count = 0 ; count < num_of_terms ; count ++)
	{
		ml_term_flush( terms[count]) ;
	}
	
	while( 1)
	{
		/* on Linux tv_usec,tv_sec members are zero cleared after select() */
		tval.tv_usec = 50000 ;	/* 0.05 sec */
		tval.tv_sec = 0 ;

		maxfd = 0 ;
		FD_ZERO( &read_fds) ;

		displays = x_get_opened_displays( &num_of_displays) ;
		
		for( count = 0 ; count < num_of_displays ; count ++)
		{
			/*
			 * it is necessary to flush events here since some events
			 * may have happened in idling
			 */
			x_window_manager_receive_next_event( &displays[count]->win_man) ;

			xfd = x_display_fd( displays[count]) ;
			
			FD_SET( xfd , &read_fds) ;
		
			if( xfd > maxfd)
			{
				maxfd = xfd ;
			}
		}

		for( count = 0 ; count < num_of_terms ; count ++)
		{
			ptyfd = ml_term_get_pty_fd( terms[count]) ;
			FD_SET( ptyfd , &read_fds) ;

			if( ptyfd > maxfd)
			{
				maxfd = ptyfd ;
			}
		}
		
		if( sock_fd >= 0)
		{
			FD_SET( sock_fd , &read_fds) ;
			
			if( sock_fd > maxfd)
			{
				maxfd = sock_fd ;
			}
		}

		if( ( ret = select( maxfd + 1 , &read_fds , NULL , NULL , &tval)) != 0)
		{
			break ;
		}

		for( count = 0 ; count < num_of_displays ; count ++)
		{
			x_window_manager_idling( &displays[count]->win_man) ;
		}
	}
	
	if( ret < 0)
	{
		/* error happened */
		
		return ;
	}

	/*
	 * Processing order should be as follows.
	 *
	 * PTY -> X WINDOW -> Socket
	 */
	 
	for( count = 0 ; count < num_of_terms ; count ++)
	{
		if( FD_ISSET( ml_term_get_pty_fd( terms[count]) , &read_fds))
		{
			ml_term_parse_vt100_sequence( terms[count]) ;
		}
	}
	
	for( count = 0 ; count < num_of_displays ; count ++)
	{
		if( FD_ISSET( x_display_fd( displays[count]) , &read_fds))
		{
			x_window_manager_receive_next_event( &displays[count]->win_man) ;
		}
	}

	if( sock_fd >= 0)
	{
		if( FD_ISSET( sock_fd , &read_fds))
		{
			client_connected() ;
		}
	}
}


/* --- global functions --- */

int
x_term_manager_init(
	int  argc ,
	char **  argv
	)
{
	kik_conf_t *  conf ;
	int  use_xim ;
	u_int  min_font_size ;
	u_int  max_font_size ;
	char *  font_rcfile ;
	char *  rcpath ;
	char *  value ;

	if( ( conf = get_min_conf( argc , argv)) == NULL)
	{
		return  0 ;
	}

	kik_conf_add_opt( conf , 'h' , "help" , 1 , "help" ,
		"show this help message") ;
	kik_conf_add_opt( conf , 'v' , "version" , 1 , "version" ,
		"show version message") ;
	kik_conf_add_opt( conf , 'P' , "ptys" , 0 , "ptys" , 
		"number of ptys to use in start up [1]") ;
	kik_conf_add_opt( conf , 'R' , "fsrange" , 0 , "font_size_range" , 
		"font size range for GUI configurator [6-30]") ;
	kik_conf_add_opt( conf , 'W' , "sep" , 0 , "word_separators" , 
		"word-separating characters for double-click [,.:;/@]") ;
	kik_conf_add_opt( conf , 'Y' , "decsp" , 1 , "compose_dec_special_font" ,
		"compose dec special font [false]") ;
#ifdef  ANTI_ALIAS
	kik_conf_add_opt( conf , 'c' , "cp932" , 1 , "use_cp932_ucs_for_xft" , 
		"use CP932-Unicode mapping table for JISX0208 [false]") ;
#endif
	kik_conf_add_opt( conf , 'i' , "xim" , 1 , "use_xim" , 
		"use XIM (X Input Method) [true]") ;
	kik_conf_add_opt( conf , 'j' , "daemon" , 0 , "daemon_mode" ,
		"start as a daemon [none/blend/genuine]") ;
	
	if( ! kik_conf_parse_args( conf , &argc , &argv))
	{
		kik_conf_delete( conf) ;

		return  0 ;
	}

	/*
	 * daemon
	 */

	is_genuine_daemon = 0 ;
	sock_fd = -1 ;
	
	if( ( value = kik_conf_get_value( conf , "daemon_mode")))
	{
		if( strcmp( value , "genuine") == 0)
		{
			if( ( sock_fd = start_daemon()) < 0)
			{
				kik_msg_printf( "mlterm failed to become daemon.\n") ;
			}
			else
			{
				is_genuine_daemon = 1 ;
			}
		}
		else if( strcmp( value , "blend") == 0)
		{
			if( ( sock_fd = start_daemon()) < 0)
			{
				kik_msg_printf( "mlterm failed to become daemon.\n") ;
			}
		}
	#if  0
		else if( strcmp( value , "none")
		{
		}
	#endif
	}

	use_xim = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "use_xim")))
	{
		if( strcmp( value , "false") == 0)
		{
			use_xim = 0 ;
		}
	}

	x_xim_init( use_xim) ;
	
	if( ( value = kik_conf_get_value( conf , "font_size_range")))
	{
		if( ! get_font_size_range( &min_font_size , &max_font_size , value))
		{
			kik_msg_printf( "font_size_range = %s is illegal.\n" , value) ;

			/* default values are used */
			min_font_size = 6 ;
			max_font_size = 30 ;
		}
	}
	else
	{
		/* default values are used */
		min_font_size = 6 ;
		max_font_size = 30 ;
	}

	if( ! x_font_custom_init( &normal_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	font_rcfile = "mlterm/font" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &normal_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &normal_font_custom , rcpath) ;

		free( rcpath) ;
	}
	
	if( ! x_font_custom_init( &v_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	font_rcfile = "mlterm/vfont" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &v_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &v_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ! x_font_custom_init( &t_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	font_rcfile = "mlterm/tfont" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &t_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &t_font_custom , rcpath) ;

		free( rcpath) ;
	}

#ifdef  ANTI_ALIAS
	if( ! x_font_custom_init( &aa_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	font_rcfile = "mlterm/aafont" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &aa_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &aa_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ! x_font_custom_init( &vaa_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	font_rcfile = "mlterm/vaafont" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &vaa_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &vaa_font_custom , rcpath) ;

		free( rcpath) ;
	}
	
	if( ! x_font_custom_init( &taa_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	font_rcfile = "mlterm/taafont" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &taa_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &taa_font_custom , rcpath) ;

		free( rcpath) ;
	}
#endif
	
	if( ( value = kik_conf_get_value( conf , "compose_dec_special_font")))
	{
		if( strcmp( value , "true") == 0)
		{
			x_compose_dec_special_font() ;
		}
	}

#ifdef  ANTI_ALIAS
	if( ( value = kik_conf_get_value( conf , "use_cp932_ucs_for_xft")) == NULL ||
		strcmp( value , "true") == 0)
	{
		ml_use_cp932_ucs_for_xft() ;
	}
#endif

	if( ! x_color_custom_init( &color_custom))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_color_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}
	
	if( ( rcpath = kik_get_sys_rc_path( "mlterm/color")))
	{
		x_color_custom_read_conf( &color_custom , rcpath) ;

		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( "mlterm/color")))
	{
		x_color_custom_read_conf( &color_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ! x_shortcut_init( &shortcut))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_shortcut_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	if( ( rcpath = kik_get_sys_rc_path( "mlterm/key")))
	{
		x_shortcut_read_conf( &shortcut , rcpath) ;

		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( "mlterm/key")))
	{
		x_shortcut_read_conf( &shortcut , rcpath) ;

		free( rcpath) ;
	}

	if( ! x_termcap_init( &termcap))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_termcap_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	if( ( rcpath = kik_get_sys_rc_path( "mlterm/termcap")))
	{
		x_termcap_read_conf( &termcap , rcpath) ;

		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( "mlterm/termcap")))
	{
		x_termcap_read_conf( &termcap , rcpath) ;

		free( rcpath) ;
	}
	
	/*
	 * others
	 */

	num_of_startup_xterms = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "ptys")))
	{
		u_int  ptys ;
		
		if( ! kik_str_to_uint( &ptys , value) || ( ! is_genuine_daemon && ptys == 0))
		{
			kik_msg_printf( "ptys %s is not valid.\n" , value) ;
		}
		else
		{
			if( ptys > MAX_TERMS)
			{
				ptys = MAX_TERMS ;
			}
			
			num_of_startup_xterms = ptys ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "word_separators")))
	{
		ml_set_word_separators( value) ;
	}

	if( ( version = kik_conf_get_version( conf)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " version string is NULL\n") ;
	#endif
	}

	config_init( conf , argc , argv) ;

	kik_conf_delete( conf) ;

	if( *main_config.disp_name)
	{
		/*
		 * setting DISPLAY environment variable to match "--display" option.
		 */
		
		char *  env ;

		if( ( env = malloc( strlen( main_config.disp_name) + 9)))
		{
			sprintf( env , "DISPLAY=%s" , main_config.disp_name) ;
			putenv( env) ;
		}
	}

	system_listener.self = NULL ;
	system_listener.exit = __exit ;
	system_listener.open_screen = open_screen ;
	system_listener.close_screen = close_screen ;
	system_listener.open_pty = open_pty ;
	system_listener.close_pty = NULL ;
	system_listener.pty_closed = pty_closed ;

	signal( SIGHUP , sig_fatal) ;
	signal( SIGINT , sig_fatal) ;
	signal( SIGQUIT , sig_fatal) ;
	signal( SIGTERM , sig_fatal) ;

	ml_term_manager_init() ;
	
	kik_alloca_garbage_collect() ;
	
	return  1 ;
}

int
x_term_manager_final(void)
{
	int  count ;

	free( version) ;
	
	config_final() ;
	
	ml_free_word_separators() ;
	
	for( count = 0 ; count < num_of_xterms ; count ++)
	{
		x_term_final( &xterms[count]) ;
	}

	free( xterms) ;

	ml_term_manager_final() ;

	x_display_close_all() ;

	x_xim_final() ;
	
	x_font_custom_final( &normal_font_custom) ;
	x_font_custom_final( &v_font_custom) ;
	x_font_custom_final( &t_font_custom) ;
#ifdef  ANTI_ALIAS
	x_font_custom_final( &aa_font_custom) ;
	x_font_custom_final( &vaa_font_custom) ;
	x_font_custom_final( &taa_font_custom) ;
#endif

	x_color_custom_final( &color_custom) ;
	
	x_shortcut_final( &shortcut) ;
	x_termcap_final( &termcap) ;

	kik_sig_child_final() ;
	
	return  1 ;
}

void
x_term_manager_event_loop(void)
{
	int  count ;

	for( count = 0 ; count < num_of_startup_xterms ; count ++)
	{
		if( ! open_term())
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " open_term() failed.\n") ;
		#endif

			if( count == 0 && ! is_genuine_daemon)
			{
				kik_msg_printf( "Unable to start - open_term() failed.\n") ;

				exit(1) ;
			}
			else
			{
				break ;
			}
		}
	}

	while( 1)
	{
		int  count ;
		
		kik_alloca_begin_stack_frame() ;

		receive_next_event() ;

		ml_close_dead_terms() ;

		if( dead_mask)
		{
			for( count = 0 ; count < MAX_TERMS ; count ++)
			{
				if( dead_mask & (0x1 << count))
				{
					x_term_final( &xterms[count]) ;
					xterms[count] = xterms[--num_of_xterms] ;
				}
			}

			dead_mask = 0 ;
		}
		
		if( num_of_xterms == 0 && ! is_genuine_daemon)
		{
			if( un_file)
			{
				unlink( un_file) ;
			}
			
			exit( 0) ;
		}

		kik_alloca_end_stack_frame() ;
	}
}
