/*
 *	$Id$
 */

#include  "x_main_config.h"

#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_mem.h>	/* malloc/realloc */
#include  <kiklib/kik_str.h>	/* kik_str_to_uint */
#include  <kiklib/kik_locale.h>	/* kik_get_lang */


/* --- global functions --- */

int
x_prepare_for_main_config(
	kik_conf_t *  conf
	)
{
	char *  rcpath ;

#ifdef  ENABLE_BACKWARD_COMPAT
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
#endif

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

#if ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
	kik_conf_add_opt( conf , '&' , "borderless" , 1 , "borderless" ,
		"override redirect [false]") ;
	kik_conf_add_opt( conf , '*' , "type" , 0 , "type_engine" ,
		"type engine "
	#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
		"[xcore]"
	#elif  ! defined(USE_TYPE_XFT) && defined(USE_TYPE_CAIRO)
		"[cairo]"
	#else
		"[xft]"
	#endif
		) ;
#endif	/* USE_WIN32GUI/USE_FRAMEBUFFER */

	kik_conf_add_opt( conf , '1' , "wscr" , 0 , "screen_width_ratio" ,
		"screen width in percent against font width [100]") ;
	kik_conf_add_opt( conf , '2' , "hscr" , 0 , "screen_height_ratio" ,
		"screen height in percent against font height [100]") ;
#ifndef  NO_IMAGE
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
		"bel (0x07) mode (none/sound/visual) [sound]") ;
	kik_conf_add_opt( conf , '8' , "88591" , 1 , "iso88591_font_for_usascii" ,
		"use ISO-8859-1 font for ASCII part of any encoding [false]") ;
	kik_conf_add_opt( conf , '9' , "crfg" , 0 , "cursor_fg_color" ,
		"cursor foreground color") ;
	kik_conf_add_opt( conf , '0' , "crbg" , 0 , "cursor_bg_color" ,
		"cursor background color") ;
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
	kik_conf_add_opt( conf , 'A' , "aa" , 1 , "use_anti_alias" , 
		"forcibly use anti alias font by using Xft or cairo") ;
#endif
	kik_conf_add_opt( conf , 'B' , "sbbg" , 0 , "sb_bg_color" , 
		"scrollbar background color") ;
#if  ! defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND)
	kik_conf_add_opt( conf , 'C' , "ind" , 1 , "use_ind" , 
		"use indic (ligature text) [false]") ;
#endif
#if  ! defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI)
	kik_conf_add_opt( conf , 'D' , "bi" , 1 , "use_bidi" ,
		"use bidi (bi-directional text) [true]") ;
#endif
	kik_conf_add_opt( conf , 'E' , "km" , 0 , "encoding" ,
		"character encoding (AUTO/ISO-8859-*/EUC-*/UTF-8/...) [AUTO]") ;
	kik_conf_add_opt( conf , 'F' , "sbfg" , 0 , "sb_fg_color" ,
		"scrollbar foreground color") ;
	kik_conf_add_opt( conf , 'G' , "vertical" , 0 , "vertical_mode" ,
		"vertical mode (none/cjk/mongol) [none]") ;
#ifndef  NO_IMAGE
	kik_conf_add_opt( conf , 'H' , "bright" , 0 , "brightness" ,
		"brightness of background image in percent [100]") ;
#endif
#if ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
	kik_conf_add_opt( conf , 'I' , "icon" , 0 , "icon_name" , 
		"icon name") ;
#endif
	kik_conf_add_opt( conf , 'J' , "dyncomb" , 1 , "use_dynamic_comb" ,
		"use dynamic combining [false]") ;
	kik_conf_add_opt( conf , 'K' , "metakey" , 0 , "mod_meta_key" ,
		"meta key [none]") ;
	kik_conf_add_opt( conf , 'L' , "ls" , 1 , "use_login_shell" , 
		"turn on login shell [false]") ;
	kik_conf_add_opt( conf , 'M' , "im" , 0 , "input_method" ,
		"input method (xim/kbd/uim/m17nlib/scim/ibus/none) [xim]") ;
	kik_conf_add_opt( conf , 'N' , "name" , 0 , "app_name" , 
		"application name") ;
	kik_conf_add_opt( conf , 'O' , "sbmod" , 0 , "scrollbar_mode" ,
		"scrollbar mode (none/left/right) [none]") ;
	kik_conf_add_opt( conf , 'P' , "clip" , 1 , "use_clipboard" ,
		"use CLIPBOARD (not only PRIMARY) selection [false]") ;
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
	kik_conf_add_opt( conf , 'W' , "sep" , 0 , "word_separators" ,
		"word-separating characters for double-click [,.:;/@]") ;
	kik_conf_add_opt( conf , 'X' , "alpha" , 0 , "alpha" ,
		"alpha blending for translucent [255]") ;
	kik_conf_add_opt( conf , 'Z' , "multicol" , 1 , "use_multi_column_char" ,
		"fullwidth character occupies two logical columns [true]") ;
	kik_conf_add_opt( conf , 'a' , "ac" , 0 , "col_size_of_width_a" ,
		"columns for Unicode \"EastAsianAmbiguous\" character [1]") ;
	kik_conf_add_opt( conf , 'b' , "bg" , 0 , "bg_color" , 
		"background color") ;
#if ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
	kik_conf_add_opt( conf , 'd' , "display" , 0 , "display" , 
		"X server to connect") ;
#endif
	kik_conf_add_opt( conf , 'f' , "fg" , 0 , "fg_color" , 
		"foreground color") ;
	kik_conf_add_opt( conf , 'g' , "geometry" , 0 , "geometry" , 
		"size (in characters) and position [80x24]") ;
	kik_conf_add_opt( conf , 'k' , "meta" , 0 , "mod_meta_mode" , 
		"mode in pressing meta key (none/esc/8bit) [8bit]") ;
	kik_conf_add_opt( conf , 'l' , "sl" , 0 , "logsize" , 
		"number of backlog (scrolled lines to save) [128]") ;
	kik_conf_add_opt( conf , 'm' , "comb" , 1 , "use_combining" , 
		"use combining characters [true]") ;
	kik_conf_add_opt( conf , 'n' , "noucsfont" , 1 , "not_use_unicode_font" ,
		"use non-Unicode fonts even in UTF-8 mode [false]") ;
	kik_conf_add_opt( conf , 'o' , "lsp" , 0 , "line_space" ,
		"extra space between lines in pixels [0]") ;
#ifndef  NO_IMAGE
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
	kik_conf_add_opt( conf , '\0' , "bdfont" , 1 , "use_bold_font" ,
		"use bold fonts [true]") ;
	kik_conf_add_opt( conf , '\0' , "iconpath" , 0 , "icon_path" ,
		"path to an imagefile to be use as an window icon") ;
#if  ! defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI)
	kik_conf_add_opt( conf , '\0' , "bimode" , 0 , "bidi_mode" ,
		"bidi mode [normal]") ;
#endif
	kik_conf_add_opt( conf , '\0' , "parent" , 0 , "parent_window" ,
		"parent window") ;
	kik_conf_add_opt( conf , '\0' , "bd" , 0 , "bd_color" ,
		"Color to use to display bold characters (equivalent to colorBD)") ;
	kik_conf_add_opt( conf , '\0' , "ul" , 0 , "ul_color" ,
		"Color to use to display underlined characters (equivalent to colorUL)") ;
	kik_conf_add_opt( conf , '\0' , "noul" , 1 , "hide_underline" ,
		"Don't draw underline [false]") ;
#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
	kik_conf_add_opt( conf , '\0' , "servlist" , 0 , "server_list" ,
		"list of servers to connect") ;
	kik_conf_add_opt( conf , '\0' , "serv" , 0 , "default_server" ,
		"connecting server by default") ;
#endif
#ifdef  USE_LIBSSH2
	kik_conf_add_opt( conf , '\0' , "dialog" , 1 , "always_show_dialog" ,
		"always show dialog to input server address, password and so on [false]") ;
	kik_conf_add_opt( conf , '\0' , "pubkey" , 0 , "ssh_public_key" ,
		"ssh public key file "
	#ifdef  USE_WIN32API
		"[%HOMEPATH%\\mlterm\\id_rsa.pub]"
	#else
		"[~/.ssh/id_rsa.pub]"
	#endif
		) ;
	kik_conf_add_opt( conf , '\0' , "privkey" , 0 , "ssh_private_key" ,
		"ssh private key file "
	#ifdef  USE_WIN32API
		"[%HOMEPATH%\\mlterm\\id_rsa]"
	#else
		"[~/.ssh/id_rsa]"
	#endif
		) ;
	kik_conf_add_opt( conf , '\0' , "ciphlist" , 0 , "cipher_list" ,
		"preferred cipher list") ;
	kik_conf_add_opt( conf , '\0' , "x11" , 1 , "ssh_x11_forwarding" ,
		"allow x11 forwarding [false]") ;
	kik_conf_add_opt( conf , '\0' , "scp" , 1 , "allow_scp" , "allow scp [false]") ;
#endif
	kik_conf_add_opt( conf , '\0' , "csp" , 0 , "letter_space" ,
		"extra space between letters in pixels [0]") ;
	kik_conf_add_opt( conf , '\0' , "ucsprop" , 1 , "use_unicode_property" ,
		"use unicode property for characters [false]") ;
	kik_conf_add_opt( conf , '\0' , "osc52" , 1 , "allow_osc52" ,
		"allow access to clipboard by OSC 52 sequence [false]") ;
	kik_conf_add_opt( conf , '\0' , "blink" , 1 , "blink_cursor" ,
		"blink cursor [false]") ;
	kik_conf_add_opt( conf , '\0' , "border" , 0 , "inner_border" ,
		"inner border [2]") ;
	kik_conf_add_opt( conf , '\0' , "restart" , 1 , "auto_restart" ,
		"restart mlterm automatically if an error like segv happens. [true]") ;
	kik_conf_add_opt( conf , '\0' , "logmsg" , 1 , "logging_msg" ,
		"output messages to ~/.mlterm/msg.log [true]") ;
	kik_conf_add_opt( conf , '\0' , "loecho" , 1 , "use_local_echo" ,
		"use local echo [false]") ;
	kik_conf_add_opt( conf , '\0' , "altbuf" , 1 , "use_alt_buffer" ,
		"use alternative buffer. [true]") ;
	kik_conf_add_opt( conf , '\0' , "colors" , 1 , "use_ansi_colors" ,
		"recognize ANSI color change escape sequences. [true]") ;
	kik_conf_add_opt( conf , '\0' , "exitbs" , 1 , "exit_backscroll_by_pty" ,
		"exit backscroll mode on receiving data from pty. [false]") ;
	kik_conf_add_opt( conf , '\0' , "shortcut" , 1 , "allow_change_shortcut" ,
		"allow dynamic change of shortcut keys. [false]") ;
	kik_conf_add_opt( conf , '\0' , "boxdraw" , 0 , "box_drawing_font" ,
		"force unicode or decsp font for box-drawing characters. [noconv]") ;
	kik_conf_add_opt( conf , '\0' , "urgent" , 1 , "use_urgent_bell" ,
		"draw the user's attention when making a bell sound. [false]") ;
	kik_conf_add_opt( conf , '\0' , "locale" , 0 , "locale" , "set locale.") ;
	kik_conf_add_opt( conf , '\0' , "ucsnoconv" , 0 , "unicode_noconv_areas" ,
		"use unicode fonts partially regardless of -n option.") ;
	kik_conf_add_opt( conf , '\0' , "ade" , 0 , "auto_detect_encodings" ,
		"encodings detected automatically.") ;
	kik_conf_add_opt( conf , '\0' , "auto" , 1 , "use_auto_detect" ,
		"detect character encoding automatically.") ;
#ifdef  USE_GRF
	kik_conf_add_opt( conf , '\0' , "multivram" , 1 , "separate_wall_picture" ,
		"draw wall picture on another vram. (available on 4bpp) [false]") ;
#endif
#ifdef  USE_IM_CURSOR_COLOR
	kik_conf_add_opt( conf , '\0' , "imcolor" , 0 , "im_cursor_color" ,
		"cursor color when input method is activated. [false]") ;
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

	memset( main_config , 0 , sizeof(x_main_config_t)) ;

	if( ( value = kik_conf_get_value( conf , "locale")))
	{
		kik_locale_init( value) ;
	}

#if ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
	if( ( value = kik_conf_get_value( conf , "display")) == NULL)
#endif
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

	if( ( value = kik_conf_get_value( conf , "app_name")))
	{
		main_config->app_name = strdup( value) ;
	}
	
	if( ( value = kik_conf_get_value( conf , "title")))
	{
		main_config->title = strdup( value) ;
	}

#if ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
	if( ( value = kik_conf_get_value( conf , "icon_name")))
	{
		main_config->icon_name = strdup( value) ;
	}
#endif

	/* BACKWARD COMPAT (3.1.7 or before) */
#if  1
	if( ( value = kik_conf_get_value( conf , "conf_menu_path_1")))
	{
		main_config->shortcut_strs[0] = malloc( 5 + strlen(value) + 2 + 1) ;
		sprintf( main_config->shortcut_strs[0] , "\"menu:%s\"" , value) ;
	}

	if( ( value = kik_conf_get_value( conf , "conf_menu_path_2")))
	{
		main_config->shortcut_strs[1] = malloc( 5 + strlen(value) + 2 + 1) ;
		sprintf( main_config->shortcut_strs[1] , "\"menu:%s\"" , value) ;
	}

	if( ( value = kik_conf_get_value( conf , "conf_menu_path_3")))
	{
		main_config->shortcut_strs[2] = malloc( 5 + strlen(value) + 2 + 1) ;
		sprintf( main_config->shortcut_strs[2] , "\"menu:%s\"" , value) ;
	}

	if( ( value = kik_conf_get_value( conf , "button3_behavior")) &&
	    /* menu1,menu2,menu3,xterm values are ignored. */
	    strncmp( value , "menu" , 4) != 0)
	{
		main_config->shortcut_strs[3] = malloc( 7 + strlen(value) + 2 + 1) ;
		/* XXX "abc" should be "exesel:\"abc\"" but it's not supported. */
		sprintf( main_config->shortcut_strs[3] , "\"exesel:%s\"" , value) ;
	}
#endif
	
	if( ( value = kik_conf_get_value( conf , "scrollbar_view_name")))
	{
		main_config->scrollbar_view_name = strdup( value) ;
	}

	main_config->use_char_combining = 1 ;
	
	if( ( value = kik_conf_get_value( conf , "use_combining")))
	{
		if( strcmp( value , "false") == 0)
		{
			main_config->use_char_combining = 0 ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "use_dynamic_comb")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->use_dynamic_comb = 1 ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "logging_vt_seq")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->logging_vt_seq = 1 ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "logging_msg")) &&
	    strcmp( value , "false") == 0)
	{
		kik_set_msg_log_file_name( NULL) ;
	}
	else
	{
		kik_set_msg_log_file_name( "mlterm/msg.log") ;
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

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
	main_config->type_engine = TYPE_XCORE ;
#elif   defined(USE_TYPE_XFT)
	main_config->type_engine = TYPE_XFT ;
#else
	main_config->type_engine = TYPE_CAIRO ;
#endif

	if( ( value = kik_conf_get_value( conf , "type_engine")))
	{
		main_config->type_engine = x_get_type_engine_by_name( value) ;
	}

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
	if( ( value = kik_conf_get_value( conf , "use_anti_alias")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->font_present |= FONT_AA ;
			if( main_config->type_engine == TYPE_XCORE)
			{
				/* forcibly use xft or cairo */
			#if  ! defined(USE_TYPE_XFT) && defined(USE_TYPE_CAIRO)
				main_config->type_engine = TYPE_CAIRO ;
			#else
				main_config->type_engine = TYPE_XFT ;
			#endif
			}
		}
		else if( strcmp( value , "false") == 0)
		{
			main_config->font_present |= FONT_NOAA ;
		}
	}
#endif

	if( ( value = kik_conf_get_value( conf , "use_variable_column_width")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->font_present |= FONT_VAR_WIDTH ;
		}
	}
	
	if( ( value = kik_conf_get_value( conf , "vertical_mode")))
	{
		if( ( main_config->vertical_mode = ml_get_vertical_mode( value)))
		{
			main_config->font_present |= FONT_VERTICAL ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "fg_color")))
	{
		main_config->fg_color = strdup( value) ;
	}
	
	if( ( value = kik_conf_get_value( conf , "bg_color")))
	{
		main_config->bg_color = strdup( value) ;
	}
	
	if( ( value = kik_conf_get_value( conf , "cursor_fg_color")))
	{
		main_config->cursor_fg_color = strdup( value) ;
	}
	
	if( ( value = kik_conf_get_value( conf , "cursor_bg_color")))
	{
		main_config->cursor_bg_color = strdup( value) ;
	}

	if( ( value = kik_conf_get_value( conf , "bd_color")))
	{
		main_config->bd_color = strdup( value) ;
	}

	if( ( value = kik_conf_get_value( conf , "ul_color")))
	{
		main_config->ul_color = strdup( value) ;
	}
	
	if( ( value = kik_conf_get_value( conf , "sb_fg_color")))
	{
		main_config->sb_fg_color = strdup( value) ;
	}
	
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

#ifdef  USE_FRAMEBUFFER
	/*
	 * The pty is always resized to fit the display size.
	 * Don't use 80x24 for the default value because the screen is not drawn
	 * correctly on startup if the display size / the us-ascii font size is
	 * 80x24 by chance.
	 */
	main_config->cols = 1 ;
	main_config->rows = 1 ;
#else
	main_config->cols = 80 ;
	main_config->rows = 24 ;

	if( ( value = kik_conf_get_value( conf , "geometry")))
	{
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
	}
#endif

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
		if( strcmp( value , "false") == 0)
		{
			main_config->use_multi_col_char = 0 ;
		}
	}

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
	
	if( ( value = kik_conf_get_value( conf , "letter_space")))
	{
		u_int  size ;
		
		if( kik_str_to_uint( &size , value))
		{
			main_config->letter_space = size ;
		}
		else
		{
			kik_msg_printf( invalid_msg , "letter space" , value) ;
		}
	}

	main_config->num_of_log_lines = 128 ;

	if( ( value = kik_conf_get_value( conf , "logsize")))
	{
		u_int  size ;

		if( kik_str_to_uint( &size , value))
		{
			main_config->num_of_log_lines = size ;
		}
		else
		{
			kik_msg_printf( invalid_msg , "log size" , value) ;
		}

	}

	main_config->tab_size = 8 ;

	if( ( value = kik_conf_get_value( conf , "tabsize")))
	{
		u_int  size ;

		if( kik_str_to_uint( &size , value))
		{
			main_config->tab_size = size ;
		}
		else
		{
			kik_msg_printf( invalid_msg , "tab size" , value) ;
		}
	}
	
	main_config->use_login_shell = 0 ;
	
	if( ( value = kik_conf_get_value( conf , "use_login_shell")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->use_login_shell = 1 ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "big5_buggy")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->big5_buggy = 1 ;
		}
	}

	main_config->use_scrollbar = 1 ;

	if( ( value = kik_conf_get_value( conf , "use_scrollbar")))
	{
		if( strcmp( value , "false") == 0)
		{
			main_config->use_scrollbar = 0 ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "scrollbar_mode")))
	{
		main_config->sb_mode = x_get_sb_mode_by_name( value) ;
	}
	else
	{
		main_config->sb_mode = SBM_LEFT ;
	}

	if( ( value = kik_conf_get_value( conf , "iso88591_font_for_usascii")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->iso88591_font_for_usascii = 1 ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "not_use_unicode_font")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->unicode_policy = NOT_USE_UNICODE_FONT ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "unicode_noconv_areas")))
	{
		ml_set_unicode_noconv_areas( value) ;
	}

	if( ( value = kik_conf_get_value( conf , "only_use_unicode_font")))
	{
		if( strcmp( value , "true") == 0)
		{
			if( main_config->unicode_policy == NOT_USE_UNICODE_FONT)
			{
				kik_msg_printf(
					"only_use_unicode_font and not_use_unicode_font options "
					"cannot be used at the same time.\n") ;

				/* default values are used */
				main_config->unicode_policy = 0 ;
			}
			else
			{
				main_config->unicode_policy = ONLY_USE_UNICODE_FONT ;
			}
		}
	}

	if( ( value = kik_conf_get_value( conf , "use_unicode_property")))
	{
		if( strcmp( value , "true") == 0)
		{
			if( main_config->unicode_policy == ONLY_USE_UNICODE_FONT)
			{
				kik_msg_printf(
					"only_use_unicode_font and use_unicode_property "
					"cannot be used at the same time.\n") ;
			}
			else
			{
				main_config->unicode_policy |= USE_UNICODE_PROPERTY ;
			}
		}
	}

	if( ( value = kik_conf_get_value( conf , "box_drawing_font")))
	{
		if( strcmp( value , "decsp") == 0)
		{
			main_config->unicode_policy |= NOT_USE_UNICODE_BOXDRAW_FONT ;
		}
		else if( strcmp( value , "unicode") == 0)
		{
			main_config->unicode_policy |= ONLY_USE_UNICODE_BOXDRAW_FONT ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "receive_string_via_ucs")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->receive_string_via_ucs = 1 ;
		}
	}

	/* "cn" and "ko" ? */
	if( strcmp( kik_get_lang() , "ja") == 0)
	{
		main_config->col_size_of_width_a = 2 ;
	}
	else
	{
		main_config->col_size_of_width_a = 1 ;
	}
	
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

	if( ( value = kik_conf_get_value( conf , "wall_picture")))
	{
		if( *value != '\0')
		{
			main_config->pic_file_path = strdup( value) ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "use_transbg")))
	{
		if( strcmp( value , "true") == 0)
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

	if( ( value = kik_conf_get_value( conf , "encoding")))
	{
		while( ( main_config->encoding = ml_get_char_encoding( value))
			== ML_UNKNOWN_ENCODING)
		{
			kik_msg_printf(
				"%s encoding is not supported. Auto detected encoding is used.\n" ,
				value) ;

			value = "auto" ;
		}

		if( strcmp( value , "auto") == 0)
		{
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

#if  ! defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI)
	main_config->use_bidi = 1 ;

	if( ( value = kik_conf_get_value( conf , "use_bidi")))
	{
		if( strcmp( value , "false") == 0)
		{
			main_config->use_bidi = 0 ;
		}
	}
#endif

	main_config->bidi_mode = BIDI_NORMAL_MODE ;
	
#if  ! defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI)
	if( ( value = kik_conf_get_value( conf , "bidi_mode")))
	{
		main_config->bidi_mode = ml_get_bidi_mode( value) ;
	}
#endif

#if  ! defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND)
	if( ( value = kik_conf_get_value( conf , "use_ind")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->use_ind = 1 ;
		}
	}
#endif

	/* If value is "none" or not is also checked in x_screen.c */
	if( ( value = kik_conf_get_value( conf , "mod_meta_key")) &&
		strcmp( value , "none") != 0)
	{
		main_config->mod_meta_key = strdup( value) ;
	}
	
	if( ( value = kik_conf_get_value( conf , "mod_meta_mode")))
	{
		main_config->mod_meta_mode = x_get_mod_meta_mode_by_name( value) ;
	}
	else
	{
		main_config->mod_meta_mode = MOD_META_SET_MSB ;
	}

	if( ( value = kik_conf_get_value( conf , "bel_mode")))
	{
		main_config->bel_mode = x_get_bel_mode_by_name( value) ;
	}
	else
	{
		main_config->bel_mode = BEL_SOUND ;
	}

	if( ( value = kik_conf_get_value( conf , "use_urgent_bell")))
	{
		if( strcmp( value , "true") == 0)
		{
			x_set_use_urgent_bell( 1) ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "use_vertical_cursor")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->use_vertical_cursor = 1 ;
		}
	}
	
	if( ( value = kik_conf_get_value( conf , "use_extended_scroll_shortcut")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->use_extended_scroll_shortcut = 1 ;
		}
	}
	
	if( ( value = kik_conf_get_value( conf , "borderless")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->borderless = 1 ;
		}
	}

	main_config->bs_mode = BSM_DEFAULT ;

	if( ( value = kik_conf_get_value( conf , "static_backscroll_mode")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->bs_mode = BSM_STATIC ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "exit_backscroll_by_pty")))
	{
		x_exit_backscroll_by_pty( 1) ;
	}

	if( ( value = kik_conf_get_value( conf , "allow_change_shortcut")))
	{
		x_allow_change_shortcut( 1) ;
	}

	if( ( value = kik_conf_get_value( conf , "icon_path")))
	{
		main_config->icon_path = strdup( value) ;
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
	
#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
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

	if( ( value = kik_conf_get_value( conf , "default_server")))
	{
		if( *value && ( main_config->default_server = strdup( value)))
		{
		#ifdef  USE_WIN32API
			x_main_config_add_to_server_list( main_config ,
				main_config->default_server) ;
		#endif
		}
	}

	if( ( value = kik_conf_get_value( conf , "always_show_dialog")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->show_dialog = 1 ;
		}
	}
#endif

#ifdef  USE_LIBSSH2
	if( ( value = kik_conf_get_value( conf , "ssh_public_key")))
	{
		main_config->public_key = strdup( value) ;
	}

	if( ( value = kik_conf_get_value( conf , "ssh_private_key")))
	{
		main_config->private_key = strdup( value) ;
	}

	if( ( value = kik_conf_get_value( conf , "cipher_list")))
	{
		ml_pty_ssh_set_cipher_list( strdup( value)) ;
	}

	if( ( value = kik_conf_get_value( conf , "ssh_x11_forwarding")))
	{
		main_config->use_x11_forwarding = (strcmp( value , "true") == 0) ;
	}

	if( ( value = kik_conf_get_value( conf , "allow_scp")))
	{
		if( strcmp( value , "true") == 0)
		{
			ml_set_use_scp_full( 1) ;
		}
	}
#endif

	if( ( value = kik_conf_get_value( conf , "allow_osc52")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->allow_osc52 = 1 ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "blink_cursor")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->blink_cursor = 1 ;
		}
	}

	main_config->margin = 2 ;

	if( ( value = kik_conf_get_value( conf , "inner_border")))
	{
		u_int  margin ;

		/* 640x480 => (640-224*2)x(480-224*2) => 192x32 on framebuffer. */
		if( kik_str_to_uint( &margin , value) && margin <= 224)
		{
			main_config->margin = margin ;
		}
	}

	main_config->use_bold_font = 1 ;

	if( ( value = kik_conf_get_value( conf , "use_bold_font")))
	{
		if( strcmp( value , "false") == 0)
		{
			main_config->use_bold_font = 0 ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "hide_underline")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->hide_underline = 1 ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "word_separators")))
	{
		ml_set_word_separators( value) ;
	}

	if( ( value = kik_conf_get_value( conf , "use_clipboard")))
	{
		if( strcmp( value , "true") == 0)
		{
			x_set_use_clipboard_selection( 1) ;
		}
	}

	if( ! ( value = kik_conf_get_value( conf , "auto_restart")) ||
	    strcmp( value , "false") != 0)
	{
		ml_set_auto_restart_cmd( kik_get_prog_path()) ;
	}

	if( ( value = kik_conf_get_value( conf , "use_local_echo")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->use_local_echo = 1 ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "click_interval")))
	{
		int  interval ;

		if( kik_str_to_int( &interval , value))
		{
			x_set_click_interval( interval) ;
		}
		else
		{
			kik_msg_printf( invalid_msg , "click_interval" , value) ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "use_alt_buffer")))
	{
		if( strcmp( value , "false") == 0)
		{
			ml_set_use_alt_buffer( 0) ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "use_ansi_colors")))
	{
		if( strcmp( value , "false") == 0)
		{
			ml_set_use_ansi_colors( 0) ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "auto_detect_encodings")))
	{
		ml_set_auto_detect_encodings( value) ;
	}

	if( ( value = kik_conf_get_value( conf , "use_auto_detect")))
	{
		if( strcmp( value , "true") == 0)
		{
			main_config->use_auto_detect = 1 ;
		}
	}

#ifdef  USE_GRF
	if( ( value = kik_conf_get_value( conf , "separate_wall_picture")))
	{
		if( strcmp( value , "true") == 0)
		{
			extern int  separate_wall_picture ;

			separate_wall_picture = 1 ;
		}
	}
#endif

#ifdef  USE_IM_CURSOR_COLOR
	if( ( value = kik_conf_get_value( conf , "im_cursor_color")))
	{
		if( *value)
		{
			x_set_im_cursor_color( value) ;
		}
	}
#endif

	if( ( value = kik_conf_get_value( conf , "exec_cmd")) && strcmp( value , "true") == 0)
	{
		if( ( main_config->cmd_argv = malloc( sizeof( char*) * (argc + 1))))
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
	free( main_config->shortcut_strs[0]) ;
	free( main_config->shortcut_strs[1]) ;
	free( main_config->shortcut_strs[2]) ;
	free( main_config->shortcut_strs[3]) ;
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

#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
	if( main_config->server_list)
	{
		free( main_config->server_list[0]) ;
		free( main_config->server_list) ;
	}

	free( main_config->default_server) ;
#endif

#ifdef  USE_LIBSSH2
	free( main_config->public_key) ;
	free( main_config->private_key) ;
#endif

	free( main_config->cmd_argv) ;

	return  1 ;
}

#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
int
x_main_config_add_to_server_list(
	x_main_config_t *  main_config ,
	char *  server
	)
{
	u_int  nlist ;
	size_t  len ;
	size_t  add_len ;
	char *  p ;
	char **  pp ;

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
		u_int  count ;
		
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
