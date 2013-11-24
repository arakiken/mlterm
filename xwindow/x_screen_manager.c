/*
 *	$Id$
 */

#include  "x_screen_manager.h"

#include  <stdio.h>		/* sprintf */
#include  <string.h>		/* memset/memcpy */
#include  <stdlib.h>		/* getenv */
#include  <unistd.h>		/* getuid */

#include  <kiklib/kik_def.h>	/* USE_WIN32API */
#ifndef  USE_WIN32API
#include  <pwd.h>		/* getpwuid */
#endif

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* kik_str_sep/kik_str_to_int/kik_str_alloca_dup/strdup */
#include  <kiklib/kik_path.h>	/* kik_basename */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */
#include  <kiklib/kik_mem.h>	/* alloca/kik_alloca_garbage_collect/malloc/free */
#include  <kiklib/kik_conf.h>
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_types.h>	/* u_int */
#include  <kiklib/kik_args.h>	/* kik_arg_str_to_array */
#include  <kiklib/kik_sig_child.h>
#include  <ml_term_manager.h>
#include  <ml_char_encoding.h>

#include  "x_sb_screen.h"
#include  "x_display.h"
#include  "x_termcap.h"

#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
#include  "x_connect_dialog.h"
#endif

#define  MAX_SCREENS  (MSU * max_screens_multiple)	/* Default MAX_SCREENS is 32. */
#define  MSU          (8 * sizeof(dead_mask[0]))	/* MAX_SCREENS_UNIT */

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  mlterm_version ;

static u_int  max_screens_multiple ;
static u_int32_t *  dead_mask ;

static x_screen_t **  screens ;
static u_int  num_of_screens ;

static u_int  depth ;

static u_int  num_of_startup_screens ;

static x_system_event_listener_t  system_listener ;

static x_main_config_t  main_config ;

static x_shortcut_t  shortcut ;
static x_termcap_t  termcap ;


/* --- static functions --- */

/*
 * Callbacks of ml_config_event_listener_t events.
 */
 
/*
 * Reload mlterm/main file and reset main_config.
 * Notice: Saved changes are not applied to the screens already opened.
 */
static void
config_saved(void)
{
	kik_conf_t *  conf ;
	char *  argv[] = { "mlterm" , NULL } ;

	x_main_config_final( &main_config) ;

	if( ( conf = kik_conf_new()) == NULL)
	{
		return ;
	}
	
	x_prepare_for_main_config( conf) ;
	x_main_config_init( &main_config , conf , 1 , argv) ;

	kik_conf_delete( conf) ;
}

static void
font_config_updated(void)
{
	u_int  count ;
	
	x_font_cache_unload_all() ;

	for( count = 0 ; count < num_of_screens ; count++)
	{
		x_screen_reset_view( screens[count]) ;
	}
}

static void
color_config_updated(void)
{
	u_int  count ;
	
	x_color_cache_unload_all() ;

	x_display_reset_cmap() ;

	for( count = 0 ; count < num_of_screens ; count++)
	{
		x_screen_reset_view( screens[count]) ;
	}
}


static ml_term_t *
create_term_intern(void)
{
	ml_term_t *  term ;
	
	if( ( term = ml_create_term( main_config.cols , main_config.rows ,
			main_config.tab_size , main_config.num_of_log_lines ,
			main_config.encoding , main_config.is_auto_encoding , 
			main_config.unicode_policy , main_config.col_size_of_width_a ,
			main_config.use_char_combining , main_config.use_multi_col_char ,
			main_config.use_bidi , main_config.bidi_mode , main_config.use_ind ,
			x_termcap_get_bool_field(
				x_termcap_get_entry( &termcap , main_config.term_type) , ML_BCE) ,
			main_config.use_dynamic_comb , main_config.bs_mode ,
			main_config.vertical_mode , main_config.use_local_echo ,
			main_config.title , main_config.icon_name)) == NULL)
	{
		return  NULL ;
	}

	if( main_config.icon_path)
	{
		ml_term_set_icon_path( term , main_config.icon_path) ;
	}

	if( main_config.logging_vt_seq)
	{
		ml_term_set_logging_vt_seq( term , 1) ;
	}

	if( main_config.use_auto_detect)
	{
		ml_term_set_use_auto_detect( term , 1) ;
	}

	if( main_config.unlimit_log_size)
	{
		ml_term_unlimit_log_size( term) ;
	}

	return  term ;
}

static int
open_pty_intern(
	ml_term_t *  term ,
	char *  cmd_path ,
	char **  cmd_argv ,
	char *  display ,
	Window  window
	)
{
	char *  env[6] ;	/* MLTERM,TERM,WINDOWID,DISPLAY,COLORFGBG,NULL */
	char **  env_p ;
	char  wid_env[9 + DIGIT_STR_LEN(Window) + 1] ;	/* "WINDOWID="(9) + [32bit digit] + NULL(1) */
	char *  disp_env ;
	char *  term_env ;
	char *  uri ;
	char *  pass ;
	int  ret ;

	env_p = env ;

	*(env_p ++) = mlterm_version ;

	sprintf( wid_env , "WINDOWID=%ld" , window) ;
	*(env_p ++) = wid_env ;

	/* "DISPLAY="(8) + NULL(1) */
	if( display && ( disp_env = alloca( 8 + strlen( display) + 1)))
	{
		sprintf( disp_env , "DISPLAY=%s" , display) ;
		*(env_p ++) = disp_env ;
	}

	/* "TERM="(5) + NULL(1) */
	if( main_config.term_type &&
	    ( term_env = alloca( 5 + strlen( main_config.term_type) + 1)))
	{
		sprintf( term_env , "TERM=%s" , main_config.term_type) ;
		*(env_p ++) = term_env ;
	}

	*(env_p ++) = "COLORFGBG=default;default" ;

	/* NULL terminator */
	*env_p = NULL ;

	uri = NULL ;
	pass = NULL ;

#if  ! defined(USE_WIN32API) && defined(USE_LIBSSH2)
	if( main_config.default_server)
#endif
#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
	{
		char *  user ;
		char *  host ;
		char *  port ;
		char *  encoding ;
		char *  exec_cmd ;
		int  x11_fwd ;
		void *  session ;

		x11_fwd = main_config.use_x11_forwarding ;

	#ifdef  USE_LIBSSH2
		if( ! main_config.show_dialog &&
		#ifdef  USE_WIN32API
		    main_config.default_server &&
		#endif
		    kik_parse_uri( NULL , &user , &host , &port , NULL , &encoding ,
			kik_str_alloca_dup( main_config.default_server)) &&
		    ( session = ml_search_ssh_session( host , port , user)))
		{
			uri = strdup( main_config.default_server) ;
			pass = strdup( "") ;
			exec_cmd = NULL ;

			ml_pty_ssh_set_use_x11_forwarding( session , x11_fwd) ;
		}
		else
	#endif
		if( ! x_connect_dialog( &uri , &pass , &exec_cmd , &x11_fwd , display , window ,
				main_config.server_list , main_config.default_server))
		{
			kik_msg_printf( "Connect dialog is canceled.\n") ;

			return  0 ;
		}
		else
		{
			if( ! kik_parse_uri( NULL , &user , &host , &port , NULL , &encoding ,
					kik_str_alloca_dup( uri)) )
			{
				encoding = NULL ;
			}

		#ifdef  USE_LIBSSH2
			ml_pty_ssh_set_use_x11_forwarding(
				ml_search_ssh_session( host , port , user) , x11_fwd) ;
		#endif
		}

	#ifdef  __DEBUG
		kik_debug_printf( "Connect dialog: URI %s pass %s\n" , uri , pass) ;
	#endif

		if( encoding)
		{
			if( ml_term_is_attached( term))
			{
				/*
				 * Don't use ml_term_change_encoding() here because
				 * encoding change could cause special visual change
				 * which should update the state of x_screen_t.
				 */
				char *  seq ;
				size_t  len ;

				if( ( seq = alloca( ( len = 16 + strlen(encoding) + 2))))
				{
					sprintf( seq , "\x1b]5379;encoding=%s\x07" , encoding) ;
					ml_term_write_loopback( term , seq , len - 1) ;
				}
			}
			else
			{
				ml_term_change_encoding( term , ml_get_char_encoding(encoding)) ;
			}
		}

		if( exec_cmd)
		{
			int  argc ;
			char *  tmp ;

			tmp = exec_cmd ;
			exec_cmd = kik_str_alloca_dup( exec_cmd) ;
			cmd_argv = kik_arg_str_to_array( &argc , exec_cmd) ;
			cmd_path = cmd_argv[0] ;

			free( tmp) ;
		}
	}
#endif


#if  0
	if( cmd_argv)
	{
		char **  p ;

		kik_debug_printf( KIK_DEBUG_TAG " %s", cmd_path) ;
		p = cmd_argv ;
		while( *p)
		{
			kik_msg_printf( " %s", *p) ;
			p++ ;
		}
		kik_msg_printf( "\n") ;
	}
#endif

	/*
	 * If cmd_path and pass are NULL, set default shell as cmd_path.
	 * If uri is not NULL (= connecting to ssh/telnet/rlogin etc servers),
	 * cmd_path is not changed.
	 */
	if( ! uri && ! cmd_path)
	{
		/*
		 * SHELL env var -> /etc/passwd -> /bin/sh
		 */
		if( ( cmd_path = getenv( "SHELL")) == NULL || *cmd_path == '\0')
		{
		#ifndef  USE_WIN32API
			struct passwd *  pw ;

			if( ( pw = getpwuid(getuid())) == NULL ||
				*( cmd_path = pw->pw_shell) == '\0')
		#endif
			{
				cmd_path = "/bin/sh" ;
			}
		}
	}

	/*
	 * Set cmd_argv by cmd_path.
	 */
	if( cmd_path && ! cmd_argv)
	{
		char *  cmd_file ;

		cmd_file = kik_basename( cmd_path) ;

		if( ( cmd_argv = alloca( sizeof( char*) * 2)) == NULL)
		{
			return  0 ;
		}

		/* 2 = `-' and NULL */
		if( ( cmd_argv[0] = alloca( strlen( cmd_file) + 2)) == NULL)
		{
			return  0 ;
		}

		if( main_config.use_login_shell)
		{
			sprintf( cmd_argv[0] , "-%s" , cmd_file) ;
		}
		else
		{
			strcpy( cmd_argv[0] , cmd_file) ;
		}

		cmd_argv[1] = NULL ;
	}

	ret = ml_term_open_pty( term , cmd_path , cmd_argv , env , uri ? uri : display , pass ,
		#ifdef  USE_LIBSSH2
			main_config.public_key , main_config.private_key
		#else
			NULL , NULL
		#endif
			) ;

#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
	if( uri)
	{
		if( ret && kik_compare_str( uri , main_config.default_server) != 0)
		{
			x_main_config_add_to_server_list( &main_config , uri) ;
			free( main_config.default_server) ;
			main_config.default_server = uri ;
		}
		else
		{
			free( uri) ;
		}

		free( pass) ;
	}
#endif

	return  ret ;
}

static int
close_screen_intern(
	x_screen_t *  screen
	)
{
	x_window_t *  root ;
	x_display_t *  disp ;

	x_screen_detach( screen) ;
	x_font_manager_delete( screen->font_man) ;
	x_color_manager_delete( screen->color_man) ;

	root = x_get_root_window( &screen->window) ;
	disp = root->disp ;

	x_display_remove_root( disp, root) ;

	if( disp->num_of_roots == 0)
	{
		x_display_close( disp) ;
	}

	return  1 ;
}

static x_screen_t *
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
	void *  p ;
	
	/*
	 * these are dynamically allocated.
	 */
	disp = NULL ;
	font_man = NULL ;
	color_man = NULL ;
	screen = NULL ;
	sb_screen = NULL ;
	root = NULL ;

	if( MAX_SCREENS <= num_of_screens)
	{
		return  NULL ;
	}

	if( pty)
	{
		if( ( term = ml_get_detached_term( pty)) == NULL)
		{
			return  NULL ;
		}
	}
	else
	{
	#ifdef  __ANDROID__
		if( ( term = ml_get_detached_term( NULL)) == NULL &&
			( term = create_term_intern()) == NULL)
	#else
		if( ( term = create_term_intern()) == NULL)
	#endif
		{
			return  NULL ;
		}
	}

	if( ( disp = x_display_open( main_config.disp_name , depth)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_display_open failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( main_config.unicode_policy & NOT_USE_UNICODE_FONT ||
		main_config.iso88591_font_for_usascii)
	{
		usascii_font_cs = x_get_usascii_font_cs( ML_ISO8859_1) ;
	}
	else if( main_config.unicode_policy & ONLY_USE_UNICODE_FONT)
	{
		usascii_font_cs = x_get_usascii_font_cs( ML_UTF8) ;
	}
	else
	{
		usascii_font_cs = x_get_usascii_font_cs( ml_term_get_encoding( term)) ;
	}
	
	if( ( font_man = x_font_manager_new( disp->display ,
		main_config.type_engine , main_config.font_present ,
		main_config.font_size , usascii_font_cs ,
		main_config.use_multi_col_char ,
		main_config.step_in_changing_font_size ,
		main_config.letter_space , main_config.use_bold_font)) == NULL)
	{
		char *  name ;

		name = x_get_charset_name( usascii_font_cs) ;

		kik_msg_printf( "No fonts found for %s. Please install fonts "
			"and edit the font config file in ~/.mlterm.\n" ,
			name ? name : "US-ASCII") ;

	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_font_manager_new() failed.\n") ;
	#endif

		goto  error ;
	}

	if( ( color_man = x_color_manager_new( disp ,
				main_config.fg_color , main_config.bg_color ,
				main_config.cursor_fg_color , main_config.cursor_bg_color ,
				main_config.bd_color , main_config.ul_color)) == NULL)
	{
		goto  error ;
	}

	if( ( screen = x_screen_new( term , font_man , color_man ,
			x_termcap_get_entry( &termcap , main_config.term_type) ,
			main_config.brightness , main_config.contrast , main_config.gamma ,
			main_config.alpha , main_config.fade_ratio , &shortcut ,
			main_config.screen_width_ratio , main_config.screen_height_ratio ,
			main_config.mod_meta_key ,
			main_config.mod_meta_mode , main_config.bel_mode ,
			main_config.receive_string_via_ucs , main_config.pic_file_path ,
			main_config.use_transbg , main_config.use_vertical_cursor ,
			main_config.big5_buggy , main_config.use_extended_scroll_shortcut ,
			main_config.borderless , main_config.line_space ,
			main_config.input_method , main_config.allow_osc52 ,
			main_config.blink_cursor , main_config.margin ,
			main_config.hide_underline)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_screen_new() failed.\n") ;
	#endif

		goto  error ;
	}

	/* Override config event listener. */
	screen->config_listener.saved = config_saved ;
	
	if( ! x_set_system_listener( screen , &system_listener))
	{
		goto  error ;
	}

#ifndef  __ANDROID__
	if( main_config.use_scrollbar &&
	    ( sb_screen = x_sb_screen_new( screen ,
				main_config.scrollbar_view_name ,
				main_config.sb_fg_color , main_config.sb_bg_color ,
				main_config.sb_mode)))
	{
		root = &sb_screen->window ;
	}
	else
#endif
	{
		root = &screen->window ;
	}

	if( ! x_display_show_root( disp, root, main_config.x, main_config.y,
		main_config.geom_hint, main_config.app_name , main_config.parent_window))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_display_show_root() failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( ( p = realloc( screens , sizeof( x_screen_t*) * (num_of_screens + 1))) == NULL)
	{
		/*
		 * XXX
		 * After x_display_show_root() screen is not deleted correctly by
		 * 'goto error'(see following error handling in open_pty_intern),
		 * but I don't know how to do.
		 */
		goto  error ;
	}

	screens = p ;

	/*
	 * New screen is successfully created here except ml_pty.
	 */

	if( pty)
	{
	#if  0
		/* mlclient /dev/... -e foo */
		if( main_config.cmd_argv)
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
	#endif
	}
	else
	{
		if( ! open_pty_intern( term , main_config.cmd_path , main_config.cmd_argv ,
			DisplayString( disp->display) , root->my_window))
		{
			x_screen_detach( screen) ;
			ml_destroy_term( term) ;

		#ifdef  USE_WIN32GUI
			/*
			 * XXX Hack
			 * In case SendMessage(WM_CLOSE) causes WM_KILLFOCUS
			 * and operates screen->term which was already deleted.
			 * (see window_unfocused())
			 */
			screens[num_of_screens++] = screen ;
			screen->window.window_unfocused = NULL ;
			SendMessage( root->my_window , WM_CLOSE , 0 , 0) ;
		#else
			close_screen_intern( screen) ;
		#endif

			return  NULL ;
		}

		if( main_config.init_str)
		{
			ml_term_write( term , main_config.init_str ,
				strlen( main_config.init_str) , 0) ;
		}
	}

	/* Don't add screen to screens before "return NULL" above unless USE_WIN32GUI. */
	screens[num_of_screens++] = screen ;

	return  screen ;
	
error:
	if( font_man)
	{
		x_font_manager_delete( font_man) ;
	}

	if( color_man)
	{
		x_color_manager_delete( color_man) ;
	}

	if( ! root || ! x_display_remove_root( disp, root))
	{
		/*
		 * If root is still NULL or is not registered to disp yet.
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

	if( disp && disp->num_of_roots == 0)
	{
		x_display_close( disp) ;
	}

	ml_destroy_term( term) ;
	
	return  NULL ;
}


/*
 * callbacks of x_system_event_listener_t
 */
 
/*
 * EXIT_PROGRAM shortcut calls this at last.
 * this is for debugging.
 */
#ifdef  DEBUG
#include  "../main/main_loop.h"

static void
__exit(
	void *  p ,
	int  status
	)
{
#ifdef  USE_WIN32GUI
	u_int  count ;

	for( count = 0 ; count < num_of_screens ; count++)
	{
		SendMessage( x_get_root_window( &screens[count]->window) , WM_CLOSE , 0 , 0) ;
	}
#endif
#if  1
	kik_mem_dump_all() ;
#endif

	main_loop_final() ;

#if  defined(USE_WIN32API) && defined(USE_LIBSSH2)
	WSACleanup() ;
#endif

	kik_dl_close_all() ;

	kik_msg_printf( "reporting unfreed memories --->\n") ;

	kik_alloca_garbage_collect() ;
	kik_mem_free_all() ;

	exit(status) ;
}
#endif

static void
open_pty(
	void *  p ,
	x_screen_t *  screen ,
	char *  dev
	)
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
	#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
		char *  default_server ;
		int  show_dialog ;
	#endif
		int  ret ;

		if( ( new = create_term_intern()) == NULL)
		{
			return ;
		}

	#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
		default_server = main_config.default_server ;
		main_config.default_server = ml_term_get_uri( screen->term) ;
		/*
		 * If show_dialog == 1, main_config.default_server can be
		 * free'ed in open_pty_intern.
		 */
		show_dialog = main_config.show_dialog ;
		main_config.show_dialog = 0 ;
	#endif

		ret = open_pty_intern( new , main_config.cmd_path ,
			main_config.cmd_argv ,
			DisplayString( screen->window.disp->display) ,
			x_get_root_window( &screen->window)->my_window) ;

	#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
		main_config.default_server = default_server ;
		main_config.show_dialog = show_dialog ;
	#endif

		if( ! ret)
		{
			ml_destroy_term( new) ;

			return ;
		}
	}

	x_screen_detach( screen) ;
	x_screen_attach( screen , new) ;
}

static void
next_pty(
	void *  p ,
	x_screen_t *  screen
	)
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
}

static void
prev_pty(
	void *  p ,
	x_screen_t *  screen
	)
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
}

static void
close_pty(
	void *  p ,
	x_screen_t *  screen ,
	char *  dev
	)
{
	ml_term_t *  term ;

	if( dev)
	{
		if( ( term = ml_get_term( dev)) == NULL)
		{
			return ;
		}
	}
	else
	{
		term = screen->term ;
	}

	/*
	 * Don't call ml_destroy_term directly, because close_pty() can be called
	 * in the context of parsing vt100 sequence.
	 */
	kik_trigger_sig_child( ml_term_get_child_pid( term)) ;
}

static void
pty_closed(
	void *  p ,
	x_screen_t *  screen	/* screen->term was already deleted. */
	)
{
	int  count ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " pty which is attached to screen %p is closed.\n" ,
		screen) ;
#endif

	for( count = num_of_screens - 1 ; count >= 0 ; count --)
	{
		if( screen == screens[count])
		{
			ml_term_t *  term ;

			if( ( term = ml_get_detached_term( NULL)) == NULL)
			{
			#ifdef  __DEBUG
				kik_debug_printf( " no detached term. closing screen.\n") ;
			#endif
			
			#ifdef  USE_WIN32GUI
				/*
				 * XXX Hack
				 * In case SendMessage(WM_CLOSE) causes WM_KILLFOCUS
				 * and operates screen->term which was already deleted.
				 * (see window_unfocused())
				 */
				screen->window.window_unfocused = NULL ;
				SendMessage( x_get_root_window( &screen->window)->my_window,
					WM_CLOSE, 0, 0) ;
			#else
				screens[count] = screens[--num_of_screens] ;
				close_screen_intern( screen) ;
			#endif
			}
			else
			{
			#ifdef  __DEBUG
				kik_debug_printf( " using detached term.\n") ;
			#endif

				x_screen_attach( screen , term) ;
			}

			return ;
		}
	}
}

static void
open_screen(
	void *  p ,
	x_screen_t *  cur_screen	/* Screen which triggers this event. */
	)
{
	char *  disp_name ;
#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
	char *  default_server ;
	int  show_dialog ;
#endif

	disp_name = main_config.disp_name ;
	main_config.disp_name = cur_screen->window.disp->name ;
#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
	default_server = main_config.default_server ;
	main_config.default_server = ml_term_get_uri( cur_screen->term) ;
	/*
	 * If show_dialog == 1, main_config.default_server can be
	 * free'ed in open_pty_intern.
	 */
	show_dialog = main_config.show_dialog ;
	main_config.show_dialog = 0 ;
#endif

	if( ! open_screen_intern( NULL))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " open_screen_intern failed.\n") ;
	#endif
	}

	main_config.disp_name = disp_name ;
#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
	main_config.default_server = default_server ;
	main_config.show_dialog = show_dialog ;
#endif
}

static void
close_screen(
	void *  p ,
	x_screen_t *  screen	/* Screen which triggers this event. */
	)
{
	u_int  count ;
	
	for( count = 0 ; count < num_of_screens ; count ++)
	{
		u_int  idx ;

		if( screen != screens[count])
		{
			continue ;
		}

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " screen %p is registered to be closed.\n",
			screen) ;
	#endif

		idx = count / MSU ;	/* count / 8 */
		dead_mask[idx] |= (1 << (count - MSU * idx)) ;

		return ;
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

static int
mlclient(
	void *  self ,
	x_screen_t *  screen ,
	char *  args ,
	FILE *  fp		/* Stream to output response of mlclient. */
	)
{
	char **  argv ;
	int  argc ;

	argv = kik_arg_str_to_array( &argc , args) ;

#ifdef  __DEBUG
	{
		int  i ;

		for( i = 0 ; i < argc ; i ++)
		{
			kik_msg_printf( "%s\n" , argv[i]) ;
		}
	}
#endif

	if( argc == 0
	#ifdef  USE_FRAMEBUFFER
	    || screen == NULL
	#endif
	    )
	{
		return  0 ;
	}

	if( argc == 2 && ( strcmp( argv[1] , "-P") == 0 || strcmp( argv[1] , "--ptylist") == 0))
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
				fprintf( fp , "(whose title is %s)" ,
					ml_term_window_name( terms[count])) ;
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
	else
	{
		kik_conf_t *  conf ;
		x_main_config_t  orig_conf ;
		char *  pty ;
	#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
		char **  server_list ;
	#endif
		
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

		if( ( conf = kik_conf_new()) == NULL)
		{
			return  0 ;
		}
		
		x_prepare_for_main_config(conf) ;

		if( ! kik_conf_parse_args( conf , &argc , &argv))
		{
			kik_conf_delete( conf) ;

			return  0 ;
		}

		orig_conf = main_config ;

		x_main_config_init( &main_config , conf , argc , argv) ;
	#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
		server_list = main_config.server_list ;
		main_config.server_list = orig_conf.server_list ;
	#endif

		kik_conf_delete( conf) ;

		if( screen)
		{
			open_pty( self , screen , pty) ;
		}
		else
		{
			if( ! open_screen_intern( pty))
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " open_screen_intern() failed.\n") ;
			#endif
			}
		}

	#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
		orig_conf.server_list = main_config.server_list ;
		main_config.server_list = server_list ;
	#endif
		x_main_config_final( &main_config) ;

		main_config = orig_conf ;
	}

	/* Flush fp stream because write(2) is called after this function is called. */
	fflush( fp) ;
	
	return  1 ;
}


/* --- global functions --- */

int
x_screen_manager_init(
	char *  _mlterm_version ,
	u_int  _depth ,
	u_int  _max_screens_multiple ,
	u_int  _num_of_startup_screens ,
	x_main_config_t *  _main_config
	)
{
	mlterm_version = _mlterm_version ;

	depth = _depth ;

	main_config = *_main_config ;
	
	max_screens_multiple = _max_screens_multiple ;

	if( ( dead_mask = calloc( sizeof( *dead_mask) , max_screens_multiple)) == NULL)
	{
		return  0 ;
	}

	if( _num_of_startup_screens > MAX_SCREENS)
	{
		num_of_startup_screens = MAX_SCREENS ;
	}
	else
	{
		num_of_startup_screens = _num_of_startup_screens ;
	}

	if( ! ml_color_config_init())
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_color_config_init failed.\n") ;
	#endif
	
		return  0 ;
	}
	
	if( ! x_shortcut_init( &shortcut))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_shortcut_init failed.\n") ;
	#endif
	
		return  0 ;
	}
	/* BACKWARD COMPAT (3.1.7 or before) */
#if  1
	else
	{
		size_t  count ;
		char  key0[] = "Control+Button1" ;
		char  key1[] = "Control+Button2" ;
		char  key2[] = "Control+Button3" ;
		char  key3[] = "Button3" ;
		char *  keys[] = { key0 , key1 , key2 , key3 } ;

		for( count = 0 ; count < sizeof(keys) / sizeof(keys[0]) ; count ++)
		{
			if( main_config.shortcut_strs[count])
			{
				x_shortcut_parse( &shortcut , keys[count] ,
					main_config.shortcut_strs[count]) ;
			}
		}
	}
#endif

	if( ! x_termcap_init( &termcap))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_termcap_init failed.\n") ;
	#endif
	
		return  0 ;
	}

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
#ifdef  DEBUG
	system_listener.exit = __exit ;
#else
	system_listener.exit = NULL ;
#endif
#ifdef  USE_FRAMEBUFFER
	system_listener.open_screen = NULL ;
#else
	system_listener.open_screen = open_screen ;
#endif
	system_listener.close_screen = close_screen ;
	system_listener.open_pty = open_pty ;
	system_listener.next_pty = next_pty ;
	system_listener.prev_pty = prev_pty ;
	system_listener.close_pty = close_pty ;
	system_listener.pty_closed = pty_closed ;
	system_listener.get_pty = get_pty ;
	system_listener.pty_list = pty_list ;
	system_listener.mlclient = mlclient ;
	system_listener.font_config_updated = font_config_updated ;
	system_listener.color_config_updated = color_config_updated ;

	if( ! ml_term_manager_init( max_screens_multiple))
	{
		free( dead_mask) ;

		return  0 ;
	}

	return  1 ;
}

int
x_screen_manager_final(void)
{
	u_int  count ;

	x_main_config_final( &main_config) ;
	
	for( count = 0 ; count < num_of_screens ; count ++)
	{
		close_screen_intern( screens[count]) ;
	}

	free( screens) ;
	free( dead_mask) ;

	ml_term_manager_final() ;

	x_display_close_all() ;

	ml_color_config_final() ;
	x_shortcut_final( &shortcut) ;
	x_termcap_final( &termcap) ;

	return  1 ;
}

#ifdef  __ANDROID__
static int  suspended ;

int
x_screen_manager_suspend(void)
{
	u_int  count ;

	x_close_dead_screens() ;

	for( count = 0 ; count < num_of_screens ; count ++)
	{
		close_screen_intern( screens[count]) ;
	}

	free( screens) ;
	screens = NULL ;
	num_of_screens = 0 ;

	x_display_close_all() ;

	suspended = 1 ;

	return  1 ;
}
#endif

u_int
x_screen_manager_startup(void)
{
	u_int  count ;
	u_int  num_started ;

	num_started = 0 ;

#ifdef  __ANDROID__
	if( suspended)
	{
		/* reload ~/.mlterm/main. */
		config_saved() ;
	}
#endif

	for( count = 0 ; count < num_of_startup_screens ; count ++)
	{
		if( ! open_screen_intern( NULL))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " open_screen_intern() failed.\n") ;
		#endif
		}
		else
		{
			num_started ++ ;
		}
	}

	return  num_started ;
}

int
x_close_dead_screens(void)
{
	if( num_of_screens > 0)
	{
		int  idx ;

		for( idx = (num_of_screens - 1) / MSU ; idx >= 0 ; idx --)
		{
			if( dead_mask[idx])
			{
				int  count ;

				for( count = MSU - 1 ; count >= 0 ; count --)
				{
					if( dead_mask[idx] & (0x1 << count))
					{
						x_screen_t *  screen ;

					#ifdef  __DEBUG
						kik_debug_printf( KIK_DEBUG_TAG
							" closing screen %d-%d." ,
							idx , count) ;
					#endif

						screen = screens[idx * MSU + count] ;
						screens[idx * MSU + count] =
							screens[--num_of_screens] ;
						close_screen_intern( screen) ;

					#ifdef  __DEBUG
						kik_msg_printf( " => Finished. Rest %d\n" ,
								num_of_screens) ;
					#endif
					}
				}

				memset( &dead_mask[idx] , 0 , sizeof(dead_mask[idx])) ;
			}
		}
	}

	return  1 ;
}

u_int
x_get_all_screens(
	x_screen_t ***  _screens
	)
{
	if( _screens)
	{
		*_screens = screens ;
	}

	return  num_of_screens ;
}

int
x_mlclient(
	char *  args ,
	FILE *  fp
	)
{
	return  mlclient( NULL , NULL , args , fp) ;
}
