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
#include  <fcntl.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* kik_str_sep/kik_str_to_int/kik_str_alloca_dup/strdup */
#include  <kiklib/kik_path.h>	/* kik_basename */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */
#include  <kiklib/kik_mem.h>	/* alloca/kik_alloca_garbage_collect/malloc/free */
#include  <kiklib/kik_conf.h>
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_net.h>	/* socket/bind/listen/sockaddr_un */
#include  <kiklib/kik_types.h>	/* u_int */
#include  <kiklib/kik_sig_child.h>
#include  <ml_term_manager.h>
#include  <ml_char_encoding.h>

#include  "version.h"
#include  "x_main_config.h"
#include  "x_xim.h"
#include  "x_sb_screen.h"
#include  "x_display.h"
#include  "x_termcap.h"
#include  "x_imagelib.h"

#define  MAX_SCREENS  (8*sizeof(dead_mask))

#define MAX_ADDTIONAL_FDS  3

/* --- static variables --- */

static x_screen_t **  screens ;
static u_int  num_of_screens ;
static u_long  dead_mask ;

static u_int  num_of_startup_screens ;
static u_int  num_of_startup_ptys ;

static x_system_event_listener_t  system_listener ;

static char *  version ;

static x_main_config_t  main_config ;

static x_color_config_t  color_config ;
static x_shortcut_t  shortcut ;
static x_termcap_t  termcap ;

static int  sock_fd ;
static char *  un_file ;
static int8_t  is_genuine_daemon ;

static struct
{
	int  fd ;
	void (*handler)( void) ;

} additional_fds[MAX_ADDTIONAL_FDS] ;

/* --- static functions --- */

static kik_conf_t *
conf_new(void)
{
	kik_conf_t *  conf ;
	
	if( ( conf = kik_conf_new( "mlterm" ,
		MAJOR_VERSION , MINOR_VERSION , REVISION , PATCH_LEVEL , CVS_REVISION)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " kik_conf_new() failed.\n") ;
	#endif
	
		return  NULL ;
	}

	return  conf ;
}

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

	if( str_p  == NULL)
	{
		kik_msg_printf( "max font size is missing.\n");

		return 0;
	}
	
	if( ! kik_str_to_uint( max , str_p))
	{
		kik_msg_printf( "max font size %s is not valid.\n" , str_p) ;

		return  0 ;
	}

	return  1 ;
}

static ml_term_t *
create_term_intern(void)
{
	ml_term_t *  term ;
	
	if( ( term = ml_create_term( main_config.cols , main_config.rows ,
			main_config.tab_size , main_config.num_of_log_lines ,
			main_config.encoding , main_config.is_auto_encoding , 
			main_config.unicode_font_policy ,
			main_config.col_size_a , main_config.use_char_combining ,
			main_config.use_multi_col_char , main_config.use_bidi ,
			x_termcap_get_bool_field(
				x_termcap_get_entry( &termcap , main_config.term_type) , ML_BCE) ,
			main_config.use_dynamic_comb , main_config.bs_mode ,
			main_config.vertical_mode , main_config.iscii_lang_type)) == NULL)
	{
		return  NULL ;
	}

	if( main_config.title)
	{
		ml_term_set_window_name( term , main_config.title) ;
	}

	if( main_config.icon_name)
	{
		ml_term_set_icon_name( term , main_config.icon_name) ;
	}

	if( main_config.icon_path)
	{
		ml_term_set_icon_path( term , main_config.icon_path) ;
	}

	if( main_config.logging_vt_seq)
	{
		ml_term_set_logging_vt_seq( term , main_config.logging_vt_seq) ;
	}

	return  term ;
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
	char *  env[6] ;	/* MLTERM,TERM,WINDOWID,DISPLAY,COLORFGBG,NULL */
	char **  env_p ;
	char  wid_env[9 + DIGIT_STR_LEN(Window) + 1] ;	/* "WINDOWID="(9) + [32bit digit] + NULL(1) */
	char *  ver_env ;
	char *  disp_env ;
	char *  term_env ;
	char *  colorfgbg_env ;

	env_p = env ;

	if( version && ( ver_env = alloca( 7 + strlen( version) + 1)))
	{
		sprintf( ver_env , "MLTERM=%s" , version) ;
		*(env_p ++) = ver_env ;
	}
	
	sprintf( wid_env , "WINDOWID=%ld" , window) ;
	*(env_p ++) = wid_env ;
	
	/* "DISPLAY="(8) + NULL(1) */
	if( display && ( disp_env = alloca( 8 + strlen( display) + 1)))
	{
		sprintf( disp_env , "DISPLAY=%s" , display) ;
		*(env_p ++) = disp_env ;
	}

	/* "TERM="(5) + NULL(1) */
	if( term_type && ( term_env = alloca( 5 + strlen( term_type) + 1)))
	{
		sprintf( term_env , "TERM=%s" , term_type) ;
		*(env_p ++) = term_env ;
	}

	/* COLORFGBG=default;default */
	if( ( colorfgbg_env = kik_str_alloca_dup( "COLORFGBG=default;default")))
	{
		*(env_p ++) = colorfgbg_env ;
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
open_screen_intern(
	char *  pty
	)
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
	void *  p ;
	
	/*
	 * these are dynamically allocated.
	 */
	term = NULL ;
	disp = NULL ;
	font_man = NULL ;
	color_man = NULL ;
	screen = NULL ;
	sb_screen = NULL ;
	root = NULL ;

	if( MAX_SCREENS <= num_of_screens)
	{
		return  0 ;
	}

	if( pty)
	{
		if( ( term = ml_get_detached_term( pty)) == NULL)
		{
			return  0 ;
		}
	}
	else
	{
	#if  0
		if( ( term = ml_get_detached_term( NULL)) == NULL &&
			( term = create_term_intern()) == NULL)
	#else
		if( ( term = create_term_intern()) == NULL)
	#endif
		{
			return  0 ;
		}
	}

	if( ( disp = x_display_open( main_config.disp_name)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_display_open failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( main_config.unicode_font_policy == NOT_USE_UNICODE_FONT ||
		main_config.iso88591_font_for_usascii)
	{
		usascii_font_cs = x_get_usascii_font_cs( ML_ISO8859_1) ;
		usascii_font_cs_changable = 0 ;
	}
	else if( main_config.unicode_font_policy == ONLY_USE_UNICODE_FONT)
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
		main_config.type_engine , main_config.font_present ,
		main_config.font_size , usascii_font_cs ,
		usascii_font_cs_changable , main_config.use_multi_col_char ,
		main_config.step_in_changing_font_size)) == NULL)
	{
		char **  csnames;

		csnames = x_font_get_cs_names( usascii_font_cs) ;
		if( csnames[0] == NULL)
		{
			kik_msg_printf(
			  "Current encoding \"%s\" is supported only with "
			  "Unicode font.  "
			  "Please use \"-u\" option.\n",
			  ml_get_char_encoding_name(ml_term_get_encoding(term))) ;
		}
		else if( strcmp( csnames[0] , "iso10646-1" ) == 0 )
		{
			kik_msg_printf(
			  "No fonts found for charset \"%s\".  "
			  "Please install fonts.\n" , csnames[0]) ;
		}
		else if( csnames[1] == NULL)
		{
			kik_msg_printf(
			  "No fonts found for charset \"%s\".  "
			  "Please install fonts or use Unicode font "
			  "(\"-u\" option).\n" , csnames[0]) ;
		}
		else
		{
			kik_msg_printf(
			  "No fonts found for charset \"%s\" or \"%s\".  "
			  "Please install fonts or use Unicode font "
			  "(\"-u\" option).\n" , csnames[0] , csnames[1] );
		}
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_font_manager_new() failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( ( color_man = x_color_manager_new( disp->display ,
				DefaultScreen( disp->display) , &color_config ,
				main_config.fg_color , main_config.bg_color ,
				main_config.cursor_fg_color , main_config.cursor_bg_color)) == NULL)
	{
		goto  error ;
	}

	if( ( screen = x_screen_new( term , font_man , color_man ,
			x_termcap_get_entry( &termcap , main_config.term_type) ,
			main_config.brightness , main_config.contrast , main_config.gamma ,
			main_config.fade_ratio , &shortcut ,
			main_config.screen_width_ratio , main_config.screen_height_ratio ,
			main_config.mod_meta_key ,
			main_config.mod_meta_mode , main_config.bel_mode ,
			main_config.receive_string_via_ucs , main_config.pic_file_path ,
			main_config.use_transbg , main_config.use_vertical_cursor ,
			main_config.big5_buggy , main_config.conf_menu_path_1 ,
			main_config.conf_menu_path_2 , main_config.conf_menu_path_3 ,
			main_config.use_extended_scroll_shortcut ,
			main_config.borderless , main_config.line_space ,
			main_config.input_method)) == NULL)
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
		root = &screen->window ;
	}

	if( ! x_window_manager_show_root( &disp->win_man , root ,
		main_config.x , main_config.y , main_config.geom_hint , main_config.app_name))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_window_manager_show_root() failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( main_config.icon_path)
	{
		x_window_manager_set_icon( root, main_config.icon_path);
	}

	if( pty && main_config.cmd_argv)
	{
		int  count ;
		for( count = 0 ; main_config.cmd_argv[count] ; count ++)
		{
			ml_term_write( term , main_config.cmd_argv[count] ,
				strlen( main_config.cmd_argv[count]) , 0) ;
			ml_term_write( term , " " , 1 , 0) ;
		}

		ml_term_write( term , "\n" , 1 , 0) ;
	}
	else
	{
		if( ! open_pty_intern( term , main_config.cmd_path , main_config.cmd_argv ,
			XDisplayString( disp->display) , root->my_window ,
			main_config.term_type , main_config.use_login_shell))
		{
			goto  error ;
		}
	}

	if( main_config.init_str)
	{
		ml_term_write( term , main_config.init_str , strlen( main_config.init_str) , 0) ;
	}
	
	if( ( p = realloc( screens , sizeof( x_screen_t*) * (num_of_screens + 1))) == NULL)
	{
		goto  error ;
	}

	screens = p ;
	screens[num_of_screens++] = screen ;
	
	return  1 ;
	
error:
	if( font_man)
	{
		x_font_manager_delete( font_man) ;
	}

	if( color_man)
	{
		x_color_manager_delete( color_man) ;
	}

	if( ! root || ! x_window_manager_remove_root( &disp->win_man , root))
	{
		/*
		 * If root is still NULL or is not registered to win_man yet.
		 */
		 
		if( screen)
		{
			x_screen_delete( screen) ;
		}

		if( sb_screen)
		{
			x_sb_screen_delete( sb_screen) ;
		}
	}

	if( disp)
	{
		x_display_close( disp) ;
	}

	return  0 ;
}

static int
close_screen_intern(
	x_screen_t *  screen
	)
{
	x_window_t *  root ;
	x_window_manager_t *  win_man ;

	x_screen_detach( screen) ;
	x_font_manager_delete( screen->font_man) ;
	x_color_manager_delete( screen->color_man) ;

	root = x_get_root_window( &screen->window) ;
	win_man = root->win_man ;
	
	if( win_man->num_of_roots == 1)
	{
		Display *  display_to_close ;

		display_to_close = root->display ;
		x_window_manager_remove_root( win_man , root) ;
		x_display_close_2( display_to_close) ;
	}
	else
	{
		x_window_manager_remove_root( win_man , root) ;
	}

	return  1 ;
}


/*
 * callbacks of x_system_event_listener_t
 */
 
/*
 * EXIT_PROGRAM shortcut calls this at last.
 * this is for debugging.
 */
#ifdef  KIK_DEBUG
#include  <kiklib/kik_locale.h>		/* kik_locale_final */
#endif
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
	x_screen_t *  screen ,
	char *  dev
	)
{
	int  count ;

	for( count = 0 ; count < num_of_screens ; count ++)
	{
		if( screen == screens[count])
		{
			ml_term_t *  new ;

			if( dev)
			{
				if( ( new = ml_get_detached_term( dev)) == NULL)
				{
					return ;
				}
			}
			else
			{
				if( ( new = create_term_intern()) == NULL)
				{
					return ;
				}

				if( ! open_pty_intern( new , main_config.cmd_path , main_config.cmd_argv ,
					XDisplayString( screen->window.display) ,
					x_get_root_window( &screen->window)->my_window ,
					main_config.term_type , main_config.use_login_shell))
				{
					return ;
				}
			}
			
			x_screen_detach( screen) ;
			x_screen_attach( screen , new) ;
			
			return ;
		}
	}
}

static void
next_pty(
	void *  p ,
	x_screen_t *  screen
	)
{
	int  count ;
	
	for( count = 0 ; count < num_of_screens ; count ++)
	{
		if( screen == screens[count])
		{
			ml_term_t *  old ;
			ml_term_t *  new ;

			if( ( old = x_screen_detach( screen)) == NULL)
			{
				return ;
			}

			if( ( new = ml_next_term( old)) == NULL)
			{
				x_screen_attach( screen , old) ;
			}
			else
			{
				x_screen_attach( screen , new) ;
			}
			
			return ;
		}
	}
}

static void
prev_pty(
	void *  p ,
	x_screen_t *  screen
	)
{
	int  count ;
	
	for( count = 0 ; count < num_of_screens ; count ++)
	{
		if( screen == screens[count])
		{
			ml_term_t *  old ;
			ml_term_t *  new ;

			if( ( old = x_screen_detach( screen)) == NULL)
			{
				return ;
			}
			
			if( ( new = ml_prev_term( old)) == NULL)
			{
				x_screen_attach( screen , old) ;
			}
			else
			{
				x_screen_attach( screen , new) ;
			}
			
			return ;
		}
	}
}
	
static void
pty_closed(
	void *  p ,
	x_screen_t *  screen	/* screen->term was already deleted. */
	)
{
	int  count ;

	for( count = num_of_screens - 1 ; count >= 0 ; count --)
	{
		if( screen == screens[count])
		{
			ml_term_t *  term ;
			
			if( ( term = ml_get_detached_term( NULL)) == NULL)
			{
				close_screen_intern( screen) ;
				screens[count] = screens[--num_of_screens] ;
			}
			else
			{
				x_screen_attach( screen , term) ;
			}

			return ;
		}
	}
}

static void
open_screen(
	void *  p
	)
{
	if( ! open_screen_intern(NULL))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " open_screen_intern failed.\n") ;
	#endif
	}
}
	
static void
close_screen(
	void *  p ,
	x_screen_t *  screen
	)
{
	int  count ;
	
	for( count = 0 ; count < num_of_screens ; count ++)
	{
		if( screen == screens[count])
		{
			dead_mask |= (1 << count) ;
			
			return ;
		}
	}
}

static ml_term_t *
get_pty(
	void *  p ,
	char *  dev
	)
{
	return  ml_get_term( dev) ;
}

static char *
pty_list(
	void *  p
	)
{
	return  ml_get_pty_list() ;
}

static void
config_saved(
	void *  p
	)
{
	kik_conf_t *  conf ;
	char *  argv[] = { "mlterm" , NULL } ;

	x_main_config_final( &main_config) ;

	if( ( conf = conf_new()) == NULL)
	{
		return ;
	}
	
	x_prepare_for_main_config( conf) ;
	x_main_config_init( &main_config , conf , 1 , argv) ;

	kik_conf_delete( conf) ;
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

	if( ( sock_fd = socket( PF_LOCAL , SOCK_STREAM , 0)) < 0)
	{
		return  -1 ;
	}

	while( bind( sock_fd , (struct sockaddr *) &servaddr , sizeof( servaddr)) < 0)
	{
		if( errno == EADDRINUSE)
		{
			if( connect( sock_fd , (struct sockaddr*) &servaddr , sizeof( servaddr)) == 0)
			{
				close( sock_fd) ;
				
				kik_msg_printf( "daemon is already running.\n") ;
				
				return  -1 ;
			}

			kik_msg_printf( "removing stale lock file %s.\n" , path) ;
			
			if( unlink( path) == 0)
			{
				continue ;
			}
		}
		else
		{
			close( sock_fd) ;

			kik_msg_printf( "failed to lock file %s: %s\n" , path , strerror(errno)) ;

			return  -1 ;
		}
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
	char *  args_dup ;
	char *  p ;

	fp = NULL ;

	sock_len = sizeof( addr) ;

	if( ( fd = accept( sock_fd , (struct sockaddr *) &addr , &sock_len)) < 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " accept failed.\n") ;
	#endif
	
		return ;
	}

	/*
	 * Set the close-on-exec flag.
	 * If this flag off, this fd remained open until the child process forked in
	 * open_screen_intern()(ml_term_open_pty()) close it.
	 */
	fcntl( fd , F_SETFD , 1) ;

	if( ( fp = fdopen( fd , "r+")) == NULL)
	{
		goto  crit_error ;
	}

	if( ( from = kik_file_new( fp)) == NULL)
	{
		goto  crit_error ;
	}

	if( ( line = kik_file_get_line( from , &line_len)) == NULL)
	{
		kik_file_delete( from) ;
		
		goto  crit_error ;
	}

	if( ( args = alloca( line_len)) == NULL)
	{
		kik_file_delete( from) ;

		goto  error ;
	}

	strncpy( args , line , line_len - 1) ;
	args[line_len - 1] = '\0' ;

	kik_file_delete( from) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s\n" , args) ;
#endif
	
	/*
	 * parsing options.
	 */

	argc = 0 ;

	if( ( argv = alloca( sizeof( char*) * ( line_len + 1))) == NULL)
	{
		goto  error ;
	}

	if( ( args_dup = alloca( line_len)) == NULL)
	{
		goto  error ;
	}
	
	p = args_dup ;

	while( *args)
	{
		int  quoted ;

		while( *args == ' ' || *args == '\t')
		{
			if( *args == '\0')
			{
				goto  parse_end ;
			}

			args ++ ;
		}

		if( *args == '\"')
		{
			quoted = 1 ;
			args ++ ;
		}
		else
		{
			quoted = 0 ;
		}
		
		while( *args)
		{
			if( quoted)
			{
				if( *args == '\"')
				{
					args ++ ;
					
					break ;
				}
			}
			else
			{
				if( *args == ' ' || *args == '\t')
				{
					args ++ ;
					
					break ;
				}
			}
			
			if( *args == '\\' && *(args + 1) == '\"')
			{
				*(p ++) = *(++ args) ;
			}
			else
			{
				*(p ++) = *args ;
			}

			args ++ ;
		}

		*(p ++) = '\0' ;
		argv[argc ++] = args_dup ;
		args_dup = p ;
	}

parse_end:

	/* NULL terminator (POSIX exec family style) */
	argv[argc] = NULL ;

#ifdef  __DEBUG
	{
		int  i ;

		for( i = 0 ; i < argc ; i ++)
		{
			kik_msg_printf( "%s\n" , argv[i]) ;
		}
	}
#endif

	if( argc == 0)
	{
		goto  error ;
	}

	if( argc == 2 &&
		( strncmp( argv[1] , "-P" , 2) == 0 || strncmp( argv[1] , "--ptylist" , 9) == 0))
	{
		/*
		 * mlclient -P or mlclient --ptylist
		 */

		ml_term_t **  terms ;
		u_int  num ;
		int  count ;
	
		num = ml_get_all_terms( &terms) ;
		for( count = 0 ; count < num ; count ++)
		{
			fprintf( fp , "%s" , ml_term_get_slave_name( terms[count])) ;
			if( ml_term_window_name( terms[count]))
			{
				fprintf( fp , "(whose title is %s)" , ml_term_window_name( terms[count])) ;
			}
			if( ml_term_is_attached( terms[count]))
			{
				fprintf( fp , " is active:)\n") ;
			}
			else
			{
				fprintf( fp , " is sleeping.zZ\n") ;
			}
		}
	}
	else if( argc == 2 && strncmp( argv[1] , "--kill" , 6) == 0)
	{
		if( un_file)
		{
			unlink( un_file) ;
		}
		fprintf( fp, "bye\n") ;
		exit( 0) ;
	}
	else
	{
		kik_conf_t *  conf ;
		x_main_config_t  orig_conf ;
		char *  pty ;
		
		if( argc >= 2 && *argv[1] != '-')
		{
			/*
			 * mlclient [dev] [options...]
			 */
			 
			pty = argv[1] ;
			argv[1] = argv[0] ;
			argv = &argv[1] ;
			argc -- ;
		}
		else
		{
			pty = NULL ;
		}

		if( ( conf = conf_new()) == NULL)
		{
			goto  error ;
		}
		
		x_prepare_for_main_config(conf) ;

		if( ! kik_conf_parse_args( conf , &argc , &argv))
		{
			kik_conf_delete( conf) ;

			goto  error ;
		}

		orig_conf = main_config ;

		x_main_config_init( &main_config , conf , argc , argv) ;

		kik_conf_delete( conf) ;

		if( ! open_screen_intern( pty))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " open_screen_intern() failed.\n") ;
		#endif
		}
		
		x_main_config_final( &main_config) ;

		main_config = orig_conf ;
	}

	fclose( fp) ;
	
	return ;

error:
	{
		char  msg[] = "Error happened.\n" ;

		write( fd , msg , sizeof( msg)) ;
	}

	/* If fd may be invalid, jump to here directly. */
crit_error:
	if( fp)
	{
		fclose( fp) ;
	}
	else
	{
		close( fd) ;
	}
}

static void
receive_next_event(void)
{
	int  count ;
	int  xfd ;
	int  ptyfd ;
	int  maxfd ;
	int  ret ;
	int  i ;
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

		for( i = 0 ; i < MAX_ADDTIONAL_FDS ; i++)
		{
			if( additional_fds[i].fd >= 0)
			{
				FD_SET( additional_fds[i].fd , &read_fds) ;
				
				if( additional_fds[i].fd > maxfd)
				{
					maxfd = additional_fds[i].fd ;
				}
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
	 * PTY -> X WINDOW -> Socket -> additional_fds
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

	for( i = 0 ; i < MAX_ADDTIONAL_FDS ; i++)
	{
		if( additional_fds[i].fd >= 0)
		{
			if( FD_ISSET( additional_fds[i].fd , &read_fds))
			{
				(*additional_fds[i].handler)() ;

				break ;
			}
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
	char *  value ;
	int  use_xim ;
	u_int  min_font_size ;
	u_int  max_font_size ;
	char *  rcpath ;
	int  i ;

	if( ! x_color_config_init( &color_config))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_color_config_init failed.\n") ;
	#endif
	
		return  0 ;
	}
	
	if( ( rcpath = kik_get_sys_rc_path( "mlterm/color")))
	{
		x_read_color_config( &color_config , rcpath) ;
		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( "mlterm/color")))
	{
		x_read_color_config( &color_config , rcpath) ;
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
		x_read_shortcut_config( &shortcut , rcpath) ;
		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( "mlterm/key")))
	{
		x_read_shortcut_config( &shortcut , rcpath) ;
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
		x_read_termcap_config( &termcap , rcpath) ;
		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( "mlterm/termcap")))
	{
		x_read_termcap_config( &termcap , rcpath) ;
		free( rcpath) ;
	}


	if( ( conf = conf_new()) == NULL)
	{
		return  0 ;
	}
	
	x_prepare_for_main_config( conf) ;

	kik_conf_add_opt( conf , '@' , "screens" , 0 , "startup_screens" ,
		"number of screens to open in start up [1]") ;
	kik_conf_add_opt( conf , 'h' , "help" , 1 , "help" ,
		"show this help message") ;
	kik_conf_add_opt( conf , 'v' , "version" , 1 , "version" ,
		"show version message") ;
	kik_conf_add_opt( conf , 'P' , "ptys" , 0 , "startup_ptys" ,
		"number of ptys to open in start up [1]") ;
	kik_conf_add_opt( conf , 'R' , "fsrange" , 0 , "font_size_range" , 
		"font size range for GUI configurator [6-30]") ;
	kik_conf_add_opt( conf , 'W' , "sep" , 0 , "word_separators" , 
		"word-separating characters for double-click [,.:;/@]") ;
	kik_conf_add_opt( conf , 'Y' , "decsp" , 1 , "compose_dec_special_font" ,
		"compose dec special font [false]") ;
#ifdef  USE_TYPE_XFT
	kik_conf_add_opt( conf , 'c' , "cp932" , 1 , "use_cp932_ucs_for_xft" , 
		"use CP932-Unicode mapping table for JISX0208 [false]") ;
#endif
	kik_conf_add_opt( conf , 'i' , "xim" , 1 , "use_xim" , 
		"use XIM (X Input Method) [true]") ;
	kik_conf_add_opt( conf , 'j' , "daemon" , 0 , "daemon_mode" ,
		"start as a daemon (none/blend/genuine) [blend]") ;

	if( ! kik_conf_parse_args( conf , &argc , &argv))
	{
		kik_conf_delete( conf) ;

		return  0 ;
	}

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

	x_set_font_size_range( min_font_size , max_font_size) ;

	x_main_config_init( &main_config , conf , argc , argv) ;

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
		else if( strcmp( value , "none") == 0)
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

	if( ( value = kik_conf_get_value( conf , "click_interval")))
	{
		int  interval ;

		if( kik_str_to_int( &interval , value))
		{
			x_set_click_interval( interval) ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "compose_dec_special_font")))
	{
		if( strcmp( value , "true") == 0)
		{
			x_compose_dec_special_font() ;
		}
	}

#ifdef  USE_TYPE_XFT
	if( ( value = kik_conf_get_value( conf , "use_cp932_ucs_for_xft")) == NULL ||
		strcmp( value , "true") == 0)
	{
		ml_use_cp932_ucs_for_xft() ;
	}
#endif

	num_of_startup_screens = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "startup_screens")))
	{
		u_int  n ;
		
		if( ! kik_str_to_uint( &n , value) || ( ! is_genuine_daemon && n == 0))
		{
			kik_msg_printf( "startup_screens %s is not valid.\n" , value) ;
		}
		else
		{
			if( n > MAX_SCREENS)
			{
				n = MAX_SCREENS ;
			}
			
			num_of_startup_screens = n ;
		}
	}
	
	num_of_startup_ptys = 0 ;

	if( ( value = kik_conf_get_value( conf , "startup_ptys")))
	{
		u_int  n ;
		
		if( ! kik_str_to_uint( &n , value) || ( ! is_genuine_daemon && n == 0))
		{
			kik_msg_printf( "startup_ptys %s is not valid.\n" , value) ;
		}
		else
		{
			if( n <= num_of_startup_screens)
			{
				num_of_startup_ptys = 0 ;
			}
			else
			{
				if( n > MAX_SCREENS)
				{
					n = MAX_SCREENS ;
				}

				num_of_startup_ptys = n - num_of_startup_screens ;
			}
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
	system_listener.next_pty = next_pty ;
	system_listener.prev_pty = prev_pty ;
	system_listener.close_pty = NULL ;
	system_listener.pty_closed = pty_closed ;
	system_listener.get_pty = get_pty ;
	system_listener.pty_list = pty_list ;
	system_listener.config_saved = config_saved ;

	signal( SIGHUP , sig_fatal) ;
	signal( SIGINT , sig_fatal) ;
	signal( SIGQUIT , sig_fatal) ;
	signal( SIGTERM , sig_fatal) ;
	signal( SIGPIPE , SIG_IGN) ;

	ml_term_manager_init() ;
	
	kik_alloca_garbage_collect() ;
	
	for( i = 0 ; i < MAX_ADDTIONAL_FDS ; i++)
	{
		additional_fds[i].fd = -1 ;
		additional_fds[i].handler = NULL ;
	}

	return  1 ;
}

int
x_term_manager_final(void)
{
	int  count ;

	free( version) ;
	
	x_main_config_final( &main_config) ;
	
	ml_free_word_separators() ;
	
	for( count = 0 ; count < num_of_screens ; count ++)
	{
		close_screen_intern( screens[count]) ;
	}

	free( screens) ;

	ml_term_manager_final() ;

	x_display_close_all() ;

	x_xim_final() ;

	x_color_config_final( &color_config) ;
	
	x_shortcut_final( &shortcut) ;
	x_termcap_final( &termcap) ;

	kik_sig_child_final() ;
	
	return  1 ;
}

void
x_term_manager_event_loop(void)
{
	int  count ;
	const char  * display ;

	if( ! *( display = main_config.disp_name) && ( ! ( display = getenv( "DISPLAY")) || ! *display))
	{
		display = ":0.0" ;
	}

	for( count = 0 ; count < num_of_startup_screens ; count ++)
	{
		if( ! open_screen_intern(NULL))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " open_screen_intern() failed.\n") ;
		#endif

			if( count == 0 && ! is_genuine_daemon)
			{
				kik_msg_printf( "Unable to start - open_screen_intern() failed.\n") ;

				exit(1) ;
			}
			else
			{
				break ;
			}
		}
	}

	for( count = 0 ; count < num_of_startup_ptys ; count ++)
	{
		ml_term_t *  term ;

		if( ( term = create_term_intern()) == NULL)
		{
			break ;
		}

		if( ! open_pty_intern( term , main_config.cmd_path , main_config.cmd_argv ,
			display , 0 , main_config.term_type , main_config.use_login_shell))
		{
			return ;
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
			for( count = 0 ; count < num_of_screens ; count ++)
			{
				if( dead_mask & (0x1 << count))
				{
					close_screen_intern( screens[count]) ;
					
					screens[count] = screens[--num_of_screens] ;
				}
			}

			dead_mask = 0 ;
		}
		
		if( num_of_screens == 0 && ! is_genuine_daemon)
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

int
x_term_manager_add_fd(
	int fd ,
	void (*handler)(void)
	)
{
	int  i ;

	if( ! handler)
	{
		return  1 ;
	}

	for( i = 0 ; i < MAX_ADDTIONAL_FDS ; i++)
	{
		if( additional_fds[i].fd == -1)
		{
			additional_fds[i].fd = fd ;
			additional_fds[i].handler = handler ;

			return  0 ;
		}
	}

	return  1 ;
}

int
x_term_manager_remove_fd( int fd)
{
	int  i ;

	for( i = 0 ; i < MAX_ADDTIONAL_FDS ; i++)
	{
		if( additional_fds[i].fd == fd)
		{
			additional_fds[i].fd = -1 ;
			additional_fds[i].handler = NULL ;

			return  0 ;
		}
	}

	return  1 ;
}

