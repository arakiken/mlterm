/*
 *	$Id$
 */

#include  "ml_term_manager.h"

#include  <stdio.h>		/* sprintf */
#include  <string.h>		/* memset/memcpy */
#include  <pwd.h>		/* getpwuid */
#include  <sys/time.h>		/* timeval */
#include  <unistd.h>		/* getpid/select */
#include  <sys/wait.h>		/* wait */
#include  <signal.h>		/* kill */
#include  <stdlib.h>		/* getenv() */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* kik_str_sep/kik_str_to_int/kik_str_alloca_dup */
#include  <kiklib/kik_path.h>	/* kik_basename */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */
#include  <kiklib/kik_mem.h>	/* alloca/kik_alloca_garbage_collect/malloc */
#include  <kiklib/kik_conf.h>
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_locale.h>	/* kik_get_codeset */
#include  <mkf/mkf_charset.h>	/* mkf_charset_t */

#include  "version.h"
#include  "ml_sb_term_screen.h"
#include  "ml_term_screen.h"
#include  "ml_sig_child.h"
#include  "ml_iscii.h"


#define  FOREACH_TERMS(term_man,counter) \
	for( (counter) = 0 ; (counter) < (term_man)->num_of_terms ; (counter) ++)


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
open_new_term(
	ml_term_manager_t *  term_man
	)
{
	ml_term_screen_t *  termscr = NULL ;
	ml_sb_term_screen_t *  sb_termscr = NULL ;
	ml_font_manager_t *  font_man = NULL ;
	ml_vt100_parser_t *  vt100_parser = NULL ;
	ml_pty_t *  pty = NULL ;
	ml_window_t *  root ;
	mkf_charset_t  usascii_font_cs ;
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
		kik_warn_printf( KIK_DEBUG_TAG " failed.\n") ;
	#endif
	
		return  0 ;
	}

	if( term_man->not_use_unicode_font || term_man->iso88591_font_for_usascii)
	{
		usascii_font_cs = ml_get_usascii_font_cs( ML_ISO8859_1) ;
		usascii_font_cs_changable = 0 ;
	}
	else if( term_man->only_use_unicode_font)
	{
		usascii_font_cs = ml_get_usascii_font_cs( ML_UTF8) ;
		usascii_font_cs_changable = 0 ;
	}
	else
	{
		usascii_font_cs = ml_get_usascii_font_cs( term_man->encoding) ;
		usascii_font_cs_changable = 1 ;
	}
	
	if( ( font_man = ml_font_manager_new( term_man->win_man.display ,
		&term_man->normal_font_custom , &term_man->v_font_custom , &term_man->t_font_custom ,
	#ifdef  ANTI_ALIAS
		&term_man->aa_font_custom , &term_man->vaa_font_custom , &term_man->taa_font_custom ,
	#else
		NULL , NULL , NULL ,
	#endif
		term_man->font_size , term_man->line_space , usascii_font_cs ,
		usascii_font_cs_changable , term_man->step_in_changing_font_size)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_font_manager_new() failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( ( termscr = ml_term_screen_new( term_man->cols , term_man->rows , font_man ,
		ml_color_table_new( &term_man->color_man , term_man->fg_color , term_man->bg_color) ,
		term_man->brightness , term_man->fade_ratio , &term_man->keymap , &term_man->termcap ,
		term_man->num_of_log_lines , term_man->tab_size ,
		term_man->screen_width_ratio , term_man->screen_height_ratio ,
		term_man->xim_open_in_startup , term_man->mod_meta_mode , term_man->bel_mode ,
		term_man->copy_paste_via_ucs , term_man->pic_file_path ,
		term_man->use_transbg , term_man->font_present , term_man->use_bidi ,
		term_man->vertical_mode , term_man->big5_buggy , term_man->conf_menu_path ,
		term_man->iscii_lang)) == NULL)
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

	if( term_man->use_scrollbar)
	{
		if( ( sb_termscr = ml_sb_term_screen_new( termscr , term_man->scrollbar_view_name ,
					ml_color_table_new( &term_man->color_man ,
						term_man->sb_fg_color , term_man->sb_bg_color) ,
					term_man->use_transbg ,
					ml_term_screen_get_picture_modifier( termscr))) == NULL)
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

	if( ( vt100_parser = ml_vt100_parser_new( termscr , term_man->encoding ,
		term_man->not_use_unicode_font , term_man->only_use_unicode_font ,
		term_man->col_size_a)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_vt100_parser_new() failed.\n") ;
	#endif
		
		goto  error ;
	}

	if( ! ml_window_manager_show_root( &term_man->win_man , root ,
		term_man->x , term_man->y , term_man->geom_hint))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_window_manager_show_root() failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( term_man->app_name)
	{
		ml_set_window_name( &termscr->window , term_man->app_name) ;
		ml_set_icon_name( &termscr->window , term_man->icon_name) ;
	}
	else
	{
		if( term_man->title)
		{
			ml_set_window_name( &termscr->window , term_man->title) ;
		}
		
		if( term_man->icon_name)
		{
			ml_set_icon_name( &termscr->window , term_man->icon_name) ;
		}
	}

	env_p = env ;
	
	sprintf( wid_env , "WINDOWID=%ld" , root->my_window) ;
	*(env_p ++) = wid_env ;
	
	disp_str = XDisplayString( term_man->win_man.display) ;

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
	if( ( term = alloca( 5 + strlen( term_man->term_type) + 1)))
	{
		sprintf( term , "TERM=%s" , term_man->term_type) ;
		*(env_p ++) = term ;
	}

	/* NULL terminator */
	*env_p = NULL ;
	
	if( term_man->cmd_path && term_man->cmd_argv)
	{
		if( ( pty = ml_pty_new( term_man->cmd_path , term_man->cmd_argv , env ,
			ml_term_screen_get_cols( termscr) , ml_term_screen_get_rows( termscr))) == NULL)
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

		if( term_man->use_login_shell)
		{
			sprintf( cmd_argv[0] , "-%s" , cmd_file) ;
		}
		else
		{
			strcpy( cmd_argv[0] , cmd_file) ;
		}

		cmd_argv[1] = NULL ;

		if( ( pty = ml_pty_new( cmd_path , cmd_argv , env ,
			ml_term_screen_get_cols( termscr) , ml_term_screen_get_rows( termscr))) == NULL)
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
	term_man->terms[term_man->num_of_terms].root_window = root ;
	term_man->terms[term_man->num_of_terms].vt100_parser = vt100_parser ;
	term_man->terms[term_man->num_of_terms].font_man = font_man ;

	term_man->num_of_terms ++ ;

	return  1 ;
	
error:
	if( font_man)
	{
		ml_font_manager_delete( font_man) ;
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
	ml_term_manager_t *  term_man ,
	int  idx
	)
{
	ml_term_t *  term ;

	term = &term_man->terms[idx] ;
	
	ml_window_manager_remove_root( &term_man->win_man , term->root_window) ;
	
	ml_vt100_parser_delete( term->vt100_parser) ;

	ml_pty_delete( term->pty) ;

	ml_font_manager_delete( term->font_man) ;

	if( term_man->num_of_terms == 1)
	{
		exit( 0) ;
	}
	
	term_man->num_of_terms -- ;

	if( idx < term_man->num_of_terms)
	{
		memcpy( &term_man->terms[idx] , &term_man->terms[term_man->num_of_terms] ,
			sizeof( *term)) ;
	}

	return  1 ;
}

static int
term_closed(
	ml_term_manager_t *  term_man
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < term_man->max_terms ; counter ++)
	{
		if( term_man->dead_mask & (0x1 << counter))
		{
			delete_term( term_man , counter) ;
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
	
	exit(status) ;
}

static void
open_pty(
	void *  p
	)
{
	ml_term_manager_t *  term_man ;

	term_man = p ;
	
	open_new_term( term_man) ;
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
	
	FOREACH_TERMS(term_man,counter)
	{
		if( root_window == term_man->terms[counter].root_window)
		{
			kill( term_man->terms[counter].pty->child_pid , SIGKILL) ;

			break ;
		}
	}
}


static void
sig_child(
	void *  p ,
	pid_t  pid
	)
{
	ml_term_manager_t *  term_man ;
	int  counter ;

	term_man = p ;
	
	FOREACH_TERMS(term_man,counter)
	{
		if( pid == term_man->terms[counter].pty->child_pid)
		{
			term_man->dead_mask |= (1 << counter) ;

			return ;
		}
	}
}

static void
signal_dispatcher( int  sig)
{
#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG "signal %d is received\n" , sig) ;
#endif
	
	/* reset */
	signal( sig , SIG_DFL) ;
	
	kill( getpid() , sig) ;
}

static void
receive_next_event(
	ml_term_manager_t *  term_man
	)
{
	int  counter ;
	int  ptyfd ;
	int  xfd ;
	int  maxfd ;
	int  ret ;
	fd_set  read_fds ;

	struct timeval  tval ;

	/*
	 * flush buffer
	 */

	FOREACH_TERMS(term_man,counter)
	{
		ml_flush_pty( term_man->terms[counter].pty) ;
	}
	
	while( 1)
	{
		/* on Linux tv_usec,tv_sec members are zero cleared after select() */
		tval.tv_usec = 50000 ;	/* 0.05 sec */
		tval.tv_sec = 0 ;

		/* it is necessary to flush events here since some events may have happened in idling */
		ml_window_manager_receive_next_event( &term_man->win_man) ;
		
		FD_ZERO( &read_fds) ;

		maxfd = xfd = XConnectionNumber( term_man->win_man.display) ;
		FD_SET( xfd , &read_fds) ;

		FOREACH_TERMS(term_man,counter)
		{
			ptyfd = term_man->terms[counter].pty->fd ;
			FD_SET( ptyfd , &read_fds) ;

			if( ptyfd > maxfd)
			{
				maxfd = ptyfd ;
			}
		}

		if( ( ret = select( maxfd + 1 , &read_fds , NULL , NULL , &tval)) != 0)
		{
			break ;
		}
		
		ml_window_manager_idling( &term_man->win_man) ;
	}
	
	if( ret < 0)
	{
		/* error happened */
		
		return ;
	}

	if( FD_ISSET( xfd , &read_fds))
	{
		ml_window_manager_receive_next_event( &term_man->win_man) ;
	}

	FOREACH_TERMS(term_man,counter)
	{
		if( FD_ISSET( term_man->terms[counter].pty->fd , &read_fds))
		{
			ml_parse_vt100_sequence( term_man->terms[counter].vt100_parser) ;
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
	char *  rcpath ;
	u_int  min_font_size ;
	u_int  max_font_size ;
	char *  font_rcfile ;
	char *  value ;
	char *  kterm = "kterm" ;
	char *  xterm = "xterm" ;

	if( ( conf = kik_conf_new( "mlterm" ,
		MAJOR_VERSION , MINOR_VERSION , REVISION , PATCH_LEVEL)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " kik_conf_new() failed.\n") ;
	#endif
	
		return  0 ;
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

	kik_conf_add_opt( conf , 'd' , "display" , 0 , "display" , 
		"X server to connect") ;
	kik_conf_add_opt( conf , 'g' , "geometry" , 0 , "geometry" , 
		"size (in characters) and position") ;
	kik_conf_add_opt( conf , '1' , "wscr" , 0 , "screen_width_ratio" ,
		"screen width ratio") ;
	kik_conf_add_opt( conf , '2' , "hscr" , 0 , "screen_height_ratio" ,
		"screen height ratio") ;
	kik_conf_add_opt( conf , 'N' , "name" , 0 , "app_name" , 
		"application name") ;
	kik_conf_add_opt( conf , 'T' , "title" , 0 , "title" , 
		"title name") ;
	kik_conf_add_opt( conf , 'I' , "icon" , 0 , "icon_name" , 
		"icon name") ;
	kik_conf_add_opt( conf , 'w' , "fontsize" , 0 , "fontsize" , 
		"font size") ;
	kik_conf_add_opt( conf , 'R' , "fsrange" , 0 , "font_size_range" , 
		"font size range") ;
	kik_conf_add_opt( conf , 'o' , "lsp" , 0 , "line_space" ,
		"number of extra dots between lines") ;
	kik_conf_add_opt( conf , 'z' ,  "largesmall" , 0 , "step_in_changing_font_size" ,
		"step in changing font size in GUI configurator") ;
	kik_conf_add_opt( conf , 'l' , "sl" , 0 , "logsize" , 
		"number of scrolled lines to save") ;
	kik_conf_add_opt( conf , 'E' , "km" , 0 , "ENCODING" , 
		"character encoding") ;
	kik_conf_add_opt( conf , 'x' , "tw" , 0 , "tabsize" , 
		"tab width") ;
	kik_conf_add_opt( conf , 'f' , "fg" , 0 , "fg_color" , 
		"fg color") ;
	kik_conf_add_opt( conf , 'b' , "bg" , 0 , "bg_color" , 
		"bg color") ;
	kik_conf_add_opt( conf , 'F' , "sbfg" , 0 , "sb_fg_color" , 
		"scrollbar fg color") ;
	kik_conf_add_opt( conf , 'B' , "sbbg" , 0 , "sb_bg_color" , 
		"scrollbar bg color") ;
	kik_conf_add_opt( conf , 'H' , "bright" , 0 , "brightness" ,
		"shading ratio of background image") ;
	kik_conf_add_opt( conf , 'r' , "fade" , 0 , "fade_ratio" , 
		"fade ratio when window unfocued.") ;
	kik_conf_add_opt( conf , 'p' , "pic" , 0 , "wall_picture" , 
		"wall picture path") ;
	kik_conf_add_opt( conf , 'a' , "ac" , 0 , "col_size_of_width_a" ,
		"column numbers for Unicode EastAsianAmbiguous character") ;
	kik_conf_add_opt( conf , 'y' , "term" , 0 , "termtype" , 
		"terminal type") ;
	kik_conf_add_opt( conf , 'S' , "sbview" , 0 , "scrollbar_view_name" , 
		"scrollbar view name") ;
	kik_conf_add_opt( conf , 'M' , "menu" , 0 , "conf_menu_path" ,
		"command path of mlconfig (GUI configurator)") ;
	kik_conf_add_opt( conf , 'K' , "maxptys" , 0 , "max_ptys" ,
		"max ptys to use") ;
	kik_conf_add_opt( conf , 'P' , "ptys" , 0 , "ptys" , 
		"num of ptys to use in start up") ;
	kik_conf_add_opt( conf , 'W' , "sep" , 0 , "word_separators" , 
		"word separator characters") ;
	kik_conf_add_opt( conf , 'k' , "meta" , 0 , "mod_meta_mode" , 
		"mode in pressing meta key") ;
	kik_conf_add_opt( conf , '7' , "bel" , 0 , "bel_mode" , 
		"bel(0x07) mode") ;
	kik_conf_add_opt( conf , 'G' , "vertical" , 0 , "vertical_mode" ,
		"vertical view mode") ;
	kik_conf_add_opt( conf , 'C' , "iscii" , 0 , "iscii_lang" , 
		"language to be used in ISCII encoding") ;
	kik_conf_add_opt( conf , 'L' , "ls" , 1 , "use_login_shell" , 
		"turn on login shell") ;
	kik_conf_add_opt( conf , 'i' , "xim" , 1 , "use_xim" , 
		"use XIM (X Input Method)") ;
	kik_conf_add_opt( conf , 't' , "transbg" , 1 , "use_transbg" , 
		"use transparent background") ;
	kik_conf_add_opt( conf , 's' , "sb" , 1 , "use_scrollbar" , 
		"use scrollbar") ;
	kik_conf_add_opt( conf , 'm' , "comb" , 1 , "use_combining" , 
		"use combining characters") ;
	kik_conf_add_opt( conf , '8' , "88591" , 1 , "iso88591_font_for_usascii" ,
		"use ISO-8859-1 font for US-ASCII part of every encodings") ;
	kik_conf_add_opt( conf , 'n' , "noucsfont" , 1 , "not_use_unicode_font" ,
		"use non-Unicode fonts even in UTF-8 mode") ;
	kik_conf_add_opt( conf , 'u' , "onlyucsfont" , 1 , "only_use_unicode_font" ,
		"use a Unicode font even in non-UTF-8 modes") ;
	kik_conf_add_opt( conf , 'U' , "viaucs" , 1 , "copy_paste_via_ucs" ,
		"process received strings via ucs.") ;
	kik_conf_add_opt( conf , 'X' , "openim" , 1 , "xim_open_in_startup" , 
		"open xim in starting up.") ;
	kik_conf_add_opt( conf , 'D' , "bi" , 1 , "use_bidi" , 
		"use bidi") ;
	kik_conf_add_opt( conf , '5' , "big5bug" , 1 , "big5_buggy" ,
		"support buggy Big5 CTEXT in XFree86 4.1 or earlier") ;
	kik_conf_add_opt( conf , 'V' , "varwidth" , 1 , "use_variable_column_width" ,
		"variable column width") ;
#ifdef  ANTI_ALIAS
	kik_conf_add_opt( conf , 'c' , "cp932" , 1 , "use_cp932_ucs_for_xft" , 
		"CP932 mapping table for JISX0208-Unicode conversion") ;
	kik_conf_add_opt( conf , 'A' , "aa" , 1 , "use_anti_alias" , 
		"use anti alias font") ;
#endif

	kik_conf_set_end_opt( conf , 'e' , NULL , "exec_cmd" , 
		"execute external command") ;

	kik_conf_parse_args( conf , &argc , &argv) ;


	/*
	 * window manager
	 */

	use_xim = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "use_xim")))
	{
		if( strcmp( value , "false") == 0)
		{
			use_xim = 0 ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "display")))
	{
		if( ! ml_window_manager_init( &term_man->win_man , value , use_xim) &&
			! ml_window_manager_init( &term_man->win_man , "" , use_xim))
		{
			return  0 ;
		}
	}
	else
	{
		if( ! ml_window_manager_init( &term_man->win_man , "" , use_xim))
		{
			return  0 ;
		}
	}

	
	/*
	 * font
	 */

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
		
	term_man->font_present = 0 ;

	if( ( value = kik_conf_get_value( conf , "use_variable_column_width")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->font_present |= FONT_VAR_WIDTH ;
		}
	}

	term_man->line_space = 0 ;

	if( ( value = kik_conf_get_value( conf , "line_space")))
	{
		u_int  size ;

		if( kik_str_to_uint( &size , value))
		{
			term_man->line_space = size ;
		}
		else
		{
			kik_msg_printf( "line space %s is not valid.\n" , value) ;
		}
	}

	term_man->step_in_changing_font_size = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "step_in_changing_font_size")))
	{
		u_int  size ;
		
		if( kik_str_to_uint( &size , value))
		{
			term_man->step_in_changing_font_size = size ;
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
			term_man->font_present |= FONT_AA ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "use_cp932_ucs_for_xft")) == NULL ||
		strcmp( value , "true") == 0)
	{
		ml_use_cp932_ucs_for_xft() ;
	}
#endif

	/*
	 * color
	 */
	
	if( ! ml_color_manager_init( &term_man->color_man ,
		term_man->win_man.display , term_man->win_man.screen))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_color_manager_init failed.\n") ;
	#endif
	
		return  0 ;
	}

	if( ( rcpath = kik_get_sys_rc_path( "mlterm/color")))
	{
		ml_color_manager_read_conf( &term_man->color_man , rcpath) ;

		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( "mlterm/color")))
	{
		ml_color_manager_read_conf( &term_man->color_man , rcpath) ;

		free( rcpath) ;
	}
	
	if( ! ml_color_manager_load( &term_man->color_man))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_color_manager_load failed.\n") ;
	#endif
	
		return  0 ;
	}

	term_man->fg_color = MLC_BLACK ;
	
	if( ( value = kik_conf_get_value( conf , "fg_color")))
	{
		ml_color_t  fg_color ;

		if( ( fg_color = ml_get_color( value)) != MLC_UNKNOWN_COLOR)
		{
			term_man->fg_color = fg_color ;
		}
	}

	term_man->bg_color = MLC_WHITE ;
	
	if( ( value = kik_conf_get_value( conf , "bg_color")))
	{
		ml_color_t  bg_color ;

		if( ( bg_color = ml_get_color( value)) != MLC_UNKNOWN_COLOR)
		{
			term_man->bg_color = bg_color ;
		}
	}

	if( term_man->fg_color == term_man->bg_color)
	{
		u_short  red ;
		u_short  blue ;
		u_short  green ;
		
		kik_msg_printf( "fg and bg colors are the same. is this ok ?\n") ;
		
		/*
		 * if fg and bg colors are the same , users will want to see characters anyway.
		 */
		
		ml_get_color_rgb( &term_man->color_man , &red , &blue , &green , term_man->fg_color) ;

		red = (red > 0x8000) ? 0x0 : 0xffff ;
		green = (green > 0x8000) ? 0x0 : 0xffff ;
		blue = (blue > 0x8000) ? 0x0 : 0xffff ;
		
		ml_color_manager_change_rgb( &term_man->color_man , MLC_PRIVATE_BG_COLOR ,
			red , blue , green) ;
		
		term_man->bg_color = MLC_PRIVATE_BG_COLOR ;
	}
	
	if( ( value = kik_conf_get_value( conf , "sb_fg_color")))
	{
		term_man->sb_fg_color = ml_get_color( value) ;
	}
	else
	{
		term_man->sb_fg_color = MLC_UNKNOWN_COLOR ;
	}

	if( ( value = kik_conf_get_value( conf , "sb_bg_color")))
	{
		term_man->sb_bg_color = ml_get_color( value) ;
	}
	else
	{
		term_man->sb_bg_color = MLC_UNKNOWN_COLOR ;
	}

	term_man->brightness = 100 ;

	if( ( value = kik_conf_get_value( conf , "brightness")))
	{
		u_int  brightness ;
		
		if( kik_str_to_uint( &brightness , value))
		{
			term_man->brightness = brightness ;
		}
		else
		{
			kik_msg_printf( "shade ratio %s is not valid.\n" , value) ;
		}
	}
	
	term_man->fade_ratio = 100 ;
	
	if( ( value = kik_conf_get_value( conf , "fade_ratio")))
	{
		u_int  fade_ratio ;
		
		if( kik_str_to_uint( &fade_ratio , value))
		{
			term_man->fade_ratio = fade_ratio ;
		}
		else
		{
			kik_msg_printf( "fade ratio %s is not valid.\n" , value) ;
		}
	}

	/*
	 * keymap and termcap
	 */

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
	 * miscellaneous
	 */

	if( ( value = kik_conf_get_value( conf , "termtype")))
	{
		if( strcasecmp( value , kterm) == 0)
		{
			term_man->term_type = kterm ;
		}
		else if( strcasecmp( value , xterm) == 0)
		{
			term_man->term_type = xterm ;
		}
		else
		{
			kik_msg_printf(
				"supported terminal types are only xterm or kterm , xterm is used.\n") ;

			term_man->term_type = xterm ;
		}
	}
	else
	{
		term_man->term_type = xterm ;
	}

	term_man->app_name = NULL ;

	if( ( value = kik_conf_get_value( conf , "app_name")))
	{
		term_man->app_name = strdup( value) ;
	}

	term_man->title = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "title")))
	{
		term_man->title = strdup( value) ;
	}

	term_man->icon_name = NULL ;

	if( ( value = kik_conf_get_value( conf , "icon_name")))
	{
		term_man->icon_name = strdup( value) ;
	}
	
	/* using default value */
	term_man->scrollbar_view_name = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "scrollbar_view_name")))
	{
		term_man->scrollbar_view_name = strdup( value) ;
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

	term_man->x = 0 ;
	term_man->y = 0 ;
	term_man->cols = 80 ;
	term_man->rows = 30 ;
	if( ( value = kik_conf_get_value( conf , "geometry")))
	{
		/* For each value not found, the argument is left unchanged.(see man XParseGeometry(3)) */
		term_man->geom_hint = XParseGeometry( value , &term_man->x , &term_man->y ,
						&term_man->cols , &term_man->rows) ;

		if( term_man->cols == 0 || term_man->rows == 0)
		{
			kik_msg_printf( "geometry option %s is illegal.\n" , value) ;
			
			term_man->cols = 80 ;
			term_man->rows = 30 ;
		}
	}
	else
	{
		term_man->geom_hint = 0 ;
	}

	term_man->screen_width_ratio = 100 ;
	
	if( ( value = kik_conf_get_value( conf , "screen_width_ratio")))
	{
		u_int  ratio ;

		if( kik_str_to_uint( &ratio , value))
		{
			term_man->screen_width_ratio = ratio ;
		}
	}

	term_man->screen_height_ratio = 100 ;
	
	if( ( value = kik_conf_get_value( conf , "screen_height_ratio")))
	{
		u_int  ratio ;

		if( kik_str_to_uint( &ratio , value))
		{
			term_man->screen_height_ratio = ratio ;
		}
	}
	
	if( ( value = kik_conf_get_value( conf , "fontsize")) == NULL)
	{
		term_man->font_size = 16 ;
	}
	else if( ! kik_str_to_uint( &term_man->font_size , value))
	{
		kik_msg_printf( "font size %s is not valid.\n" , value) ;

		/* default value is used. */
		term_man->font_size = 16 ;
	}

	if( term_man->font_size > max_font_size)
	{
		kik_msg_printf( "font size %d is too large. %d is used.\n" ,
			term_man->font_size , max_font_size) ;
		
		term_man->font_size = max_font_size ;
	}
	else if( term_man->font_size < min_font_size)
	{
		kik_msg_printf( "font size %d is too small. %d is used.\n" ,
			term_man->font_size , min_font_size) ;
			
		term_man->font_size = min_font_size ;
	}

	if( ( value = kik_conf_get_value( conf , "logsize")) == NULL)
	{
		term_man->num_of_log_lines = 128 ;
	}
	else if( ! kik_str_to_uint( &term_man->num_of_log_lines , value))
	{
		kik_msg_printf( "log size %s is not valid.\n" , value) ;

		/* default value is used. */
		term_man->num_of_log_lines = 128 ;
	}

	if( ( value = kik_conf_get_value( conf , "tabsize")) == NULL)
	{
		/* default value is used. */
		term_man->tab_size = 8 ;
	}
	else if( ! kik_str_to_uint( &term_man->tab_size , value))
	{
		kik_msg_printf( "tab size %s is not valid.\n" , value) ;

		/* default value is used. */
		term_man->tab_size = 8 ;
	}
	
	term_man->use_login_shell = 0 ;
	
	if( ( value = kik_conf_get_value( conf , "use_login_shell")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->use_login_shell = 1 ;
		}
	}

	term_man->big5_buggy = 0 ;

	if( ( value = kik_conf_get_value( conf , "big5_buggy")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->big5_buggy = 1 ;
		}
	}

	term_man->use_scrollbar = 0 ;

	if( ( value = kik_conf_get_value( conf , "use_scrollbar")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->use_scrollbar = 1 ;
		}
	}

	term_man->iso88591_font_for_usascii = 0 ;

	if( ( value = kik_conf_get_value( conf , "iso88591_font_for_usascii")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->iso88591_font_for_usascii = 1 ;
		}
	}

	term_man->not_use_unicode_font = 0 ;

	if( ( value = kik_conf_get_value( conf , "not_use_unicode_font")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->not_use_unicode_font = 1 ;
		}
	}

	term_man->only_use_unicode_font = 0 ;

	if( ( value = kik_conf_get_value( conf , "only_use_unicode_font")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->only_use_unicode_font = 1 ;
		}
	}

	if( term_man->only_use_unicode_font && term_man->not_use_unicode_font)
	{
		kik_msg_printf(
			"only_use_unicode_font and not_use_unicode_font options cannot be used "
			"at the same time.\n") ;

		/* default values are used */
		term_man->only_use_unicode_font = 0 ;
		term_man->not_use_unicode_font = 0 ;
	}

	term_man->copy_paste_via_ucs = 0 ;

	if( ( value = kik_conf_get_value( conf , "copy_paste_via_ucs")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->copy_paste_via_ucs = 1 ;
		}
	}
	
	/* default value is used */
	term_man->col_size_a = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "col_size_of_width_a")))
	{
		u_int  col_size_a ;
		
		if( kik_str_to_uint( &col_size_a , value))
		{
			term_man->col_size_a = col_size_a ;
		}
		else
		{
			kik_msg_printf( "col size of width a %s is not valid.\n" , value) ;
		}
	}

	term_man->pic_file_path = NULL ;

	if( ( value = kik_conf_get_value( conf , "wall_picture")))
	{
		term_man->pic_file_path = strdup( value) ;
	}

	term_man->use_transbg = 0 ;

	if( ( value = kik_conf_get_value( conf , "use_transbg")))
	{
		if( strcmp( value , "true") == 0)
		{
			term_man->use_transbg = 1 ;
		}
	}

	if( term_man->pic_file_path && term_man->use_transbg)
	{
		kik_msg_printf(
			"wall picture and transparent background cannot be used at the same time.\n") ;

		/* using wall picture */
		term_man->use_transbg = 0 ;
	}

	if( ( term_man->encoding = ml_get_char_encoding( kik_get_codeset())) == ML_UNKNOWN_ENCODING)
	{
		term_man->encoding = ML_ISO8859_1 ;
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
				term_man->encoding = encoding ;
			}
		}
	}

	term_man->conf_menu_path = NULL ;

	if( ( value = kik_conf_get_value( conf , "conf_menu_path")))
	{
		term_man->conf_menu_path = strdup( value) ;
	}

	term_man->xim_open_in_startup = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "xim_open_in_startup")))
	{
		if( strcmp( value , "false") == 0)
		{
			term_man->xim_open_in_startup = 0 ;
		}
	}

	term_man->use_bidi = 1 ;

	if( ( value = kik_conf_get_value( conf , "use_bidi")))
	{
		if( strcmp( value , "false") == 0)
		{
			term_man->use_bidi = 0 ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "mod_meta_mode")))
	{
		term_man->mod_meta_mode = ml_get_mod_meta_mode( value) ;
	}
	else
	{
		term_man->mod_meta_mode = MOD_META_NONE ;
	}

	if( ( value = kik_conf_get_value( conf , "bel_mode")))
	{
		term_man->bel_mode = ml_get_bel_mode( value) ;
	}
	else
	{
		term_man->bel_mode = BEL_SOUND ;
	}

	term_man->vertical_mode = 0 ;
	
	if( ( value = kik_conf_get_value( conf , "vertical_mode")))
	{
		term_man->vertical_mode = ml_get_vertical_mode( value) ;
	}

	term_man->max_terms = 5 ;

	if( ( value = kik_conf_get_value( conf , "max_ptys")))
	{
		u_int  max ;

		/*
		 * max_ptys are 1 - 32.
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
		
		if( ! kik_str_to_uint( &ptys , value) || ptys > term_man->max_terms)
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

	term_man->iscii_lang = ISCIILANG_MALAYALAM ;
	
	if( ( value = kik_conf_get_value( conf , "iscii_lang")))
	{
		ml_iscii_lang_t  lang ;
		
		if( ( lang = ml_iscii_get_lang( strdup( value))) != ISCIILANG_UNKNOWN)
		{
			term_man->iscii_lang = lang ;
		}
	}

	term_man->cmd_path = NULL ;
	term_man->cmd_argv = NULL ;

	if( ( value = kik_conf_get_value( conf , "exec_cmd")) && strcmp( value , "true") == 0)
	{
		term_man->cmd_path = argv[0] ;

		if( ( term_man->cmd_argv = malloc( sizeof( char*) * (argc + 2))) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
		#endif
		
			term_man->cmd_path = NULL ;
			term_man->cmd_argv = NULL ;
		}
		else
		{
			memcpy( &term_man->cmd_argv[0] , argv , sizeof( char*) * argc) ;
			term_man->cmd_argv[argc + 1] = NULL ;
		}
	}
	
	kik_conf_delete( conf) ;

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

	signal( SIGHUP , signal_dispatcher) ;
	signal( SIGINT , signal_dispatcher) ;
	signal( SIGQUIT , signal_dispatcher) ;
	signal( SIGTERM , signal_dispatcher) ;

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

	free( term_man->app_name) ;
	free( term_man->title) ;
	free( term_man->icon_name) ;
	free( term_man->conf_menu_path) ;
	free( term_man->pic_file_path) ;
	free( term_man->cmd_argv) ;
	free( term_man->scrollbar_view_name) ;
	
	ml_free_word_separators() ;
	
	FOREACH_TERMS(term_man,counter)
	{
		ml_vt100_parser_delete( term_man->terms[counter].vt100_parser) ;
		ml_pty_delete( term_man->terms[counter].pty) ;
		ml_font_manager_delete( term_man->terms[counter].font_man) ;
	}

	free( term_man->terms) ;
	
	ml_window_manager_final( &term_man->win_man) ;
	ml_color_manager_final( &term_man->color_man) ;
	
	ml_font_custom_final( &term_man->normal_font_custom) ;
	ml_font_custom_final( &term_man->v_font_custom) ;
	ml_font_custom_final( &term_man->t_font_custom) ;
#ifdef  ANTI_ALIAS
	ml_font_custom_final( &term_man->aa_font_custom) ;
	ml_font_custom_final( &term_man->vaa_font_custom) ;
	ml_font_custom_final( &term_man->taa_font_custom) ;
#endif
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
		if( ! open_new_term( term_man))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " open_new_term() failed.\n") ;
		#endif

			break ;
		}
	}

	while( 1)
	{
		kik_alloca_begin_stack_frame() ;

		receive_next_event( term_man) ;

		if( term_man->dead_mask)
		{
			term_closed( term_man) ;
		}

		kik_alloca_end_stack_frame() ;
	}
}
