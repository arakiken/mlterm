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
#include  <fcntl.h>		/* creat */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* kik_str_sep/kik_str_to_int/kik_str_alloca_dup/strdup */
#include  <kiklib/kik_path.h>	/* kik_basename */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */
#include  <kiklib/kik_mem.h>	/* alloca/kik_alloca_garbage_collect/malloc/free */
#include  <kiklib/kik_conf.h>
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_locale.h>	/* kik_get_codeset */
#include  <kiklib/kik_net.h>	/* socket/bind/listen/sockaddr_un */
#include  <kiklib/kik_sig_child.h>
#include  <ml_term_factory.h>

#include  "version.h"
#include  "x_xim.h"
#include  "x_sb_screen.h"


/* --- static variables --- */

static char *  un_file ;


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

static x_display_t *
open_display(
	x_term_manager_t *  term_man
	)
{
	int  count ;
	x_display_t *  disp ;
	void *  p ;

	for( count = 0 ; count < term_man->num_of_displays ; count ++)
	{
		if( strcmp( term_man->displays[count]->name , term_man->conf.disp_name) == 0)
		{
			return  term_man->displays[count] ;
		}
	}

	if( ( disp = x_display_open( term_man->conf.disp_name)) == NULL)
	{
		return  NULL ;
	}

	if( ( p = realloc( term_man->displays ,
			sizeof( x_display_t*) * (term_man->num_of_displays + 1))) == NULL)
	{
		x_display_close( disp) ;

		return  NULL ;
	}

	term_man->displays = p ;
	term_man->displays[term_man->num_of_displays ++] = disp ;

	return  disp ;
}

static int
close_display(
	x_term_manager_t *  term_man ,
	x_display_t *  disp
	)
{
	int  count ;

	for( count = 0 ; count < term_man->num_of_displays ; count ++)
	{
		if( term_man->displays[count] == disp)
		{
			x_display_close( disp) ;
			term_man->displays[count] =
				term_man->displays[-- term_man->num_of_displays] ;

		#ifdef  DEBUG
			kik_debug_printf( "X connection closed.\n") ;
		#endif

			return  1 ;
		}
	}
	
	return  0 ;
}

static int
open_term(
	x_term_manager_t *  term_man
	)
{
	x_display_t *  disp ;
	x_screen_t *  screen ;
	x_sb_screen_t *  sb_screen ;
	x_font_manager_t *  font_man ;
	x_color_manager_t *  color_man ;
	x_window_t *  root ;
	mkf_charset_t  usascii_font_cs ;
	int  usascii_font_cs_changable ;
	x_termcap_entry_t *  termcap ;
	ml_term_t *  term ;
	char *  env[5] ;	/* MLTERM,TERM,WINDOWID,DISPLAY,NULL */
	char **  env_p ;
	char  wid_env[9 + DIGIT_STR_LEN(Window) + 1] ;	/* "WINDOWID="(9) + [32bit digit] + NULL(1) */
	char *  disp_env ;
	char *  disp_str ;
	char *  term_env ;
	char *  cmd_path ;
	char **  cmd_argv ;
	
	if( term_man->num_of_terms == term_man->max_terms
		/* block until dead_mask is cleared */
		|| term_man->dead_mask)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " busy.\n") ;
	#endif
	
		return  0 ;
	}

	/*
	 * these are dynamically allocated.
	 */
	disp = NULL ;
	font_man = NULL ;
	color_man = NULL ;
	screen = NULL ;
	sb_screen = NULL ;
	term = NULL ;

	termcap = x_termcap_get_entry( &term_man->termcap , term_man->conf.term_type) ;

	if( ( term = ml_attach_term( term_man->conf.cols , term_man->conf.rows ,
			term_man->conf.tab_size , term_man->conf.num_of_log_lines ,
			term_man->conf.encoding , term_man->conf.not_use_unicode_font ,
			term_man->conf.only_use_unicode_font , term_man->conf.col_size_a ,
			term_man->conf.use_char_combining , term_man->conf.use_multi_col_char ,
			x_termcap_get_bool_field( termcap , ML_BCE))) == NULL)
	{
		goto  error ;
	}
	
	if( ( disp = open_display( term_man)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " open_display failed.\n") ;
	#endif
	
		return  0 ;
	}

	if( term_man->conf.not_use_unicode_font || term_man->conf.iso88591_font_for_usascii)
	{
		usascii_font_cs = x_get_usascii_font_cs( ML_ISO8859_1) ;
		usascii_font_cs_changable = 0 ;
	}
	else if( term_man->conf.only_use_unicode_font)
	{
		usascii_font_cs = x_get_usascii_font_cs( ML_UTF8) ;
		usascii_font_cs_changable = 0 ;
	}
	else
	{
		usascii_font_cs = x_get_usascii_font_cs( term_man->conf.encoding) ;
		usascii_font_cs_changable = 1 ;
	}
	
	if( ( font_man = x_font_manager_new( disp->display ,
		&term_man->normal_font_custom , &term_man->v_font_custom , &term_man->t_font_custom ,
	#ifdef  ANTI_ALIAS
		&term_man->aa_font_custom , &term_man->vaa_font_custom , &term_man->taa_font_custom ,
	#else
		NULL , NULL , NULL ,
	#endif
		term_man->conf.font_present , term_man->conf.font_size ,
		usascii_font_cs , usascii_font_cs_changable ,
		term_man->conf.use_multi_col_char ,
		term_man->conf.step_in_changing_font_size)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_font_manager_new() failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( ( color_man = x_color_manager_new( disp->display ,
				DefaultScreen( disp->display) , &term_man->color_custom ,
				term_man->conf.fg_color , term_man->conf.bg_color)) == NULL)
	{
		goto  error ;
	}

	if( ( screen = x_screen_new( term , font_man , color_man , termcap ,
			term_man->conf.brightness ,
			term_man->conf.fade_ratio , &term_man->keymap ,
			term_man->conf.screen_width_ratio , term_man->conf.screen_height_ratio ,
			term_man->conf.xim_open_in_startup , term_man->conf.mod_meta_mode ,
			term_man->conf.bel_mode , term_man->conf.copy_paste_via_ucs ,
			term_man->conf.pic_file_path , term_man->conf.use_transbg ,
			term_man->conf.use_bidi , term_man->conf.vertical_mode ,
			term_man->conf.use_vertical_cursor ,
			term_man->conf.big5_buggy , term_man->conf.conf_menu_path ,
			term_man->conf.iscii_lang , term_man->conf.use_extended_scroll_shortcut ,
			term_man->conf.use_dynamic_comb , term_man->conf.line_space)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_screen_new() failed.\n") ;
	#endif

		goto  error ;
	}

	if( ! x_set_system_listener( screen , &term_man->system_listener))
	{
		goto  error ;
	}

	if( term_man->conf.use_scrollbar)
	{
		if( ( sb_screen = x_sb_screen_new( screen ,
					term_man->conf.scrollbar_view_name ,
					term_man->conf.sb_fg_color , term_man->conf.sb_bg_color ,
					term_man->conf.sb_mode)) == NULL)
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
		term_man->conf.x , term_man->conf.y , term_man->conf.geom_hint))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_window_manager_show_root() failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( term_man->conf.app_name)
	{
		x_set_window_name( &screen->window , term_man->conf.app_name) ;
		x_set_icon_name( &screen->window , term_man->conf.icon_name) ;
	}
	else
	{
		if( term_man->conf.title)
		{
			x_set_window_name( &screen->window , term_man->conf.title) ;
		}
		
		if( term_man->conf.icon_name)
		{
			x_set_icon_name( &screen->window , term_man->conf.icon_name) ;
		}
	}
	
	env_p = env ;

	if( term_man->version)
	{
		*(env_p ++) = term_man->version ;
	}
	
	sprintf( wid_env , "WINDOWID=%ld" , root->my_window) ;
	*(env_p ++) = wid_env ;
	
	disp_str = XDisplayString( disp->display) ;

	/* "DISPLAY="(8) + NULL(1) */
	if( ( disp_env = alloca( 8 + strlen( disp_str) + 1)))
	{
		sprintf( disp_env , "DISPLAY=%s" , disp_str) ;
		*(env_p ++) = disp_env ;
	}
	else
	{
		*(env_p ++) = NULL ;
	}

	/* "TERM="(5) + NULL(1) */
	if( ( term_env = alloca( 5 + strlen( term_man->conf.term_type) + 1)))
	{
		sprintf( term_env , "TERM=%s" , term_man->conf.term_type) ;
		*(env_p ++) = term_env ;
	}

	/* NULL terminator */
	*env_p = NULL ;

	if( term_man->conf.cmd_path && term_man->conf.cmd_argv)
	{
		cmd_path = term_man->conf.cmd_path ;
		cmd_argv = term_man->conf.cmd_argv ;
	}
	else
	{
		struct passwd *  pw ;
		char *  cmd_file ;

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
		
		if( ( cmd_argv = alloca( sizeof( char*) * 2)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
		#endif
		
			goto  error ;
		}

		cmd_file = kik_basename( cmd_path) ;

		/* 2 = `-' and NULL */
		if( ( cmd_argv[0] = alloca( strlen( cmd_file) + 2)) == NULL)
		{
			goto  error ;
		}

		if( term_man->conf.use_login_shell)
		{
			sprintf( cmd_argv[0] , "-%s" , cmd_file) ;
		}
		else
		{
			strcpy( cmd_argv[0] , cmd_file) ;
		}

		cmd_argv[1] = NULL ;
	}

	if( ! ml_term_open_pty( term , cmd_path , cmd_argv , env , disp_str))
	{
		goto  error ;
	}

	term_man->terms[term_man->num_of_terms].display = disp ;
	term_man->terms[term_man->num_of_terms].root_window = root ;
	term_man->terms[term_man->num_of_terms].font_man = font_man ;
	term_man->terms[term_man->num_of_terms].color_man = color_man ;
	term_man->terms[term_man->num_of_terms].term = term ;

	term_man->num_of_terms ++ ;

	return  1 ;
	
error:
	if( disp)
	{
		close_display( term_man , disp) ;
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
		ml_term_delete( term) ;
	}

	return  0 ;
}

static int
close_term(
	x_term_t *  term
	)
{
	x_window_manager_remove_root( &term->display->win_man , term->root_window) ;
	x_font_manager_delete( term->font_man) ;
	x_color_manager_delete( term->color_man) ;

	if( term->term)
	{
		ml_term_delete( term->term) ;
	}
	
	return  1 ;
}

static int
close_dead_terms(
	x_term_manager_t *  term_man
	)
{
	int  count ;
	
	for( count = term_man->max_terms - 1 ; count >= 0 ; count --)
	{
		if( term_man->dead_mask & (0x1 << count))
		{
			if( -- term_man->num_of_terms == 0 && ! term_man->is_genuine_daemon)
			{
				if( un_file)
				{
					unlink( un_file) ;
				}
				
				exit( 0) ;
			}

			close_term( &term_man->terms[count]) ;
			
			if( term_man->terms[count].display->win_man.num_of_roots == 0)
			{
				close_display( term_man , term_man->terms[count].display) ;
			}

			term_man->terms[count] = term_man->terms[term_man->num_of_terms] ;
		}
	}
	
	term_man->dead_mask = 0 ;
	
	return  1 ;
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
	x_term_manager_t *  term_man ;

	term_man = p ;

	x_term_manager_final( term_man) ;
	
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
	void *  p
	)
{
	x_term_manager_t *  term_man ;

	term_man = p ;
	
	if( ! open_term( term_man))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " open_term() failed.\n") ;
	#endif
	}
}

static void
close_pty(
	void *  p ,
	x_window_t *  root_window
	)
{
	int  count ;
	x_term_manager_t *  term_man ;

	term_man = p ;
	
	for( count = 0 ; count < term_man->num_of_terms ; count ++)
	{
		if( root_window == term_man->terms[count].root_window)
		{
		#if  0
			kill( ml_term_get_child_pid( term_man->terms[count].term) , SIGKILL) ;
		#else
			term_man->dead_mask |= (1 << count) ;
			
			ml_detach_term( term_man->terms[count].term) ;
			term_man->terms[count].term = NULL ;
		#endif
		
			break ;
		}
	}
}


/*
 * signal handlers.
 */
 
static void
sig_child(
	void *  p ,
	pid_t  pid
	)
{
	x_term_manager_t *  term_man ;
	int  count ;

	term_man = p ;
	
	for( count = 0 ; count < term_man->num_of_terms ; count ++)
	{
		if( pid == ml_term_get_child_pid( term_man->terms[count].term))
		{
			term_man->dead_mask |= (1 << count) ;

			return ;
		}
	}
}

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
start_daemon(
	x_term_manager_t *  term_man
	)
{
	int  fd ;
	char *  path = "/tmp/mlterm.unix" ;
	pid_t  pid ;
	int  sock_fd ;
	struct sockaddr_un  servaddr ;

	if( ( fd = creat( path , 0600)) == -1)
	{
		/* already exists */

		kik_msg_printf( "remove %s before starting daemon.\n" , path) ;

		return  -1 ;
	}

	close( fd) ;
	
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

	if( ( sock_fd = socket( AF_LOCAL , SOCK_STREAM , 0)) < 0)
	{
		return  -1 ;
	}

	unlink( path) ;

	memset( &servaddr , 0 , sizeof( servaddr)) ;
	servaddr.sun_family = AF_LOCAL ;
	strcpy( servaddr.sun_path , path) ;

	if( bind( sock_fd , (struct sockaddr *) &servaddr , sizeof( servaddr)) < 0)
	{
		close( sock_fd) ;
	
		return  -1 ;
	}
	
	if( listen( sock_fd , 1024) < 0)
	{
		close( sock_fd) ;
		
		return  -1 ;
	}

	un_file = path ;

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
	kik_conf_add_opt( conf , '5' , "big5bug" , 1 , "big5_buggy" ,
		"manage buggy Big5 CTEXT in XFree86 4.1 or earlier [false]") ;
	kik_conf_add_opt( conf , '7' , "bel" , 0 , "bel_mode" , 
		"bel (0x07) mode [none/sound/visual, default = none]") ;
	kik_conf_add_opt( conf , '8' , "88591" , 1 , "iso88591_font_for_usascii" ,
		"use ISO-8859-1 font for ASCII part of any encoding [false]") ;
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
	kik_conf_add_opt( conf , 'H' , "bright" , 0 , "brightness" ,
		"brightness of background image in percent [100]") ;
	kik_conf_add_opt( conf , 'I' , "icon" , 0 , "icon_name" , 
		"icon name") ;
	kik_conf_add_opt( conf , 'J' , "dyncomb" , 1 , "use_dynamic_comb" ,
		"use dynamic combining [false]") ;
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
	kik_conf_add_opt( conf , 'U' , "viaucs" , 1 , "copy_paste_via_ucs" ,
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
		"size (in characters) and position [80x30]") ;
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
	x_term_manager_t *  term_man ,
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

	if( ( term_man->conf.disp_name = strdup( value)) == NULL)
	{
		return  0 ;
	}
	
	if( ( value = kik_conf_get_value( conf , "fontsize")) == NULL)
	{
		term_man->conf.font_size = 16 ;
	}
	else if( ! kik_str_to_uint( &term_man->conf.font_size , value))
	{
		kik_msg_printf( "font size %s is not valid.\n" , value) ;

		/* default value is used. */
		term_man->conf.font_size = 16 ;
	}

	if( term_man->conf.font_size > term_man->normal_font_custom.max_font_size)
	{
		kik_msg_printf( "font size %d is too large. %d is used.\n" ,
			term_man->conf.font_size , term_man->normal_font_custom.max_font_size) ;
		
		term_man->conf.font_size = term_man->normal_font_custom.max_font_size ;
	}
	else if( term_man->conf.font_size < term_man->normal_font_custom.min_font_size)
	{
		kik_msg_printf( "font size %d is too small. %d is used.\n" ,
			term_man->conf.font_size , term_man->normal_font_custom.min_font_size) ;
			
		term_man->conf.font_size = term_man->normal_font_custom.min_font_size ;
	}

	term_man->conf.app_name = NULL ;

	if( ( value = kik_conf_get_value( conf , "app_name")))
	{
		term_man->conf.app_name = strdup( value) ;
	}

	term_man->conf.title = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "title")))
	{
		term_man->conf.title = strdup( value) ;
	}

	term_man->conf.icon_name = NULL ;

	if( ( value = kik_conf_get_value( conf , "icon_name")))
	{
		term_man->conf.icon_name = strdup( value) ;
	}
	
	term_man->conf.conf_menu_path = NULL ;

	if( ( value = kik_conf_get_value( conf , "conf_menu_path")))
	{
		term_man->conf.conf_menu_path = strdup( value) ;
	}

	/* use default value */
	term_man->conf.scrollbar_view_name = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "scrollbar_view_name")))
	{
		term_man->conf.scrollbar_view_name = strdup( value) ;
	}

	term_man->conf.use_char_combining = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "use_combining")))
	{
		if( strcmp( value , "false") == 0)
		{
			term_man->conf.use_char_combining = 0 ;
		}
	}

	term_man->conf.use_dynamic_comb = 0 ;
	if( ( value = kik_conf_get_value( conf , "use_dynamic_comb")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->conf.use_dynamic_comb = 1 ;
		}
	}

	term_man->conf.font_present = 0 ;

	if( ( value = kik_conf_get_value( conf , "use_variable_column_width")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->conf.font_present |= FONT_VAR_WIDTH ;
		}
	}

	term_man->conf.step_in_changing_font_size = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "step_in_changing_font_size")))
	{
		u_int  size ;
		
		if( kik_str_to_uint( &size , value))
		{
			term_man->conf.step_in_changing_font_size = size ;
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
			term_man->conf.font_present |= FONT_AA ;
		}
	}
#endif

	term_man->conf.vertical_mode = 0 ;
	
	if( ( value = kik_conf_get_value( conf , "vertical_mode")))
	{
		if( ( term_man->conf.vertical_mode = ml_get_vertical_mode( value)))
		{
			/*
			 * vertical font is automatically used under vertical mode.
			 * similler processing is done in x_screen.c:change_vertical_mode.
			 */
			term_man->conf.font_present |= FONT_VERTICAL ;
			term_man->conf.font_present &= ~FONT_VAR_WIDTH ;
		}
	}

	term_man->conf.fg_color = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "fg_color")))
	{
		term_man->conf.fg_color = strdup( value) ;
	}

	term_man->conf.bg_color = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "bg_color")))
	{
		term_man->conf.bg_color = strdup( value) ;
	}

	term_man->conf.sb_fg_color = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "sb_fg_color")))
	{
		term_man->conf.sb_fg_color = strdup( value) ;
	}

	term_man->conf.sb_bg_color = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "sb_bg_color")))
	{
		term_man->conf.sb_bg_color = strdup( value) ;
	}
	
	if( ( value = kik_conf_get_value( conf , "termtype")))
	{
		term_man->conf.term_type = strdup( value) ;
	}
	else
	{
		term_man->conf.term_type = strdup( "xterm") ;
	}

	term_man->conf.x = 0 ;
	term_man->conf.y = 0 ;
	term_man->conf.cols = 80 ;
	term_man->conf.rows = 30 ;
	if( ( value = kik_conf_get_value( conf , "geometry")))
	{
		/* For each value not found, the argument is left unchanged.(see man XParseGeometry(3)) */
		term_man->conf.geom_hint = XParseGeometry( value , &term_man->conf.x , &term_man->conf.y ,
						&term_man->conf.cols , &term_man->conf.rows) ;

		if( term_man->conf.cols == 0 || term_man->conf.rows == 0)
		{
			kik_msg_printf( "geometry option %s is illegal.\n" , value) ;
			
			term_man->conf.cols = 80 ;
			term_man->conf.rows = 30 ;
		}
	}
	else
	{
		term_man->conf.geom_hint = 0 ;
	}

	term_man->conf.screen_width_ratio = 100 ;
	
	if( ( value = kik_conf_get_value( conf , "screen_width_ratio")))
	{
		u_int  ratio ;

		if( kik_str_to_uint( &ratio , value))
		{
			term_man->conf.screen_width_ratio = ratio ;
		}
	}

	term_man->conf.screen_height_ratio = 100 ;
	
	if( ( value = kik_conf_get_value( conf , "screen_height_ratio")))
	{
		u_int  ratio ;

		if( kik_str_to_uint( &ratio , value))
		{
			term_man->conf.screen_height_ratio = ratio ;
		}
	}
	
	term_man->conf.use_multi_col_char = 1 ;

	if( ( value = kik_conf_get_value( conf , "use_multi_column_char")))
	{
		if( strcmp( value , "false") == 0)
		{
			term_man->conf.use_multi_col_char = 0 ;
		}
	}

	term_man->conf.line_space = 0 ;

	if( ( value = kik_conf_get_value( conf , "line_space")))
	{
		u_int  size ;

		if( kik_str_to_uint( &size , value))
		{
			term_man->conf.line_space = size ;
		}
		else
		{
			kik_msg_printf( "line space %s is not valid.\n" , value) ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "logsize")) == NULL)
	{
		term_man->conf.num_of_log_lines = 128 ;
	}
	else if( ! kik_str_to_uint( &term_man->conf.num_of_log_lines , value))
	{
		kik_msg_printf( "log size %s is not valid.\n" , value) ;

		/* default value is used. */
		term_man->conf.num_of_log_lines = 128 ;
	}

	if( ( value = kik_conf_get_value( conf , "tabsize")) == NULL)
	{
		/* default value is used. */
		term_man->conf.tab_size = 8 ;
	}
	else if( ! kik_str_to_uint( &term_man->conf.tab_size , value))
	{
		kik_msg_printf( "tab size %s is not valid.\n" , value) ;

		/* default value is used. */
		term_man->conf.tab_size = 8 ;
	}
	
	term_man->conf.use_login_shell = 0 ;
	
	if( ( value = kik_conf_get_value( conf , "use_login_shell")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->conf.use_login_shell = 1 ;
		}
	}

	term_man->conf.big5_buggy = 0 ;

	if( ( value = kik_conf_get_value( conf , "big5_buggy")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->conf.big5_buggy = 1 ;
		}
	}

	term_man->conf.use_scrollbar = 1 ;

	if( ( value = kik_conf_get_value( conf , "use_scrollbar")))
	{
		if( strcmp( value , "false") == 0)
		{
			term_man->conf.use_scrollbar = 0 ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "scrollbar_mode")))
	{
		term_man->conf.sb_mode = x_get_sb_mode( value) ;
	}
	else
	{
		term_man->conf.sb_mode = SB_LEFT ;
	}

	term_man->conf.iso88591_font_for_usascii = 0 ;

	if( ( value = kik_conf_get_value( conf , "iso88591_font_for_usascii")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->conf.iso88591_font_for_usascii = 1 ;
		}
	}

	term_man->conf.not_use_unicode_font = 0 ;

	if( ( value = kik_conf_get_value( conf , "not_use_unicode_font")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->conf.not_use_unicode_font = 1 ;
		}
	}

	term_man->conf.only_use_unicode_font = 0 ;

	if( ( value = kik_conf_get_value( conf , "only_use_unicode_font")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->conf.only_use_unicode_font = 1 ;
		}
	}

	if( term_man->conf.only_use_unicode_font && term_man->conf.not_use_unicode_font)
	{
		kik_msg_printf(
			"only_use_unicode_font and not_use_unicode_font options cannot be used "
			"at the same time.\n") ;

		/* default values are used */
		term_man->conf.only_use_unicode_font = 0 ;
		term_man->conf.not_use_unicode_font = 0 ;
	}

	term_man->conf.copy_paste_via_ucs = 0 ;

	if( ( value = kik_conf_get_value( conf , "copy_paste_via_ucs")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->conf.copy_paste_via_ucs = 1 ;
		}
	}
	
	/* default value is used */
	term_man->conf.col_size_a = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "col_size_of_width_a")))
	{
		u_int  col_size_a ;
		
		if( kik_str_to_uint( &col_size_a , value))
		{
			term_man->conf.col_size_a = col_size_a ;
		}
		else
		{
			kik_msg_printf( "col size of width a %s is not valid.\n" , value) ;
		}
	}

	term_man->conf.pic_file_path = NULL ;

	if( ( value = kik_conf_get_value( conf , "wall_picture")))
	{
		term_man->conf.pic_file_path = strdup( value) ;
	}

	term_man->conf.use_transbg = 0 ;

	if( ( value = kik_conf_get_value( conf , "use_transbg")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->conf.use_transbg = 1 ;
		}
	}

	if( term_man->conf.pic_file_path && term_man->conf.use_transbg)
	{
		kik_msg_printf(
			"wall picture and transparent background cannot be used at the same time.\n") ;

		/* using wall picture */
		term_man->conf.use_transbg = 0 ;
	}

	term_man->conf.brightness = 100 ;

	if( ( value = kik_conf_get_value( conf , "brightness")))
	{
		u_int  brightness ;
		
		if( kik_str_to_uint( &brightness , value))
		{
			term_man->conf.brightness = brightness ;
		}
		else
		{
			kik_msg_printf( "shade ratio %s is not valid.\n" , value) ;
		}
	}
	
	term_man->conf.fade_ratio = 100 ;
	
	if( ( value = kik_conf_get_value( conf , "fade_ratio")))
	{
		u_int  fade_ratio ;
		
		if( kik_str_to_uint( &fade_ratio , value) && fade_ratio <= 100)
		{
			term_man->conf.fade_ratio = fade_ratio ;
		}
		else
		{
			kik_msg_printf( "fade ratio %s is not valid.\n" , value) ;
		}
	}

	if( ( term_man->conf.encoding = ml_get_char_encoding( kik_get_codeset())) == ML_UNKNOWN_ENCODING)
	{
		term_man->conf.encoding = ML_ISO8859_1 ;
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
				term_man->conf.encoding = encoding ;
			}
		}
	}

	term_man->conf.xim_open_in_startup = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "xim_open_in_startup")))
	{
		if( strcmp( value , "false") == 0)
		{
			term_man->conf.xim_open_in_startup = 0 ;
		}
	}

	term_man->conf.use_bidi = 1 ;

	if( ( value = kik_conf_get_value( conf , "use_bidi")))
	{
		if( strcmp( value , "false") == 0)
		{
			term_man->conf.use_bidi = 0 ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "mod_meta_mode")))
	{
		term_man->conf.mod_meta_mode = x_get_mod_meta_mode( value) ;
	}
	else
	{
		term_man->conf.mod_meta_mode = MOD_META_NONE ;
	}

	if( ( value = kik_conf_get_value( conf , "bel_mode")))
	{
		term_man->conf.bel_mode = x_get_bel_mode( value) ;
	}
	else
	{
		term_man->conf.bel_mode = BEL_SOUND ;
	}

	term_man->conf.use_vertical_cursor = 0 ;

	if( ( value = kik_conf_get_value( conf , "use_vertical_cursor")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->conf.use_vertical_cursor = 1 ;
		}
	}
	
	term_man->conf.iscii_lang = ISCIILANG_MALAYALAM ;
	
	if( ( value = kik_conf_get_value( conf , "iscii_lang")))
	{
		ml_iscii_lang_t  lang ;
		
		if( ( lang = ml_iscii_get_lang( strdup( value))) != ISCIILANG_UNKNOWN)
		{
			term_man->conf.iscii_lang = lang ;
		}
	}

	term_man->conf.use_extended_scroll_shortcut = 0 ;
	
	if( ( value = kik_conf_get_value( conf , "use_extended_scroll_shortcut")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->conf.use_extended_scroll_shortcut = 1 ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "exec_cmd")) && strcmp( value , "true") == 0)
	{
		if( ( term_man->conf.cmd_argv = malloc( sizeof( char*) * (argc + 1))) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
		#endif
		
			term_man->conf.cmd_path = NULL ;
			term_man->conf.cmd_argv = NULL ;
		}
		else
		{
			/*
			 * !! Notice !!
			 * cmd_path and strings in cmd_argv vector should be allocated
			 * by the caller.
			 */
			 
			term_man->conf.cmd_path = argv[0] ;
			
			memcpy( &term_man->conf.cmd_argv[0] , argv , sizeof( char*) * argc) ;
			term_man->conf.cmd_argv[argc] = NULL ;
		}
	}
	else
	{
		term_man->conf.cmd_path = NULL ;
		term_man->conf.cmd_argv = NULL ;
	}
	
	return  1 ;
}

static int
config_final(
	x_term_manager_t *  term_man
	)
{
	free( term_man->conf.disp_name) ;
	free( term_man->conf.app_name) ;
	free( term_man->conf.title) ;
	free( term_man->conf.icon_name) ;
	free( term_man->conf.term_type) ;
	free( term_man->conf.conf_menu_path) ;
	free( term_man->conf.pic_file_path) ;
	free( term_man->conf.scrollbar_view_name) ;
	free( term_man->conf.fg_color) ;
	free( term_man->conf.bg_color) ;
	free( term_man->conf.sb_fg_color) ;
	free( term_man->conf.sb_bg_color) ;
	free( term_man->conf.cmd_argv) ;

	return  1 ;
}

static void
client_connected(
	x_term_manager_t *  term_man
	)
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
	struct x_config  orig_conf ;

	if( ( fd = accept( term_man->sock_fd , (struct sockaddr *) &addr , &sock_len)) < 0)
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
	
	orig_conf = term_man->conf ;

	config_init( term_man , conf , argc , argv) ;

	kik_conf_delete( conf) ;
	
	if( ! open_term( term_man))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " open_term() failed.\n") ;
	#endif
	}

	config_final( term_man) ;

	term_man->conf = orig_conf ;
}

static void
receive_next_event(
	x_term_manager_t *  term_man
	)
{
	int  count ;
	int  xfd ;
	int  ptyfd ;
	int  maxfd ;
	int  ret ;
	fd_set  read_fds ;
	struct timeval  tval ;
	ml_term_t **  detached_terms ;
	u_int  num_of_detached ;

	num_of_detached = ml_get_detached_terms( &detached_terms) ;
	
	/*
	 * flush buffer
	 */

	for( count = 0 ; count < term_man->num_of_terms ; count ++)
	{
		ml_term_flush( term_man->terms[count].term) ;
	}
	
	while( 1)
	{
		/* on Linux tv_usec,tv_sec members are zero cleared after select() */
		tval.tv_usec = 50000 ;	/* 0.05 sec */
		tval.tv_sec = 0 ;

		maxfd = 0 ;
		FD_ZERO( &read_fds) ;

		for( count = 0 ; count < term_man->num_of_displays ; count ++)
		{
			/*
			 * it is necessary to flush events here since some events
			 * may have happened in idling
			 */
			x_window_manager_receive_next_event( &term_man->displays[count]->win_man) ;

			xfd = x_display_fd( term_man->displays[count]) ;
			
			FD_SET( xfd , &read_fds) ;
		
			if( xfd > maxfd)
			{
				maxfd = xfd ;
			}
		}

		for( count = 0 ; count < term_man->num_of_terms ; count ++)
		{
			ptyfd = ml_term_get_pty_fd( term_man->terms[count].term) ;
			FD_SET( ptyfd , &read_fds) ;

			if( ptyfd > maxfd)
			{
				maxfd = ptyfd ;
			}
		}
		
		for( count = 0 ; count < num_of_detached ; count ++)
		{
			ptyfd = ml_term_get_pty_fd( detached_terms[count]) ;
			FD_SET( ptyfd , &read_fds) ;

			if( ptyfd > maxfd)
			{
				maxfd = ptyfd ;
			}
		}
		
		if( term_man->sock_fd >= 0)
		{
			FD_SET( term_man->sock_fd , &read_fds) ;
			
			if( term_man->sock_fd > maxfd)
			{
				maxfd = term_man->sock_fd ;
			}
		}

		if( ( ret = select( maxfd + 1 , &read_fds , NULL , NULL , &tval)) != 0)
		{
			break ;
		}

		for( count = 0 ; count < term_man->num_of_displays ; count ++)
		{
			x_window_manager_idling( &term_man->displays[count]->win_man) ;
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
	 
	for( count = 0 ; count < term_man->num_of_terms ; count ++)
	{
		if( FD_ISSET( ml_term_get_pty_fd( term_man->terms[count].term) , &read_fds))
		{
			ml_term_parse_vt100_sequence( term_man->terms[count].term) ;
		}
	}

	for( count = 0 ; count < num_of_detached ; count ++)
	{
		if( FD_ISSET( ml_term_get_pty_fd( detached_terms[count]) , &read_fds))
		{
			ml_term_parse_vt100_sequence( detached_terms[count]) ;
		}
	}
	
	for( count = 0 ; count < term_man->num_of_displays ; count ++)
	{
		if( FD_ISSET( x_display_fd( term_man->displays[count]) , &read_fds))
		{
			x_window_manager_receive_next_event( &term_man->displays[count]->win_man) ;
		}
	}

	if( term_man->sock_fd >= 0)
	{
		if( FD_ISSET( term_man->sock_fd , &read_fds))
		{
			client_connected( term_man) ;
		}
	}
}


/* --- global functions --- */

int
x_term_manager_init(
	x_term_manager_t *  term_man ,
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

	kik_conf_add_opt( conf , 'K' , "maxptys" , 0 , "max_ptys" ,
		"max ptys to use [5]") ;
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

	term_man->is_genuine_daemon = 0 ;
	term_man->sock_fd = -1 ;
	
	if( ( value = kik_conf_get_value( conf , "daemon_mode")))
	{
		if( strcmp( value , "genuine") == 0)
		{
			if( ( term_man->sock_fd = start_daemon( term_man)) < 0)
			{
				kik_msg_printf( "mlterm failed to become daemon.\n") ;
			}
			else
			{
				term_man->is_genuine_daemon = 1 ;
			}
		}
		else if( strcmp( value , "blend") == 0)
		{
			if( ( term_man->sock_fd = start_daemon( term_man)) < 0)
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

	if( ! x_font_custom_init( &term_man->normal_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	font_rcfile = "mlterm/font" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &term_man->normal_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &term_man->normal_font_custom , rcpath) ;

		free( rcpath) ;
	}
	
	if( ! x_font_custom_init( &term_man->v_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	font_rcfile = "mlterm/vfont" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &term_man->v_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &term_man->v_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ! x_font_custom_init( &term_man->t_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	font_rcfile = "mlterm/tfont" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &term_man->t_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &term_man->t_font_custom , rcpath) ;

		free( rcpath) ;
	}

#ifdef  ANTI_ALIAS
	if( ! x_font_custom_init( &term_man->aa_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	font_rcfile = "mlterm/aafont" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &term_man->aa_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &term_man->aa_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ! x_font_custom_init( &term_man->vaa_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	font_rcfile = "mlterm/vaafont" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &term_man->vaa_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &term_man->vaa_font_custom , rcpath) ;

		free( rcpath) ;
	}
	
	if( ! x_font_custom_init( &term_man->taa_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	font_rcfile = "mlterm/taafont" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &term_man->taa_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		x_font_custom_read_conf( &term_man->taa_font_custom , rcpath) ;

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

	if( ! x_color_custom_init( &term_man->color_custom))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_color_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}
	
	if( ( rcpath = kik_get_sys_rc_path( "mlterm/color")))
	{
		x_color_custom_read_conf( &term_man->color_custom , rcpath) ;

		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( "mlterm/color")))
	{
		x_color_custom_read_conf( &term_man->color_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ! x_keymap_init( &term_man->keymap))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_keymap_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	if( ( rcpath = kik_get_sys_rc_path( "mlterm/key")))
	{
		x_keymap_read_conf( &term_man->keymap , rcpath) ;

		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( "mlterm/key")))
	{
		x_keymap_read_conf( &term_man->keymap , rcpath) ;

		free( rcpath) ;
	}

	if( ! x_termcap_init( &term_man->termcap))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_termcap_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	if( ( rcpath = kik_get_sys_rc_path( "mlterm/termcap")))
	{
		x_termcap_read_conf( &term_man->termcap , rcpath) ;

		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( "mlterm/termcap")))
	{
		x_termcap_read_conf( &term_man->termcap , rcpath) ;

		free( rcpath) ;
	}
	
	/*
	 * others
	 */

	term_man->max_terms = 5 ;

	if( ( value = kik_conf_get_value( conf , "max_ptys")))
	{
		u_int  max ;

		/*
		 * max_ptys is 1 - 32.
		 * 32 is the limit of dead_mask(32bit).
		 */
		if( ! kik_str_to_uint( &max , value) || max == 0 || max > 32)
		{
			kik_msg_printf( "max ptys %s is not valid.\n" , value) ;
		}
		else
		{
			term_man->max_terms = max ;
		}
	}

	term_man->num_of_startup_terms = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "ptys")))
	{
		u_int  ptys ;
		
		if( ! kik_str_to_uint( &ptys , value) || ptys > term_man->max_terms ||
			( ! term_man->is_genuine_daemon && ptys == 0))
		{
			kik_msg_printf( "ptys %s is not valid.\n" , value) ;
		}
		else
		{
			term_man->num_of_startup_terms = ptys ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "word_separators")))
	{
		ml_set_word_separators( value) ;
	}

	if( ( term_man->version = kik_conf_get_version( conf)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " version string is NULL\n") ;
	#endif
	}

	config_init( term_man , conf , argc , argv) ;

	kik_conf_delete( conf) ;

	if( *term_man->conf.disp_name)
	{
		/*
		 * setting DISPLAY environment variable to match "--display" option.
		 */
		
		char *  env ;

		if( ( env = malloc( strlen( term_man->conf.disp_name) + 9)))
		{
			sprintf( env , "DISPLAY=%s" , term_man->conf.disp_name) ;
			putenv( env) ;
		}
	}

	term_man->displays = NULL ;
	term_man->num_of_displays = 0 ;
	
	if( ( term_man->terms = malloc( sizeof( x_term_t) * term_man->max_terms)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif
		
		return  0 ;
	}

	term_man->num_of_terms = 0 ;
	term_man->dead_mask = 0 ;

	term_man->system_listener.self = term_man ;
	term_man->system_listener.exit = __exit ;
	term_man->system_listener.open_pty = open_pty ;
	term_man->system_listener.close_pty = close_pty ;

	signal( SIGHUP , sig_fatal) ;
	signal( SIGINT , sig_fatal) ;
	signal( SIGQUIT , sig_fatal) ;
	signal( SIGTERM , sig_fatal) ;

	kik_sig_child_init() ;
	kik_add_sig_child_listener( term_man , sig_child) ;
	
	kik_alloca_garbage_collect() ;
	
	return  1 ;
}

int
x_term_manager_final(
	x_term_manager_t *  term_man
	)
{
	int  count ;

	kik_remove_sig_child_listener( term_man) ;
	
	free( term_man->version) ;
	
	config_final( term_man) ;
	
	ml_free_word_separators() ;
	
	for( count = 0 ; count < term_man->num_of_terms ; count ++)
	{
		close_term( &term_man->terms[count]) ;
	}

	free( term_man->terms) ;

	for( count = 0 ; count < term_man->num_of_displays ; count ++)
	{
		x_display_close( term_man->displays[count]) ;
	}

	free( term_man->displays) ;

	x_xim_final() ;
	
	x_font_custom_final( &term_man->normal_font_custom) ;
	x_font_custom_final( &term_man->v_font_custom) ;
	x_font_custom_final( &term_man->t_font_custom) ;
#ifdef  ANTI_ALIAS
	x_font_custom_final( &term_man->aa_font_custom) ;
	x_font_custom_final( &term_man->vaa_font_custom) ;
	x_font_custom_final( &term_man->taa_font_custom) ;
#endif

	x_color_custom_final( &term_man->color_custom) ;
	
	x_keymap_final( &term_man->keymap) ;
	x_termcap_final( &term_man->termcap) ;

	kik_sig_child_final() ;
	
	return  1 ;
}

void
x_term_manager_event_loop(
	x_term_manager_t *  term_man
	)
{
	int  count ;

	for( count = 0 ; count < term_man->num_of_startup_terms ; count ++)
	{
		if( ! open_term( term_man))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " open_term() failed.\n") ;
		#endif

			if( count == 0 && ! term_man->is_genuine_daemon)
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
		kik_alloca_begin_stack_frame() ;

		receive_next_event( term_man) ;

		if( term_man->dead_mask)
		{
			close_dead_terms( term_man) ;
		}

		kik_alloca_end_stack_frame() ;
	}
}
