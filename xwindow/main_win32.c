/*
 *	$Id$
 */

#include <stdio.h>
#include <kiklib/kik_debug.h>
#include <kiklib/kik_sig_child.h>
#include <kiklib/kik_util.h>
#include <kiklib/kik_conf_io.h>
#include <ml_term.h>
#include <ml_str_parser.h>
#include <ml_char_encoding.h>
#include "x_window_manager.h"
#include "x_screen.h"

x_screen_t *  screen ;
ml_term_t *  term ;
x_color_config_t  color_config ;
x_shortcut_t  shortcut ;
x_termcap_t  termcap ;
x_window_manager_t  win_man ;


#if  0
#define  __DEBUG
#endif

#define  FONT_SIZE  12
#define  COLUMNS  80
#define  LINES    30


static void
usage(void)
{
  	kik_msg_printf( "Usage: mlterm [-h/-telnet/-ssh/-rlogin/-raw] [host]\n") ;
  	kik_msg_printf( "(Without any option, mlterm start with sh.exe or cmd.exe.)\n") ;
}

static void
sig_child(
  	void *  p ,
  	pid_t  pid
  	)
{
	x_window_t *  win = p ;

#ifdef  DEBUG
  	kik_debug_printf( KIK_DEBUG_TAG " SIG_CHILD received.\n") ;
#endif

	PostMessage(win->my_window, WM_DESTROY, 0, 0) ;
}

static void
set_font_config(
	char *  file ,
	char *  key ,
	char *  val ,
	int  save
	)
{
	if( ! x_customize_font_file( file, key, val, save))
	{
		return  ;
	}
	
	x_font_cache_unload_all() ;

	x_screen_reset_view( screen) ;
}

static void
set_color_config(
	char *  file ,
	char *  key ,
	char *  val ,
	int  save
	)
{
	kik_debug_printf( "HELO\n") ;
	if( ! x_customize_color_file( &color_config, key, val, save))
	{
		return  ;
	}
	
	x_color_cache_unload_all() ;

	x_screen_reset_view( screen) ;
}

void
close_screen(
	void *  p ,
	x_screen_t *  screen
	)
{
        PostQuitMessage(0) ;
}

VOID CALLBACK timer_proc(
	HWND  hwnd,
	UINT  msg,
	UINT  timerid,
	DWORD  time
	)
{
	x_window_manager_idling( &win_man) ;
}

int PASCAL WinMain(
  	HINSTANCE hinst,
  	HINSTANCE hprev,
  	char *  cmdline,
  	int cmdshow
	)
{
  	char *  p ;
  	char *  protocol ;
  	char *  host ;
	Display  disp ;
	x_font_manager_t *  font_man ;
	x_color_manager_t *  color_man ;
	x_system_event_listener_t  sys_listener ;
  	char  wid[50] ;
        char  lines[12] ;
        char  columns[15] ;
	MSG  msg ;
	
  	protocol = NULL ;
  	host = NULL ;
  
	p = cmdline ;

	if( ! kik_locale_init(""))
	{
		kik_msg_printf( "locale settings failed.\n") ;
	}

#if  1
        kik_debug_printf( "cmd_line => %s\n", cmdline) ;
	kik_debug_printf( "GetCommandLine() => %s\n", GetCommandLine()) ;
#endif
  	while( 1)
        {
  		while( *p != '\0' && *p != ' ')
        	{
        		p++ ;
        	}

          	if( *p == '\0')
                {
                  	break ;
                }
  		*(p++) = '\0' ;

          	if( strcmp( cmdline, "-telnet") == 0 || strcmp( cmdline, "-ssh") == 0 ||
                    	strcmp( cmdline, "-rlogin") == 0 || strcmp( cmdline, "-raw") == 0)
                {
                  	protocol = cmdline ;
                }
          	else if( strcmp( cmdline, "-h") == 0)
                {
                  	usage() ;

                 	return  0 ;
                }
          
  		while( *p == ' ')
        	{
          		p++ ;
        	}

          	if( *p == '\0')
                {
                  	break ;
                }
        	cmdline = p ;
        }

  	if( *cmdline != '\0')
        {
  		host = cmdline ;
        }

#if  0
  	kik_debug_printf( "proto:%s host:%s\n", protocol, host) ;
#endif

	disp.hinst = hinst ;

  	term = ml_term_new( COLUMNS, LINES, 8, 50, ml_get_char_encoding( "auto"), 1,
		NO_UNICODE_FONT_POLICY, 1, 0, 1, 0, 1, 0, BSM_VOLATILE, 0, ISCIILANG_UNKNOWN) ;

	if( ! x_window_manager_init( &win_man, &disp))
	{
		kik_warn_printf( " x_window_manager_init failed.\n") ;

		return  -1 ;
	}

	kik_set_sys_conf_dir( "") ;

	if( ! ( font_man = x_font_manager_new( &disp, TYPE_XCORE, 0, FONT_SIZE,
				ISO8859_1_R, 0, 0, 1)))
	{
		kik_warn_printf( " x_font_manager_new failed.\n") ;

		return  -1 ;
	}

	if( ! x_color_config_init( &color_config))
	{
		kik_warn_printf( " x_color_config_init failed.\n") ;

		return  -1 ;
	}

	if( ! ( color_man = x_color_manager_new( &disp, 0, &color_config, "black", "white",
				NULL, NULL)))
	{
		kik_warn_printf( " x_color_manager_new failed.\n") ;

		return  -1 ;
	}

	if( ! x_termcap_init( &termcap))
	{
		kik_warn_printf( " x_termcap_init failed.\n") ;

		return  -1 ;
	}

	if( ! x_shortcut_init( &shortcut))
	{
		kik_warn_printf( " x_shortcut_init failed.\n") ;

		return  -1 ;
	}

	if( ! ( screen = x_screen_new( term, font_man, color_man,
				x_termcap_get_entry( &termcap, "xterm") ,
				100, 100, 100, 100, &shortcut, 100, 100,
				NULL, MOD_META_SET_MSB, BEL_SOUND, 0,
				NULL, 0, 0, 0, NULL, NULL, NULL,
				1, 0, 0, NULL)))
	{
		kik_warn_printf( " x_screen_new failed.\n") ;

		return  -1 ;
	}

	/* Override config event listener */
	screen->config_listener.set_font = set_font_config ;
	screen->config_listener.set_color = set_color_config ;

	sys_listener.exit = NULL ;
	sys_listener.open_screen = NULL ;
	sys_listener.close_screen = close_screen ;
	sys_listener.open_pty = NULL ;
	sys_listener.next_pty = NULL ;
	sys_listener.prev_pty = NULL ;
	sys_listener.close_pty = NULL ;
	sys_listener.pty_closed = NULL ;
	sys_listener.get_pty = NULL ;
	sys_listener.pty_list = NULL ;

	x_set_system_listener( screen, &sys_listener) ;

	if( ! x_window_manager_show_root( &win_man, &screen->window, 0, 0, 0, "mlterm"))
	{
		kik_warn_printf( " x_window_manager_show_root failed.\n") ;
		
		return  0 ;
	}

  	snprintf( wid, 50, "WINDOWID=%d", screen->window.my_window) ;
        snprintf( lines, 12, "LINES=%d", LINES) ;
        snprintf( columns, 15, "COLUMNS=%d", COLUMNS) ;

  	if( protocol)
        {
        	char *  argv[] = { protocol, host, NULL } ;
          	char *  env[] = { wid, NULL } ;
          	ml_term_open_pty( term, "plink.exe", argv, env, NULL) ;
        }
  	else
  	{
          	HANDLE  file ;

          	file = CreateFile( "C:\\msys\\1.0\\bin\\sh.exe", GENERIC_READ,
				FILE_SHARE_READ|FILE_SHARE_WRITE,
                		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL) ;
          	if( file == INVALID_HANDLE_VALUE)
                {
                  	ml_term_open_pty( term, "C:\\windows\\system32\\cmd.exe",NULL,NULL,NULL) ;
                }
          	else
                {
        		char *  env[] = { lines, columns, wid, NULL } ;
        		char *  argv[] = { "--login", "-i" , NULL } ;

  			ml_term_open_pty( term, "C:\\msys\\1.0\\bin\\sh.exe", argv, env, NULL) ;
                  	CloseHandle(file) ;
                }
        }

  	kik_sig_child_init() ;
  	kik_add_sig_child_listener( &screen->window, sig_child) ;

	/* x_window_manager_idling() called in 0.1sec. */
	SetTimer( NULL, 0, 100, timer_proc) ;

  	while( GetMessage( &msg,NULL,0,0))
        {
		if( ml_term_parse_vt100_sequence( term))
		{
			while( ml_term_parse_vt100_sequence( term)) ;
		}
		
          	TranslateMessage(&msg) ;
          	DispatchMessage(&msg) ;
        }

	x_window_manager_final( &win_man) ;
	x_font_manager_delete( font_man) ;
	x_color_manager_delete( color_man) ;
	x_color_config_final( &color_config) ;
	x_termcap_final( &termcap) ;
	x_shortcut_final( &shortcut) ;
	ml_term_delete(term) ;
		
  	kik_sig_child_final() ;
	kik_locale_final() ;

#ifdef  KIK_DEBUG
	kik_mem_free_all() ;
#endif

  	return  0 ;
}
