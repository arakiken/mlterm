/*
 *	$Id$
 */

#include  "ml_term_manager.h"

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
#include  <mkf/mkf_charset.h>	/* mkf_charset_t */

#include  "version.h"
#include  "ml_xim.h"
#include  "ml_sb_term_screen.h"
#include  "ml_sig_child.h"


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

static ml_display_t *
open_display(
	ml_term_manager_t *  term_man
	)
{
	int  counter ;
	ml_display_t *  disp ;
	void *  p ;

	for( counter = 0 ; counter < term_man->num_of_displays ; counter ++)
	{
		if( strcmp( term_man->displays[counter]->name , term_man->conf.disp_name) == 0)
		{
			return  term_man->displays[counter] ;
		}
	}

	if( ( disp = malloc( sizeof( ml_display_t))) == NULL)
	{
		return  NULL ;
	}
	
	if( ( disp->display = XOpenDisplay( term_man->conf.disp_name)) == NULL)
	{
		kik_msg_printf( " display %s couldn't be opened.\n" , term_man->conf.disp_name) ;

		goto  error1 ;
	}

	if( ( disp->name = strdup( term_man->conf.disp_name)) == NULL)
	{
		goto  error2 ;
	}
	
	if( ! ml_window_manager_init( &disp->win_man , disp->display))
	{
		goto  error3 ;
	}

	ml_xim_display_opened( disp->display) ;
	ml_picture_display_opened( disp->display) ;
	
	if( ! ml_color_manager_init( &disp->color_man , disp->display ,
		DefaultScreen( disp->display) , &term_man->color_custom))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_color_manager_init failed.\n") ;
	#endif

		goto  error4 ;
	}

	if( ( p = realloc( term_man->displays ,
			sizeof( ml_display_t*) * (term_man->num_of_displays + 1))) == NULL)
	{
		goto  error5 ;
	}

	term_man->displays = p ;
	term_man->displays[term_man->num_of_displays ++] = disp ;

#ifdef  DEBUG
	kik_debug_printf( "X connection opened.\n") ;
#endif

	return  disp ;

error5:
	ml_color_manager_final( &disp->color_man) ;

error4:
	ml_window_manager_final( &disp->win_man) ;

error3:
	free( disp->name) ;

error2:
	XCloseDisplay( disp->display) ;

error1:
	free( disp) ;

	return  NULL ;
}

static int
delete_display(
	ml_display_t *  disp
	)
{
	free( disp->name) ;
	ml_color_manager_final( &disp->color_man) ;
	ml_window_manager_final( &disp->win_man) ;
	ml_xim_display_closed( disp->display) ;
	ml_picture_display_closed( disp->display) ;
	XCloseDisplay( disp->display) ;
	
	free( disp) ;

	return  1 ;
}

static int
close_display(
	ml_term_manager_t *  term_man ,
	ml_display_t *  disp
	)
{
	int  counter ;

	for( counter = 0 ; counter < term_man->num_of_displays ; counter ++)
	{
		if( term_man->displays[counter] == disp)
		{
			delete_display( disp) ;
			term_man->displays[counter] =
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
	ml_term_manager_t *  term_man
	)
{
	ml_display_t *  disp ;
	ml_term_model_t *  termmdl ;
	ml_term_screen_t *  termscr ;
	ml_sb_term_screen_t *  sb_termscr ;
	ml_font_manager_t *  font_man ;
	ml_vt100_parser_t *  vt100_parser ;
	ml_pty_t *  pty ;
	ml_window_t *  root ;
	mkf_charset_t  usascii_font_cs ;
	ml_color_t  fg_color ;
	ml_color_t  bg_color ;
	ml_char_t  sp_ch ;
	ml_char_t  nl_ch ;
	int  usascii_font_cs_changable ;
	char *  env[4] ;
	char **  env_p ;
	char  wid_env[9 + DIGIT_STR_LEN(Window) + 1] ;	/* "WINDOWID="(9) + [32bit digit] + NULL(1) */
	char *  disp_env ;
	char *  disp_str ;
	char *  term ;

	if( term_man->num_of_terms == term_man->max_terms
		/* block until dead_mask is cleared */
		|| term_man->dead_mask)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " still busy.\n") ;
	#endif
	
		return  0 ;
	}

	/*
	 * these are dynamically allocated.
	 */
	disp = NULL ;
	font_man = NULL ;
	termmdl = NULL ;
	termscr = NULL ;
	sb_termscr = NULL ;
	vt100_parser = NULL ;
	pty = NULL ;

	if( ( disp = open_display( term_man)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " open_display failed.\n") ;
	#endif
	
		return  0 ;
	}

	if( term_man->conf.not_use_unicode_font || term_man->conf.iso88591_font_for_usascii)
	{
		usascii_font_cs = ml_get_usascii_font_cs( ML_ISO8859_1) ;
		usascii_font_cs_changable = 0 ;
	}
	else if( term_man->conf.only_use_unicode_font)
	{
		usascii_font_cs = ml_get_usascii_font_cs( ML_UTF8) ;
		usascii_font_cs_changable = 0 ;
	}
	else
	{
		usascii_font_cs = ml_get_usascii_font_cs( term_man->conf.encoding) ;
		usascii_font_cs_changable = 1 ;
	}
	
	if( ( font_man = ml_font_manager_new( disp->display ,
		&term_man->normal_font_custom , &term_man->v_font_custom , &term_man->t_font_custom ,
	#ifdef  ANTI_ALIAS
		&term_man->aa_font_custom , &term_man->vaa_font_custom , &term_man->taa_font_custom ,
	#else
		NULL , NULL , NULL ,
	#endif
		term_man->conf.font_size , term_man->conf.line_space , usascii_font_cs ,
		usascii_font_cs_changable , term_man->conf.use_multi_col_char ,
		term_man->conf.step_in_changing_font_size)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_font_manager_new() failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( term_man->conf.fg_color == NULL ||
		( fg_color = ml_get_color( &disp->color_man , term_man->conf.fg_color)) == ML_UNKNOWN_COLOR)
	{
		if( ( fg_color = ml_get_color( &disp->color_man , "black")) == ML_UNKNOWN_COLOR)
		{
			goto  error ;
		}
	}

	if( term_man->conf.bg_color == NULL ||
		( bg_color = ml_get_color( &disp->color_man , term_man->conf.bg_color)) == ML_UNKNOWN_COLOR)
	{
		if( ( bg_color = ml_get_color( &disp->color_man , "white")) == ML_UNKNOWN_COLOR)
		{
			goto  error ;
		}
	}

	if( fg_color == bg_color)
	{
		kik_msg_printf( " Foreground and background colors are the same.\n") ;
	}

	ml_char_init( &sp_ch) ;
	ml_char_init( &nl_ch) ;
	
	ml_char_set( &sp_ch , " " , 1 , ml_get_usascii_font( font_man) ,
		0 , ML_FG_COLOR , ML_BG_COLOR , 0) ;
	ml_char_set( &nl_ch , "\n" , 1 , ml_get_usascii_font( font_man) ,
		0 , ML_FG_COLOR , ML_BG_COLOR , 0) ;

	if( ( termmdl = ml_term_model_new( term_man->conf.cols , term_man->conf.rows ,
				&sp_ch , &nl_ch , term_man->conf.tab_size ,
				term_man->conf.num_of_log_lines , term_man->conf.use_bce)) == NULL)
	{
		goto  error ;
	}

	ml_char_final( &sp_ch) ;
	ml_char_final( &nl_ch) ;

	if( ( termscr = ml_term_screen_new( termmdl , font_man , &disp->color_man ,
		fg_color , bg_color , term_man->conf.brightness ,
		term_man->conf.fade_ratio , &term_man->keymap , &term_man->termcap ,
		term_man->conf.screen_width_ratio , term_man->conf.screen_height_ratio ,
		term_man->conf.xim_open_in_startup , term_man->conf.mod_meta_mode ,
		term_man->conf.bel_mode , term_man->conf.copy_paste_via_ucs ,
		term_man->conf.pic_file_path , term_man->conf.use_transbg ,
		term_man->conf.font_present , term_man->conf.use_bidi ,
		term_man->conf.vertical_mode , term_man->conf.use_vertical_cursor ,
		term_man->conf.big5_buggy , term_man->conf.conf_menu_path ,
		term_man->conf.iscii_lang , term_man->conf.use_extended_scroll_shortcut ,
		term_man->conf.use_dynamic_comb)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_term_screen_new() failed.\n") ;
	#endif

		goto  error ;
	}

	if( ! ml_set_system_listener( termscr , &term_man->system_listener))
	{
		goto  error ;
	}

	if( term_man->conf.use_scrollbar)
	{
		ml_color_t  sb_fg_color ;
		ml_color_t  sb_bg_color ;
		
		if( term_man->conf.sb_fg_color)
		{
			sb_fg_color = ml_get_color( &disp->color_man , term_man->conf.sb_fg_color) ;
		}
		else
		{
			sb_fg_color = ML_UNKNOWN_COLOR ;
		}

		if( term_man->conf.sb_bg_color)
		{
			sb_bg_color = ml_get_color( &disp->color_man , term_man->conf.sb_bg_color) ;
		}
		else
		{
			sb_bg_color = ML_UNKNOWN_COLOR ;
		}

		if( ( sb_termscr = ml_sb_term_screen_new( termscr ,
					term_man->conf.scrollbar_view_name ,
					&disp->color_man , sb_fg_color , sb_bg_color ,
					term_man->conf.sb_mode)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_sb_term_screen_new() failed.\n") ;
		#endif

			goto  error ;
		}

		root = &sb_termscr->window ;
	}
	else
	{
		sb_termscr = NULL ;
		root = &termscr->window ;
	}

	if( ( vt100_parser = ml_vt100_parser_new( termscr , termscr->model ,
		term_man->conf.encoding , term_man->conf.not_use_unicode_font ,
		term_man->conf.only_use_unicode_font , term_man->conf.col_size_a)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_vt100_parser_new() failed.\n") ;
	#endif
		
		goto  error ;
	}

	if( ! ml_window_manager_show_root( &disp->win_man , root ,
		term_man->conf.x , term_man->conf.y , term_man->conf.geom_hint))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_window_manager_show_root() failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( term_man->conf.app_name)
	{
		ml_set_window_name( &termscr->window , term_man->conf.app_name) ;
		ml_set_icon_name( &termscr->window , term_man->conf.icon_name) ;
	}
	else
	{
		if( term_man->conf.title)
		{
			ml_set_window_name( &termscr->window , term_man->conf.title) ;
		}
		
		if( term_man->conf.icon_name)
		{
			ml_set_icon_name( &termscr->window , term_man->conf.icon_name) ;
		}
	}

	env_p = env ;
	
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
	if( ( term = alloca( 5 + strlen( term_man->conf.term_type) + 1)))
	{
		sprintf( term , "TERM=%s" , term_man->conf.term_type) ;
		*(env_p ++) = term ;
	}

	/* NULL terminator */
	*env_p = NULL ;

	if( term_man->conf.cmd_path && term_man->conf.cmd_argv)
	{
		if( ( pty = ml_pty_new( term_man->conf.cmd_path , term_man->conf.cmd_argv ,
				env , disp_str , ml_term_model_get_logical_cols( termscr->model) ,
				ml_term_model_get_logical_rows( termscr->model))) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_pty_new() failed.\n") ;
		#endif

			goto  error ;
		}
	}
	else
	{
		struct passwd *  pw ;
		char *  cmd_path ;
		char *  cmd_file ;
		char **  cmd_argv ;

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

		if( ( pty = ml_pty_new( cmd_path , cmd_argv , env , disp_str ,
				ml_term_model_get_logical_cols( termscr->model) ,
				ml_term_model_get_logical_rows( termscr->model))) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_pty_new() failed.\n") ;
		#endif

			goto  error ;
		}
	}

	if( ! ml_term_screen_set_pty( termscr , pty))
	{
		goto  error ;
	}

	if( ! ml_vt100_parser_set_pty( vt100_parser , pty))
	{
		goto  error ;
	}

	term_man->terms[term_man->num_of_terms].pty = pty ;
	term_man->terms[term_man->num_of_terms].display = disp ;
	term_man->terms[term_man->num_of_terms].root_window = root ;
	term_man->terms[term_man->num_of_terms].vt100_parser = vt100_parser ;
	term_man->terms[term_man->num_of_terms].font_man = font_man ;
	term_man->terms[term_man->num_of_terms].model = termmdl ;

	term_man->num_of_terms ++ ;

	return  1 ;
	
error:
	if( disp)
	{
		close_display( term_man , disp) ;
	}
	
	if( font_man)
	{
		ml_font_manager_delete( font_man) ;
	}

	if( termmdl)
	{
		ml_term_model_delete( termmdl) ;
	}

	if( termscr)
	{
		ml_term_screen_delete( termscr) ;
	}

	if( sb_termscr)
	{
		ml_sb_term_screen_delete( sb_termscr) ;
	}

	if( vt100_parser)
	{
		ml_vt100_parser_delete( vt100_parser) ;
	}

	if( pty)
	{
		ml_pty_delete( pty) ;
	}

	return  0 ;
}

static int
delete_term(
	ml_term_t *  term
	)
{
	ml_vt100_parser_delete( term->vt100_parser) ;

	ml_pty_delete( term->pty) ;

	ml_font_manager_delete( term->font_man) ;

	ml_term_model_delete( term->model) ;

	ml_window_manager_remove_root( &term->display->win_man , term->root_window) ;
	
	return  1 ;
}

static int
close_dead_terms(
	ml_term_manager_t *  term_man
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < term_man->max_terms ; counter ++)
	{
		if( term_man->dead_mask & (0x1 << counter))
		{
			delete_term( &term_man->terms[counter]) ;
			
			if( -- term_man->num_of_terms == 0 && ! term_man->is_genuine_daemon)
			{
				if( un_file)
				{
					unlink( un_file) ;
				}
			
				exit( 0) ;
			}

			if( term_man->terms[counter].display->win_man.num_of_roots == 0)
			{
				close_display( term_man , term_man->terms[counter].display) ;
			}

			term_man->terms[counter] = term_man->terms[term_man->num_of_terms] ;
		}
	}
	
	term_man->dead_mask = 0 ;
	
	return  1 ;
}


/*
 * callbacks of ml_system_event_listener_t
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
	ml_term_manager_t *  term_man ;

	term_man = p ;

	ml_term_manager_final( term_man) ;
	
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
	ml_term_manager_t *  term_man ;

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
	ml_window_t *  root_window
	)
{
	int  counter ;
	ml_term_manager_t *  term_man ;

	term_man = p ;
	
	for( counter = 0 ; counter < term_man->num_of_terms ; counter ++)
	{
		if( root_window == term_man->terms[counter].root_window)
		{
			kill( term_man->terms[counter].pty->child_pid , SIGKILL) ;

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
	ml_term_manager_t *  term_man ;
	int  counter ;

	term_man = p ;
	
	for( counter = 0 ; counter < term_man->num_of_terms ; counter ++)
	{
		if( pid == term_man->terms[counter].pty->child_pid)
		{
			term_man->dead_mask |= (1 << counter) ;

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
	ml_term_manager_t *  term_man
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
		"screen width ratio") ;
	kik_conf_add_opt( conf , '2' , "hscr" , 0 , "screen_height_ratio" ,
		"screen height ratio") ;
	kik_conf_add_opt( conf , '3' , "bce" , 1 , "use_bce" ,
		"use bce") ;
	kik_conf_add_opt( conf , '5' , "big5bug" , 1 , "big5_buggy" ,
		"support buggy Big5 CTEXT in XFree86 4.1 or earlier") ;
	kik_conf_add_opt( conf , '7' , "bel" , 0 , "bel_mode" , 
		"bel(0x07) mode") ;
	kik_conf_add_opt( conf , '8' , "88591" , 1 , "iso88591_font_for_usascii" ,
		"use ISO-8859-1 font for US-ASCII part of every encodings") ;
#ifdef  ANTI_ALIAS
	kik_conf_add_opt( conf , 'A' , "aa" , 1 , "use_anti_alias" , 
		"use anti alias font") ;
#endif
	kik_conf_add_opt( conf , 'B' , "sbbg" , 0 , "sb_bg_color" , 
		"scrollbar bg color") ;
	kik_conf_add_opt( conf , 'C' , "iscii" , 0 , "iscii_lang" , 
		"language to be used in ISCII encoding") ;
	kik_conf_add_opt( conf , 'D' , "bi" , 1 , "use_bidi" , 
		"use bidi") ;
	kik_conf_add_opt( conf , 'E' , "km" , 0 , "ENCODING" , 
		"character encoding") ;
	kik_conf_add_opt( conf , 'F' , "sbfg" , 0 , "sb_fg_color" , 
		"scrollbar fg color") ;
	kik_conf_add_opt( conf , 'G' , "vertical" , 0 , "vertical_mode" ,
		"vertical view mode") ;
	kik_conf_add_opt( conf , 'H' , "bright" , 0 , "brightness" ,
		"the amount of darkening or lightening background image") ;
	kik_conf_add_opt( conf , 'I' , "icon" , 0 , "icon_name" , 
		"icon name") ;
	kik_conf_add_opt( conf , 'J' , "dyncomb" , 1 , "use_dynamic_comb" ,
		"use dynamic combining") ;
	kik_conf_add_opt( conf , 'L' , "ls" , 1 , "use_login_shell" , 
		"turn on login shell") ;
	kik_conf_add_opt( conf , 'M' , "menu" , 0 , "conf_menu_path" ,
		"command path of mlconfig (GUI configurator)") ;
	kik_conf_add_opt( conf , 'N' , "name" , 0 , "app_name" , 
		"application name") ;
	kik_conf_add_opt( conf , 'O' , "sbmod" , 0 , "scrollbar_mode" ,
		"scrollbar mode") ;
	kik_conf_add_opt( conf , 'Q' , "vcur" , 1 , "use_vertical_cursor" ,
		"use vertical cursor") ;
	kik_conf_add_opt( conf , 'S' , "sbview" , 0 , "scrollbar_view_name" , 
		"scrollbar view name") ;
	kik_conf_add_opt( conf , 'T' , "title" , 0 , "title" , 
		"title name") ;
	kik_conf_add_opt( conf , 'U' , "viaucs" , 1 , "copy_paste_via_ucs" ,
		"process received strings via ucs.") ;
	kik_conf_add_opt( conf , 'V' , "varwidth" , 1 , "use_variable_column_width" ,
		"variable column width") ;
	kik_conf_add_opt( conf , 'X' , "openim" , 1 , "xim_open_in_startup" , 
		"open xim in starting up") ;
	kik_conf_add_opt( conf , 'Z' , "multicol" , 1 , "use_multi_column_char" ,
		"use multiple column character") ;
	kik_conf_add_opt( conf , 'a' , "ac" , 0 , "col_size_of_width_a" ,
		"column numbers for Unicode EastAsianAmbiguous character") ;
	kik_conf_add_opt( conf , 'b' , "bg" , 0 , "bg_color" , 
		"bg color") ;
	kik_conf_add_opt( conf , 'd' , "display" , 0 , "display" , 
		"X server to connect") ;
	kik_conf_add_opt( conf , 'f' , "fg" , 0 , "fg_color" , 
		"fg color") ;
	kik_conf_add_opt( conf , 'g' , "geometry" , 0 , "geometry" , 
		"size (in characters) and position") ;
	kik_conf_add_opt( conf , 'k' , "meta" , 0 , "mod_meta_mode" , 
		"mode in pressing meta key") ;
	kik_conf_add_opt( conf , 'l' , "sl" , 0 , "logsize" , 
		"number of scrolled lines to save") ;
	kik_conf_add_opt( conf , 'm' , "comb" , 1 , "use_combining" , 
		"use combining characters") ;
	kik_conf_add_opt( conf , 'n' , "noucsfont" , 1 , "not_use_unicode_font" ,
		"use non-Unicode fonts even in UTF-8 mode") ;
	kik_conf_add_opt( conf , 'o' , "lsp" , 0 , "line_space" ,
		"number of extra dots between lines") ;
	kik_conf_add_opt( conf , 'p' , "pic" , 0 , "wall_picture" , 
		"wall picture path") ;
	kik_conf_add_opt( conf , 'q' , "extkey" , 1 , "use_extended_scroll_shortcut" ,
		"use extended scroll shortcut key") ;
	kik_conf_add_opt( conf , 'r' , "fade" , 0 , "fade_ratio" , 
		"fade ratio when window unfocued.") ;
	kik_conf_add_opt( conf , 's' , "sb" , 1 , "use_scrollbar" , 
		"use scrollbar") ;
	kik_conf_add_opt( conf , 't' , "transbg" , 1 , "use_transbg" , 
		"use transparent background") ;
	kik_conf_add_opt( conf , 'u' , "onlyucsfont" , 1 , "only_use_unicode_font" ,
		"use a Unicode font even in non-UTF-8 modes") ;
	kik_conf_add_opt( conf , 'w' , "fontsize" , 0 , "fontsize" , 
		"font size") ;
	kik_conf_add_opt( conf , 'x' , "tw" , 0 , "tabsize" , 
		"tab width") ;
	kik_conf_add_opt( conf , 'y' , "term" , 0 , "termtype" , 
		"terminal type") ;
	kik_conf_add_opt( conf , 'z' ,  "largesmall" , 0 , "step_in_changing_font_size" ,
		"step in changing font size in GUI configurator") ;

	kik_conf_set_end_opt( conf , 'e' , NULL , "exec_cmd" , 
		"execute external command") ;

	return  conf ;	
}

static int
config_init(
	ml_term_manager_t *  term_man ,
	kik_conf_t *  conf ,
	int  argc ,
	char **  argv
	)
{
	char *  kterm = "kterm" ;
	char *  xterm = "xterm" ;
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

	if( ( value = kik_conf_get_value( conf , "use_combining")))
	{
		if( strcmp( value , "true") == 0)
		{
			ml_use_char_combining() ;
		}
	}
	else
	{
		/* combining is used as default */
		ml_use_char_combining() ;
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
	term_man->conf.font_present &= ~FONT_AA ;
	
	if( ( value = kik_conf_get_value( conf , "use_anti_alias")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->conf.font_present |= FONT_AA ;
		}
	}
#endif

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
		if( strcasecmp( value , kterm) == 0)
		{
			term_man->conf.term_type = kterm ;
		}
		else if( strcasecmp( value , xterm) == 0)
		{
			term_man->conf.term_type = xterm ;
		}
		else
		{
			kik_msg_printf(
				"supported terminal types are only xterm or kterm , xterm is used.\n") ;

			term_man->conf.term_type = xterm ;
		}
	}
	else
	{
		term_man->conf.term_type = xterm ;
	}

	term_man->conf.use_bce = 0 ;
	if( ( value = kik_conf_get_value( conf , "use_bce")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->conf.use_bce = 1 ;
		}
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
		term_man->conf.sb_mode = ml_get_sb_mode( value) ;
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
		term_man->conf.mod_meta_mode = ml_get_mod_meta_mode( value) ;
	}
	else
	{
		term_man->conf.mod_meta_mode = MOD_META_NONE ;
	}

	if( ( value = kik_conf_get_value( conf , "bel_mode")))
	{
		term_man->conf.bel_mode = ml_get_bel_mode( value) ;
	}
	else
	{
		term_man->conf.bel_mode = BEL_SOUND ;
	}

	term_man->conf.vertical_mode = 0 ;
	
	if( ( value = kik_conf_get_value( conf , "vertical_mode")))
	{
		term_man->conf.vertical_mode = ml_get_vertical_mode( value) ;
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
	ml_term_manager_t *  term_man
	)
{
	free( term_man->conf.disp_name) ;
	free( term_man->conf.app_name) ;
	free( term_man->conf.title) ;
	free( term_man->conf.icon_name) ;
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
	ml_term_manager_t *  term_man
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
	struct ml_config  orig_conf ;

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
	ml_term_manager_t *  term_man
	)
{
	int  counter ;
	int  xfd ;
	int  ptyfd ;
	int  maxfd ;
	int  ret ;
	fd_set  read_fds ;

	struct timeval  tval ;

	/*
	 * flush buffer
	 */

	for( counter = 0 ; counter < term_man->num_of_terms ; counter ++)
	{
		ml_flush_pty( term_man->terms[counter].pty) ;
	}
	
	while( 1)
	{
		/* on Linux tv_usec,tv_sec members are zero cleared after select() */
		tval.tv_usec = 50000 ;	/* 0.05 sec */
		tval.tv_sec = 0 ;

		maxfd = 0 ;
		FD_ZERO( &read_fds) ;

		for( counter = 0 ; counter < term_man->num_of_displays ; counter ++)
		{
			/*
			 * it is necessary to flush events here since some events
			 * may have happened in idling
			 */
			ml_window_manager_receive_next_event( &term_man->displays[counter]->win_man) ;

			xfd = XConnectionNumber( term_man->displays[counter]->display) ;
			
			FD_SET( xfd , &read_fds) ;
		
			if( xfd > maxfd)
			{
				maxfd = xfd ;
			}
		}
		
		for( counter = 0 ; counter < term_man->num_of_terms ; counter ++)
		{
			ptyfd = term_man->terms[counter].pty->fd ;
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

		for( counter = 0 ; counter < term_man->num_of_displays ; counter ++)
		{
			ml_window_manager_idling( &term_man->displays[counter]->win_man) ;
		}
	}
	
	if( ret < 0)
	{
		/* error happened */
		
		return ;
	}

	for( counter = 0 ; counter < term_man->num_of_displays ; counter ++)
	{
		if( FD_ISSET( XConnectionNumber( term_man->displays[counter]->display) , &read_fds))
		{
			ml_window_manager_receive_next_event( &term_man->displays[counter]->win_man) ;
		}
	}

	for( counter = 0 ; counter < term_man->num_of_terms ; counter ++)
	{
		if( FD_ISSET( term_man->terms[counter].pty->fd , &read_fds))
		{
			ml_parse_vt100_sequence( term_man->terms[counter].vt100_parser) ;
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
ml_term_manager_init(
	ml_term_manager_t *  term_man ,
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
		"max ptys to use") ;
	kik_conf_add_opt( conf , 'h' , "help" , 1 , "help" ,
		"help message") ;
	kik_conf_add_opt( conf , 'v' , "version" , 1 , "version" ,
		"version message") ;
	kik_conf_add_opt( conf , 'P' , "ptys" , 0 , "ptys" , 
		"num of ptys to use in start up") ;
	kik_conf_add_opt( conf , 'R' , "fsrange" , 0 , "font_size_range" , 
		"font size range") ;
	kik_conf_add_opt( conf , 'W' , "sep" , 0 , "word_separators" , 
		"word separator characters") ;
	kik_conf_add_opt( conf , 'Y' , "decsp" , 1 , "compose_dec_special_font" ,
		"compose dec special font") ;
#ifdef  ANTI_ALIAS
	kik_conf_add_opt( conf , 'c' , "cp932" , 1 , "use_cp932_ucs_for_xft" , 
		"CP932 mapping table for JISX0208-Unicode conversion") ;
#endif
	kik_conf_add_opt( conf , 'i' , "xim" , 1 , "use_xim" , 
		"use XIM (X Input Method)") ;
	kik_conf_add_opt( conf , 'j' , "daemon" , 0 , "daemon_mode" ,
		"start as a daemon") ;
	
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

	ml_xim_init( use_xim) ;
	
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

	if( ! ml_font_custom_init( &term_man->normal_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}
	
	font_rcfile = "mlterm/font" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		ml_font_custom_read_conf( &term_man->normal_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		ml_font_custom_read_conf( &term_man->normal_font_custom , rcpath) ;

		free( rcpath) ;
	}
	
	if( ! ml_font_custom_init( &term_man->v_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	font_rcfile = "mlterm/vfont" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		ml_font_custom_read_conf( &term_man->v_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		ml_font_custom_read_conf( &term_man->v_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ! ml_font_custom_init( &term_man->t_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	font_rcfile = "mlterm/tfont" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		ml_font_custom_read_conf( &term_man->t_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		ml_font_custom_read_conf( &term_man->t_font_custom , rcpath) ;

		free( rcpath) ;
	}

#ifdef  ANTI_ALIAS
	if( ! ml_font_custom_init( &term_man->aa_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	font_rcfile = "mlterm/aafont" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		ml_font_custom_read_conf( &term_man->aa_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		ml_font_custom_read_conf( &term_man->aa_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ! ml_font_custom_init( &term_man->vaa_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	font_rcfile = "mlterm/vaafont" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		ml_font_custom_read_conf( &term_man->vaa_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		ml_font_custom_read_conf( &term_man->vaa_font_custom , rcpath) ;

		free( rcpath) ;
	}
	
	if( ! ml_font_custom_init( &term_man->taa_font_custom , min_font_size , max_font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_font_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	font_rcfile = "mlterm/taafont" ;
	
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		ml_font_custom_read_conf( &term_man->taa_font_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		ml_font_custom_read_conf( &term_man->taa_font_custom , rcpath) ;

		free( rcpath) ;
	}
#endif
		
	if( ( value = kik_conf_get_value( conf , "compose_dec_special_font")))
	{
		if( strcmp( value , "true") == 0)
		{
			ml_compose_dec_special_font() ;
		}
	}

#ifdef  ANTI_ALIAS
	if( ( value = kik_conf_get_value( conf , "use_cp932_ucs_for_xft")) == NULL ||
		strcmp( value , "true") == 0)
	{
		ml_use_cp932_ucs_for_xft() ;
	}
#endif

	if( ! ml_color_custom_init( &term_man->color_custom))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_color_custom_init failed.\n") ;
	#endif
	
		return  0 ;
	}
	
	if( ( rcpath = kik_get_sys_rc_path( "mlterm/color")))
	{
		ml_color_custom_read_conf( &term_man->color_custom , rcpath) ;

		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( "mlterm/color")))
	{
		ml_color_custom_read_conf( &term_man->color_custom , rcpath) ;

		free( rcpath) ;
	}

	if( ! ml_keymap_init( &term_man->keymap))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_keymap_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	if( ( rcpath = kik_get_sys_rc_path( "mlterm/key")))
	{
		ml_keymap_read_conf( &term_man->keymap , rcpath) ;

		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( "mlterm/key")))
	{
		ml_keymap_read_conf( &term_man->keymap , rcpath) ;

		free( rcpath) ;
	}

	if( ! ml_termcap_init( &term_man->termcap))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_termcap_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	if( ( rcpath = kik_get_sys_rc_path( "mlterm/termcap")))
	{
		ml_termcap_read_conf( &term_man->termcap , rcpath) ;

		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( "mlterm/termcap")))
	{
		ml_termcap_read_conf( &term_man->termcap , rcpath) ;

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

	config_init( term_man , conf , argc , argv) ;

	kik_conf_delete( conf) ;

	term_man->displays = NULL ;
	term_man->num_of_displays = 0 ;
	
	if( ( term_man->terms = malloc( sizeof( ml_term_t) * term_man->max_terms)) == NULL)
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

	ml_sig_child_init() ;
	ml_add_sig_child_listener( term_man , sig_child) ;
	
	kik_alloca_garbage_collect() ;
	
	return  1 ;
}

int
ml_term_manager_final(
	ml_term_manager_t *  term_man
	)
{
	int  counter ;

	config_final( term_man) ;
	
	ml_free_word_separators() ;
	
	for( counter = 0 ; counter < term_man->num_of_terms ; counter ++)
	{
		delete_term( &term_man->terms[counter]) ;
	}

	free( term_man->terms) ;

	for( counter = 0 ; counter < term_man->num_of_displays ; counter ++)
	{
		delete_display( term_man->displays[counter]) ;
	}

	free( term_man->displays) ;

	ml_xim_final() ;
	
	ml_font_custom_final( &term_man->normal_font_custom) ;
	ml_font_custom_final( &term_man->v_font_custom) ;
	ml_font_custom_final( &term_man->t_font_custom) ;
#ifdef  ANTI_ALIAS
	ml_font_custom_final( &term_man->aa_font_custom) ;
	ml_font_custom_final( &term_man->vaa_font_custom) ;
	ml_font_custom_final( &term_man->taa_font_custom) ;
#endif

	ml_color_custom_final( &term_man->color_custom) ;
	
	ml_keymap_final( &term_man->keymap) ;
	ml_termcap_final( &term_man->termcap) ;

	ml_sig_child_final() ;
	ml_remove_sig_child_listener( term_man) ;
	
	return  1 ;
}

void
ml_term_manager_event_loop(
	ml_term_manager_t *  term_man
	)
{
	int  counter ;

	for( counter = 0 ; counter < term_man->num_of_startup_terms ; counter ++)
	{
		if( ! open_term( term_man))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " open_term() failed.\n") ;
		#endif

			if( counter == 0 && ! term_man->is_genuine_daemon)
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
