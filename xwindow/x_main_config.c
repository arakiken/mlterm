/*
 *	$Id$
 */

#include  "x_main_config.h"

#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_mem.h>	/* malloc/realloc */
#include  <kiklib/kik_str.h>	/* kik_str_to_uint */


/* --- static variables --- */

static char *  true = "true" ;
static char *  false = "false" ;


/* --- global functions --- */

int
x_prepare_for_main_config(
	kik_conf_t *  conf
	)
{
	char *  rcpath ;
	
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

	kik_conf_add_opt( conf , '#' , "initstr" , 0 , "init_str" ,
		"initial string sent to pty") ;
	kik_conf_add_opt( conf , '$' , "mc" , 0 , "click_interval" ,
		"click interval(milisecond) [250]") ;
	kik_conf_add_opt( conf , '%' , "logseq" , 1 , "logging_vt_seq" ,
		"enable logging vt100 sequence [false]") ;

#ifndef  USE_WIN32GUI
	kik_conf_add_opt( conf , '&' , "borderless" , 1 , "borderless" ,
		"override redirect [false]") ;
#if  defined(USE_TYPE_XCORE)
	kik_conf_add_opt( conf , '*' , "type" , 0 , "type_engine" ,
		"type engine [xcore]") ;
#elif  defined(USE_TYPE_XFT)
	kik_conf_add_opt( conf , '*' , "type" , 0 , "type_engine" ,
		"type engine [xft]") ;
#endif
#endif	/* USE_WIN32GUI */

	kik_conf_add_opt( conf , '1' , "wscr" , 0 , "screen_width_ratio" ,
		"screen width in percent against font width [100]") ;
	kik_conf_add_opt( conf , '2' , "hscr" , 0 , "screen_height_ratio" ,
		"screen height in percent against font height [100]") ;
	kik_conf_add_opt( conf , '3' , "contrast" , 0 , "contrast" ,
		"contrast of background image in percent [100]") ;
	kik_conf_add_opt( conf , '4' , "gamma" , 0 , "gamma" ,
		"gamma of background image in percent [100]") ;
	kik_conf_add_opt( conf , '5' , "big5bug" , 1 , "big5_buggy" ,
		"manage buggy Big5 CTEXT in XFree86 4.1 or earlier [false]") ;
	kik_conf_add_opt( conf , '6' , "stbs" , 1 , "static_backscroll_mode" ,
		"screen is static under backscroll mode [false]") ;
	kik_conf_add_opt( conf , '7' , "bel" , 0 , "bel_mode" , 
		"bel (0x07) mode (none/sound/visual) [sound]") ;
	kik_conf_add_opt( conf , '8' , "88591" , 1 , "iso88591_font_for_usascii" ,
		"use ISO-8859-1 font for ASCII part of any encoding [false]") ;
	kik_conf_add_opt( conf , '9' , "crfg" , 0 , "cursor_fg_color" ,
		"cursor foreground color") ;
	kik_conf_add_opt( conf , '0' , "crbg" , 0 , "cursor_bg_color" ,
		"cursor background color") ;
#ifdef  USE_TYPE_XFT
	kik_conf_add_opt( conf , 'A' , "aa" , 1 , "use_anti_alias" , 
		"forcibly use anti alias font by using Xft") ;
#endif
	kik_conf_add_opt( conf , 'B' , "sbbg" , 0 , "sb_bg_color" , 
		"scrollbar background color") ;
#ifdef  USE_IND
	kik_conf_add_opt( conf , 'C' , "iscii" , 0 , "iscii_lang" , 
		"language to be used in ISCII encoding") ;
#endif
#ifdef  USE_FRIBIDI
	kik_conf_add_opt( conf , 'D' , "bi" , 1 , "use_bidi" , 
		"use bidi (bi-directional text) [true]") ;
#endif
	kik_conf_add_opt( conf , 'E' , "km" , 0 , "ENCODING" , 
		"character encoding (AUTO/ISO-8859-*/EUC-*/UTF-8/...) [AUTO]") ;
	kik_conf_add_opt( conf , 'F' , "sbfg" , 0 , "sb_fg_color" , 
		"scrollbar foreground color") ;
	kik_conf_add_opt( conf , 'G' , "vertical" , 0 , "vertical_mode" ,
		"vertical mode (none/cjk/mongol) [none]") ;
	kik_conf_add_opt( conf , 'H' , "bright" , 0 , "brightness" ,
		"brightness of background image in percent [100]") ;
#ifndef  USE_WIN32GUI
	kik_conf_add_opt( conf , 'I' , "icon" , 0 , "icon_name" , 
		"icon name") ;
#endif
	kik_conf_add_opt( conf , 'J' , "dyncomb" , 1 , "use_dynamic_comb" ,
		"use dynamic combining [false]") ;
	kik_conf_add_opt( conf , 'K' , "metakey" , 0 , "mod_meta_key" ,
		"specify meta key [none]") ;
	kik_conf_add_opt( conf , 'L' , "ls" , 1 , "use_login_shell" , 
		"turn on login shell [false]") ;
	kik_conf_add_opt( conf , 'M' , "menu" , 0 , "conf_menu_path_3" ,
		"command path of mlconfig (GUI configurator)") ;
	kik_conf_add_opt( conf , 'N' , "name" , 0 , "app_name" , 
		"application name") ;
	kik_conf_add_opt( conf , 'O' , "sbmod" , 0 , "scrollbar_mode" ,
		"scrollbar mode (none/left/right) [none]") ;
	kik_conf_add_opt( conf , 'Q' , "vcur" , 1 , "use_vertical_cursor" ,
		"rearrange cursor key for vertical mode [false]") ;
	kik_conf_add_opt( conf , 'S' , "sbview" , 0 , "scrollbar_view_name" , 
		"scrollbar view name (simple/sample/...) [simple]") ;
	kik_conf_add_opt( conf , 'T' , "title" , 0 , "title" , 
		"title name") ;
	kik_conf_add_opt( conf , 'U' , "viaucs" , 1 , "receive_string_via_ucs" ,
		"process received (pasted) strings via Unicode [false]") ;
	kik_conf_add_opt( conf , 'V' , "varwidth" , 1 , "use_variable_column_width" ,
		"variable column width (for proportional/ISCII) [false]") ;
	kik_conf_add_opt( conf , 'X' , "alpha" , 0 , "alpha" ,
		"alpha blending for translucent [210]") ;
	kik_conf_add_opt( conf , 'Z' , "multicol" , 1 , "use_multi_column_char" ,
		"fullwidth character occupies two logical columns [true]") ;
	kik_conf_add_opt( conf , 'a' , "ac" , 0 , "col_size_of_width_a" ,
		"columns for Unicode \"EastAsianAmbiguous\" character [1]") ;
	kik_conf_add_opt( conf , 'b' , "bg" , 0 , "bg_color" , 
		"background color") ;
#ifndef  USE_WIN32GUI
	kik_conf_add_opt( conf , 'd' , "display" , 0 , "display" , 
		"X server to connect") ;
#endif
	kik_conf_add_opt( conf , 'f' , "fg" , 0 , "fg_color" , 
		"foreground color") ;
	kik_conf_add_opt( conf , 'g' , "geometry" , 0 , "geometry" , 
		"size (in characters) and position [80x24]") ;
	kik_conf_add_opt( conf , 'k' , "meta" , 0 , "mod_meta_mode" , 
		"mode in pressing meta key (none/esc/8bit) [none]") ;
	kik_conf_add_opt( conf , 'l' , "sl" , 0 , "logsize" , 
		"number of backlog (scrolled lines to save) [128]") ;
	kik_conf_add_opt( conf , 'm' , "comb" , 1 , "use_combining" , 
		"use combining characters [true]") ;
	kik_conf_add_opt( conf , 'n' , "noucsfont" , 1 , "not_use_unicode_font" ,
		"use non-Unicode fonts even in UTF-8 mode [false]") ;
	kik_conf_add_opt( conf , 'o' , "lsp" , 0 , "line_space" ,
		"extra space between lines in pixels [0]") ;
#ifdef  USE_EXT_IMAGELIB
	kik_conf_add_opt( conf , 'p' , "pic" , 0 , "wall_picture" , 
		"path for wallpaper (background) image") ;
#endif
	kik_conf_add_opt( conf , 'q' , "extkey" , 1 , "use_extended_scroll_shortcut" ,
		"use extended scroll shortcut keys [false]") ;
	kik_conf_add_opt( conf , 'r' , "fade" , 0 , "fade_ratio" , 
		"fade ratio in percent when window unfocued [100]") ;
	kik_conf_add_opt( conf , 's' , "sb" , 1 , "use_scrollbar" , 
		"use scrollbar [true]") ;
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
#ifdef  USE_EXT_IMAGELIB
	kik_conf_add_opt( conf , '\0' , "iconpath" , 0 , "icon_path" ,
		"path to an imagefile to be use as an window icon") ;
#endif
#ifdef  USE_FRIBIDI
	kik_conf_add_opt( conf , '\0' , "bimode" , 0 , "bidi_mode" ,
		"bidi mode [normal]") ;
#endif
	kik_conf_add_opt( conf , '\0' , "im" , 0 , "input_method" ,
		"input method (xim/kbd/uim/iiimf/m17nlib/scim/none) [xim]") ;
	kik_conf_add_opt( conf , '\0' , "parent" , 0 , "parent_window" ,
		"parent window") ;
#ifdef  USE_WIN32API
	kik_conf_add_opt( conf , '\0' , "servlist" , 0 , "server_list" ,
		"list of servers to connect") ;
	kik_conf_add_opt( conf , '\0' , "serv" , 0 , "default_server" ,
		"connecting server by default") ;
#endif
	kik_conf_set_end_opt( conf , 'e' , NULL , "exec_cmd" , 
		"execute external command") ;

	return  1 ;
}

int
x_main_config_init(
	x_main_config_t *  main_config ,
	kik_conf_t *  conf ,
	int  argc ,
	char **  argv
	)
{
	char *  value ;
	char *  invalid_msg = "%s %s is not valid.\n" ;
	
	if( ( value = kik_conf_get_value( conf , "display")) == NULL)
	{
		value = "" ;
	}

	if( ( main_config->disp_name = strdup( value)) == NULL)
	{
		return  0 ;
	}
	
	if( ( value = kik_conf_get_value( conf , "fontsize")) == NULL)
	{
		main_config->font_size = 16 ;
	}
	else if( ! kik_str_to_uint( &main_config->font_size , value))
	{
		kik_msg_printf( invalid_msg , "font size" , value) ;

		/* default value is used. */
		main_config->font_size = 16 ;
	}

	if( main_config->font_size > x_get_max_font_size())
	{
		kik_msg_printf( "font size %d is too large. %d is used.\n" ,
			main_config->font_size , x_get_max_font_size()) ;
		
		main_config->font_size = x_get_max_font_size() ;
	}
	else if( main_config->font_size < x_get_min_font_size())
	{
		kik_msg_printf( "font size %d is too small. %d is used.\n" ,
			main_config->font_size , x_get_min_font_size()) ;
			
		main_config->font_size = x_get_min_font_size() ;
	}

	main_config->app_name = NULL ;

	if( ( value = kik_conf_get_value( conf , "app_name")))
	{
		main_config->app_name = strdup( value) ;
	}

	main_config->title = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "title")))
	{
		main_config->title = strdup( value) ;
	}

	main_config->icon_name = NULL ;

	if( ( value = kik_conf_get_value( conf , "icon_name")))
	{
		main_config->icon_name = strdup( value) ;
	}

	/* Not changeable. */
	main_config->conf_menu_path_1 = NULL ;

	if( ( value = kik_conf_get_value( conf , "conf_menu_path_1")))
	{
		main_config->conf_menu_path_1 = strdup( value) ;
	}

	/* Not changeable. */
	main_config->conf_menu_path_2 = NULL ;

	if( ( value = kik_conf_get_value( conf , "conf_menu_path_2")))
	{
		main_config->conf_menu_path_2 = strdup( value) ;
	}

	main_config->conf_menu_path_3 = NULL ;

	if( ( value = kik_conf_get_value( conf , "conf_menu_path_3")))
	{
		main_config->conf_menu_path_3 = strdup( value) ;
	}

	/* use default value */
	main_config->scrollbar_view_name = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "scrollbar_view_name")))
	{
		main_config->scrollbar_view_name = strdup( value) ;
	}

	main_config->use_char_combining = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "use_combining")))
	{
		if( strcmp( value , false) == 0)
		{
			main_config->use_char_combining = 0 ;
		}
	}

	main_config->use_dynamic_comb = 0 ;
	if( ( value = kik_conf_get_value( conf , "use_dynamic_comb")))
	{
		if( strcmp( value , true) == 0)
		{
			main_config->use_dynamic_comb = 1 ;
		}
	}

	main_config->logging_vt_seq = 0 ;
	if( ( value = kik_conf_get_value( conf , "logging_vt_seq")))
	{
		if( strcmp( value , true) == 0)
		{
			main_config->logging_vt_seq = 1 ;
		}
	}

	main_config->font_present = 0 ;

	if( ( value = kik_conf_get_value( conf , "use_variable_column_width")))
	{
		if( strcmp( value , true) == 0)
		{
			main_config->font_present |= FONT_VAR_WIDTH ;
		}
	}

	main_config->step_in_changing_font_size = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "step_in_changing_font_size")))
	{
		u_int  size ;
		
		if( kik_str_to_uint( &size , value))
		{
			main_config->step_in_changing_font_size = size ;
		}
		else
		{
			kik_msg_printf( invalid_msg , "step in changing font size" , value) ;
		}
	}

#ifdef  USE_TYPE_XCORE
	main_config->type_engine = TYPE_XCORE ;
#else
	main_config->type_engine = TYPE_XFT ;
#endif

	if( ( value = kik_conf_get_value( conf , "type_engine")))
	{
	#ifdef  USE_TYPE_XFT
		if( strcmp( value , "xft") == 0)
		{
			main_config->type_engine = TYPE_XFT ;
		}
		else
	#endif

	#ifdef  USE_TYPE_XCORE
		if( strcmp( value , "xcore") == 0)
		{
			main_config->type_engine = TYPE_XCORE ;
		}
		else
	#endif
		{
			kik_msg_printf( "%s is unsupported type engine.\n" , value) ;
		}
	}

#ifdef  USE_TYPE_XFT
	if( ( value = kik_conf_get_value( conf , "use_anti_alias")))
	{
		if( strcmp( value , true) == 0)
		{
			main_config->font_present |= FONT_AA ;
			/* forcibly use xft */
			main_config->type_engine = TYPE_XFT ;
		}
		else if( strcmp( value , false) == 0)
		{
			main_config->font_present |= FONT_NOAA ;
		}
	}
#endif

	main_config->vertical_mode = 0 ;
	
	if( ( value = kik_conf_get_value( conf , "vertical_mode")))
	{
		if( ( main_config->vertical_mode = ml_get_vertical_mode( value)))
		{
			/*
			 * vertical font is automatically used under vertical mode.
			 * similler processing is done in x_screen.c:change_vertical_mode.
			 */
			main_config->font_present |= FONT_VERTICAL ;
			main_config->font_present &= ~FONT_VAR_WIDTH ;
		}
	}

	main_config->fg_color = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "fg_color")))
	{
		main_config->fg_color = strdup( value) ;
	}

	main_config->bg_color = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "bg_color")))
	{
		main_config->bg_color = strdup( value) ;
	}

	main_config->cursor_fg_color = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "cursor_fg_color")))
	{
		main_config->cursor_fg_color = strdup( value) ;
	}

	main_config->cursor_bg_color = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "cursor_bg_color")))
	{
		main_config->cursor_bg_color = strdup( value) ;
	}
	
	main_config->sb_fg_color = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "sb_fg_color")))
	{
		main_config->sb_fg_color = strdup( value) ;
	}

	main_config->sb_bg_color = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "sb_bg_color")))
	{
		main_config->sb_bg_color = strdup( value) ;
	}
	
	if( ( value = kik_conf_get_value( conf , "termtype")))
	{
		main_config->term_type = strdup( value) ;
	}
	else
	{
		main_config->term_type = strdup( "xterm") ;
	}
	
	main_config->x = 0 ;
	main_config->y = 0 ;
	main_config->cols = 80 ;
	main_config->rows = 24 ;
	
	if( ( value = kik_conf_get_value( conf , "geometry")))
	{
	#ifdef  USE_WIN32GUI
		/*
		 * "+[x]+[y]" is igonred.
		 */
		char *  p ;
		u_int  cols ;
		u_int  rows ;

		main_config->geom_hint = 0 ;
		p = value ;

		if( ( p = strchr( value, '+')))
		{
			*p = '\0' ;
		}

		if( sscanf( value, "%ux%u", &cols, &rows) == 2)
		{
			main_config->cols = cols ;
			main_config->rows = rows ;
		}
	#else
		/*
		 * For each value not found, the argument is left unchanged.
		 * (see man XParseGeometry(3))
		 */
		main_config->geom_hint = XParseGeometry( value ,
						&main_config->x , &main_config->y ,
						&main_config->cols , &main_config->rows) ;

		if( main_config->cols == 0 || main_config->rows == 0)
		{
			kik_msg_printf( "geometry option %s is illegal.\n" , value) ;
			
			main_config->cols = 80 ;
			main_config->rows = 24 ;
		}
	#endif
	}
	else
	{
		main_config->geom_hint = 0 ;
	}

	main_config->screen_width_ratio = 100 ;
	
	if( ( value = kik_conf_get_value( conf , "screen_width_ratio")))
	{
		u_int  ratio ;

		if( kik_str_to_uint( &ratio , value) && ratio)
		{
			main_config->screen_width_ratio = ratio ;
		}
		else
		{
			kik_msg_printf( invalid_msg , "screen_width_ratio" , value) ;
		}
	}

	main_config->screen_height_ratio = 100 ;
	
	if( ( value = kik_conf_get_value( conf , "screen_height_ratio")))
	{
		u_int  ratio ;

		if( kik_str_to_uint( &ratio , value) && ratio)
		{
			main_config->screen_height_ratio = ratio ;
		}
		else
		{
			kik_msg_printf( invalid_msg , "screen_height_ratio" , value) ;
		}
	}
	
	main_config->use_multi_col_char = 1 ;

	if( ( value = kik_conf_get_value( conf , "use_multi_column_char")))
	{
		if( strcmp( value , false) == 0)
		{
			main_config->use_multi_col_char = 0 ;
		}
	}

	main_config->line_space = 0 ;

	if( ( value = kik_conf_get_value( conf , "line_space")))
	{
		u_int  size ;

		if( kik_str_to_uint( &size , value))
		{
			main_config->line_space = size ;
		}
		else
		{
			kik_msg_printf( invalid_msg , "line space" , value) ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "logsize")) == NULL)
	{
		main_config->num_of_log_lines = 128 ;
	}
	else if( ! kik_str_to_uint( &main_config->num_of_log_lines , value))
	{
		kik_msg_printf( invalid_msg , "log size" , value) ;

		/* default value is used. */
		main_config->num_of_log_lines = 128 ;
	}

	if( ( value = kik_conf_get_value( conf , "tabsize")) == NULL)
	{
		/* default value is used. */
		main_config->tab_size = 8 ;
	}
	else if( ! kik_str_to_uint( &main_config->tab_size , value))
	{
		kik_msg_printf( invalid_msg , "tab size" , value) ;

		/* default value is used. */
		main_config->tab_size = 8 ;
	}
	
	main_config->use_login_shell = 0 ;
	
	if( ( value = kik_conf_get_value( conf , "use_login_shell")))
	{
		if( strcmp( value , true) == 0)
		{
			main_config->use_login_shell = 1 ;
		}
	}

	main_config->big5_buggy = 0 ;

	if( ( value = kik_conf_get_value( conf , "big5_buggy")))
	{
		if( strcmp( value , true) == 0)
		{
			main_config->big5_buggy = 1 ;
		}
	}

	main_config->use_scrollbar = 1 ;

	if( ( value = kik_conf_get_value( conf , "use_scrollbar")))
	{
		if( strcmp( value , false) == 0)
		{
			main_config->use_scrollbar = 0 ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "scrollbar_mode")))
	{
		main_config->sb_mode = x_get_sb_mode( value) ;
	}
	else
	{
		main_config->sb_mode = SBM_LEFT ;
	}

	main_config->iso88591_font_for_usascii = 0 ;

	if( ( value = kik_conf_get_value( conf , "iso88591_font_for_usascii")))
	{
		if( strcmp( value , true) == 0)
		{
			main_config->iso88591_font_for_usascii = 1 ;
		}
	}

	main_config->unicode_font_policy = 0 ;

	if( ( value = kik_conf_get_value( conf , "not_use_unicode_font")))
	{
		if( strcmp( value , true) == 0)
		{
			main_config->unicode_font_policy = NOT_USE_UNICODE_FONT ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "only_use_unicode_font")))
	{
		if( strcmp( value , true) == 0)
		{
			if( main_config->unicode_font_policy == NOT_USE_UNICODE_FONT)
			{
				kik_msg_printf(
					"only_use_unicode_font and not_use_unicode_font options "
					"cannot be used at the same time.\n") ;

				/* default values are used */
				main_config->unicode_font_policy = 0 ;
			}
			else
			{
				main_config->unicode_font_policy = ONLY_USE_UNICODE_FONT ;
			}
		}
	}

	main_config->receive_string_via_ucs = 0 ;

	if( ( value = kik_conf_get_value( conf , "receive_string_via_ucs")))
	{
		if( strcmp( value , true) == 0)
		{
			main_config->receive_string_via_ucs = 1 ;
		}
	}
	
	/* default value is used */
	main_config->col_size_of_width_a = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "col_size_of_width_a")))
	{
		u_int  col_size_of_width_a ;
		
		if( kik_str_to_uint( &col_size_of_width_a , value))
		{
			main_config->col_size_of_width_a = col_size_of_width_a ;
		}
		else
		{
			kik_msg_printf( invalid_msg , "col size of width a" , value) ;
		}
	}

	main_config->pic_file_path = NULL ;

	if( ( value = kik_conf_get_value( conf , "wall_picture")))
	{
		if( *value != '\0')
		{
			main_config->pic_file_path = strdup( value) ;
		}
	}

	main_config->use_transbg = 0 ;

	if( ( value = kik_conf_get_value( conf , "use_transbg")))
	{
		if( strcmp( value , true) == 0)
		{
			main_config->use_transbg = 1 ;
		}
	}

	if( main_config->pic_file_path && main_config->use_transbg)
	{
		kik_msg_printf(
			"wall picture and transparent background cannot be used at the same time.\n") ;

		/* using wall picture */
		main_config->use_transbg = 0 ;
	}

	main_config->brightness = 100 ;

	if( ( value = kik_conf_get_value( conf , "brightness")))
	{
		u_int  brightness ;
		
		if( kik_str_to_uint( &brightness , value))
		{
			main_config->brightness = brightness ;
		}
		else
		{
			kik_msg_printf( invalid_msg , "shade ratio" , value) ;
		}
	}

	main_config->contrast = 100 ;

	if( ( value = kik_conf_get_value( conf , "contrast")))
	{
		u_int  contrast ;
		
		if( kik_str_to_uint( &contrast , value))
		{
			main_config->contrast = contrast ;
		}
		else
		{
			kik_msg_printf( invalid_msg , "contrast ratio" , value) ;
		}
	}
	
	main_config->gamma = 100 ;

	if( ( value = kik_conf_get_value( conf , "gamma")))
	{
		u_int  gamma ;
		
		if( kik_str_to_uint( &gamma , value))
		{
			main_config->gamma = gamma ;
		}
		else
		{
			kik_msg_printf( invalid_msg , "gamma ratio" , value) ;
		}
	}

	main_config->alpha = 255 ;

	if( ( value = kik_conf_get_value( conf , "alpha")))
	{
		u_int  alpha ;
		
		if( kik_str_to_uint( &alpha , value))
		{
			main_config->alpha = alpha ;
		}
		else
		{
			kik_msg_printf( invalid_msg , "alpha" , value) ;
		}
	}
	
	main_config->fade_ratio = 100 ;
	
	if( ( value = kik_conf_get_value( conf , "fade_ratio")))
	{
		u_int  fade_ratio ;
		
		if( kik_str_to_uint( &fade_ratio , value) && fade_ratio <= 100)
		{
			main_config->fade_ratio = fade_ratio ;
		}
		else
		{
			kik_msg_printf( invalid_msg , "fade ratio" , value) ;
		}
	}

	main_config->is_auto_encoding = 0 ;
	if( ( value = kik_conf_get_value( conf , "ENCODING")))
	{
		if( ( main_config->encoding = ml_get_char_encoding( value)) == ML_UNKNOWN_ENCODING)
		{
			kik_msg_printf(
				"%s encoding is not supported. Auto detected encoding is used.\n" ,
				value) ;
				
			main_config->encoding = ml_get_char_encoding( "auto") ;
			main_config->is_auto_encoding = 1 ;
		}
	}
	else
	{
		main_config->encoding = ml_get_char_encoding( "auto") ;
		main_config->is_auto_encoding = 1 ;
	}

	if( main_config->encoding == ML_UNKNOWN_ENCODING)
	{
		main_config->encoding = ML_ISO8859_1 ;
	}

	main_config->use_bidi = 1 ;

	if( ( value = kik_conf_get_value( conf , "use_bidi")))
	{
		if( strcmp( value , false) == 0)
		{
			main_config->use_bidi = 0 ;
		}
	}

	main_config->bidi_mode = BIDI_NORMAL_MODE ;
	
	if( ( value = kik_conf_get_value( conf , "bidi_mode")))
	{
		main_config->bidi_mode = ml_get_bidi_mode( value) ;
	}

	/* If value is "none" or not is also checked in x_screen.c */
	if( ( value = kik_conf_get_value( conf , "mod_meta_key")) &&
		strcmp( value , "none") != 0)
	{
		main_config->mod_meta_key = strdup( value) ;
	}
	else
	{
		main_config->mod_meta_key = NULL ;
	}
	
	if( ( value = kik_conf_get_value( conf , "mod_meta_mode")))
	{
		main_config->mod_meta_mode = x_get_mod_meta_mode( value) ;
	}
	else
	{
		main_config->mod_meta_mode = MOD_META_SET_MSB ;
	}

	if( ( value = kik_conf_get_value( conf , "bel_mode")))
	{
		main_config->bel_mode = x_get_bel_mode( value) ;
	}
	else
	{
		main_config->bel_mode = BEL_SOUND ;
	}

	main_config->use_vertical_cursor = 0 ;

	if( ( value = kik_conf_get_value( conf , "use_vertical_cursor")))
	{
		if( strcmp( value , true) == 0)
		{
			main_config->use_vertical_cursor = 1 ;
		}
	}
	
	main_config->iscii_lang_type = ISCIILANG_MALAYALAM ;

	if( ( value = kik_conf_get_value( conf , "iscii_lang")))
	{
		ml_iscii_lang_type_t  type ;
		
		if( ( type = ml_iscii_get_lang( value)) != ISCIILANG_UNKNOWN)
		{
			main_config->iscii_lang_type = type ;
		}
	}

	main_config->use_extended_scroll_shortcut = 0 ;
	
	if( ( value = kik_conf_get_value( conf , "use_extended_scroll_shortcut")))
	{
		if( strcmp( value , true) == 0)
		{
			main_config->use_extended_scroll_shortcut = 1 ;
		}
	}

	main_config->borderless = 0 ;
	
	if( ( value = kik_conf_get_value( conf , "borderless")))
	{
		if( strcmp( value , true) == 0)
		{
			main_config->borderless = 1 ;
		}
	}

	main_config->bs_mode = BSM_VOLATILE ;

	if( ( value = kik_conf_get_value( conf , "static_backscroll_mode")))
	{
		if( strcmp( value , true) == 0)
		{
			main_config->bs_mode = BSM_STATIC ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "icon_path")))
	{
		main_config->icon_path = strdup( value) ;
	}
	else
	{
		main_config->icon_path = NULL ;
	}

	if( ( value = kik_conf_get_value( conf , "input_method")))
	{
		main_config->input_method = strdup( value) ;
	}
	else
	{
		main_config->input_method = strdup( "xim") ;
	}

	if( ( value = kik_conf_get_value( conf , "init_str")))
	{
		if( ( main_config->init_str = malloc( strlen( value) + 1)))
		{
			char *  p1 ;
			char *  p2 ;

			p1 = value ;
			p2 = main_config->init_str ;

			while( *p1)
			{
				if( *p1 == '\\')
				{
					p1 ++ ;
					
					if( *p1 == '\0')
					{
						break ;
					}
					else if( *p1 == 'n')
					{
						*(p2 ++) = '\n' ;
					}
					else if( *p1 == 'r')
					{
						*(p2 ++) = '\r' ;
					}
					else if( *p1 == 't')
					{
						*(p2 ++) = '\t' ;
					}
					else if( *p1 == 'e')
					{
						*(p2 ++) = '\033' ;
					}
					else
					{
						*(p2 ++) = *p1 ;
					}
				}
				else
				{
					*(p2 ++) = *p1 ;
				}

				p1 ++ ;
			}

			*p2 = '\0' ;
		}
	}
	else
	{
		main_config->init_str = NULL ;
	}

	main_config->parent_window = 0 ;
	
	if( ( value = kik_conf_get_value( conf , "parent_window")))
	{
		u_int  parent_window ;
		
		if( kik_str_to_uint( &parent_window , value))
		{
			main_config->parent_window = parent_window ;
		}
		else
		{
			kik_msg_printf( invalid_msg , "parent window" , value) ;
		}
	}
	
#ifdef  USE_WIN32API
	main_config->server_list = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "server_list")))
	{
		if( ( main_config->server_list = malloc( sizeof( char*) *
						/* A,B => A B NULL */
						( kik_count_char_in_str( value , ',') + 2)) ) )
		{
			if( ( value = strdup( value)))
			{
				int  count ;

				count = 0 ;
				do
				{
					main_config->server_list[count++] =
								kik_str_sep( &value , ",") ;
				}
				while( value) ;

				main_config->server_list[count] = NULL ;
			}
			else
			{
				free( main_config->server_list) ;
				main_config->server_list = NULL ;
			}
		}
	}
	
	main_config->default_server = NULL ;
	
	if( ( value = kik_conf_get_value( conf , "default_server")))
	{
		if( ( main_config->default_server = strdup( value)))
		{
			x_main_config_add_to_server_list( main_config ,
				main_config->default_server) ;
		}
	}
#endif

	if( ( value = kik_conf_get_value( conf , "exec_cmd")) && strcmp( value , true) == 0)
	{
		if( ( main_config->cmd_argv = malloc( sizeof( char*) * (argc + 1))) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
		#endif
		
			main_config->cmd_path = NULL ;
			main_config->cmd_argv = NULL ;
		}
		else
		{
			/*
			 * !! Notice !!
			 * cmd_path and strings in cmd_argv vector should be allocated
			 * by the caller.
			 */
			 
			main_config->cmd_path = argv[0] ;
			
			memcpy( &main_config->cmd_argv[0] , argv , sizeof( char*) * argc) ;
			main_config->cmd_argv[argc] = NULL ;
		}
	}
	else
	{
		main_config->cmd_path = NULL ;
		main_config->cmd_argv = NULL ;
	}

	return  1 ;
}

int
x_main_config_final(
	x_main_config_t *  main_config
	)
{
	free( main_config->disp_name) ;
	free( main_config->app_name) ;
	free( main_config->title) ;
	free( main_config->icon_name) ;
	free( main_config->term_type) ;
	free( main_config->conf_menu_path_1) ;
	free( main_config->conf_menu_path_2) ;
	free( main_config->conf_menu_path_3) ;
	free( main_config->pic_file_path) ;
	free( main_config->scrollbar_view_name) ;
	free( main_config->fg_color) ;
	free( main_config->bg_color) ;
	free( main_config->cursor_fg_color) ;
	free( main_config->cursor_bg_color) ;
	free( main_config->sb_fg_color) ;
	free( main_config->sb_bg_color) ;
	free( main_config->mod_meta_key) ;
	free( main_config->icon_path) ;
	free( main_config->input_method) ;
	free( main_config->init_str) ;

#ifdef  USE_WIN32API
	if( main_config->server_list)
	{
		free( main_config->server_list[0]) ;
		free( main_config->server_list) ;
	}
	free( main_config->default_server) ;
#endif

	free( main_config->cmd_argv) ;

	return  1 ;
}

#ifdef  USE_WIN32API
int
x_main_config_add_to_server_list(
	x_main_config_t *  main_config ,
	char *  server
	)
{
	u_int  nlist ;
	int  found ;
	size_t  len ;
	size_t  add_len ;
	char *  p ;
	char **  pp ;

	found = 0 ;
	nlist = 0 ;
	len = 0 ;
	if( main_config->server_list)
	{
		while( main_config->server_list[nlist])
		{
			len += ( strlen( main_config->server_list[nlist]) + 1) ;
			
			if( strcmp( main_config->server_list[nlist++] , server) == 0)
			{
				return  1 ;
			}
		}
	}

	add_len = strlen( server) + 1 ;
	p = NULL ;
	if( main_config->server_list)
	{
		if( ( p = realloc( main_config->server_list[0] , len + add_len)))
		{
			memcpy( p + len , server , add_len) ; 
		}
	}
	else
	{
		p = strdup( server) ;
	}

	if( p && ( pp = realloc( main_config->server_list ,
				sizeof(char*) * (nlist + 2))))
	{
		int  count ;
		
		main_config->server_list = pp ;

		for( count = 0 ; count < nlist + 1 ; count++)
		{
			main_config->server_list[count] = p ;
			p += ( strlen( p) + 1) ;
		}
		main_config->server_list[count] = NULL ;

		return  1 ;
	}
	else
	{
		free( p) ;
		
		return  0 ;
	}
}
#endif
