/*
 *	$Id$
 */

#include  "ml_config_menu.h"

#include  <stdio.h>		/* fprintf */
#include  <unistd.h>		/* pipe/close */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_str.h>	/* kik_str_sep/kik_str_to_int/strdup */
#include  <kiklib/kik_mem.h>	/* alloca/malloc */

#include  "ml_sig_child.h"


#ifndef  LIBEXECDIR
#define  MLCONFIG_PATH  "/usr/local/libexec/mlconfig"
#else
#define  MLCONFIG_PATH  LIBEXECDIR "/mlconfig"
#endif


/* --- static functions --- */

static void
sig_child(
	void *  self ,
	pid_t  pid
	)
{
	ml_config_menu_t *  config_menu ;
	FILE *  fp ;
	kik_file_t *  kin ;
	char *  p ;
	char *  input_line ;
	size_t  len ;
	char *  command ;

	config_menu = self ;

	if( ! config_menu->session || config_menu->session->pid != pid)
	{
		return ;
	}

	if( ( fp = fdopen( config_menu->session->fd , "r")) == NULL)
	{
		goto  end ;
	}

	if( ( kin = kik_file_new( fp)) == NULL)
	{
		fclose( fp) ;
	
		goto  end ;
	}
	
	if( ( p = kik_file_get_line( kin , &len)) == NULL || len == 0)
	{
		kik_file_delete( kin) ;
		fclose( fp) ;
			
		goto  end ;
	}

	if( ( input_line = alloca( len)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif
	
		kik_file_delete( kin) ;
		fclose( fp) ;
		
		goto  end ;
	}
	
	strncpy( input_line , p , len - 1) ;
	input_line[len - 1] = '\0' ;

	kik_file_delete( kin) ;
	fclose( fp) ;

	if( ( command = kik_str_sep( &input_line , ":")) == NULL)
	{
		goto  end ;
	}

	if( strcmp( command , "CONFIG") == 0)
	{
		/*
		 * CONFIG:[encoding] [iscii lang] [fg color] [bg color] [sb fg color] [sb bg color] \
		 * [tabsize] [logsize] [fontsize] [line space] [screen width ratio] [screen height ratio] \
		 * [mod meta mode] [bel mode] [vertical mode] [sb mode] [char combining] [dynamic comb] \
		 * [copy paste via ucs] [is transparent] [shade ratio] [fade ratio] [font present] \
		 * [use multi col char] [use bidi] [sb view name] [xim] [locale] \
		 * [wall pic][LF]
		 */
		 
		int  encoding ;
		int  iscii_lang ;
		char *  fg_color ;
		char *  bg_color ;
		char *  sb_fg_color ;
		char *  sb_bg_color ;
		u_int  tabsize ;
		u_int  logsize ;
		u_int  fontsize ;
		u_int  line_space ;
		u_int  screen_width_ratio ;
		u_int  screen_height_ratio ;
		int  mod_meta_mode ;
		int  bel_mode ;
		int  vertical_mode ;
		int  sb_mode ;
		int  use_char_combining ;
		int  use_dynamic_comb ;
		int  copy_paste_via_ucs ;
		int  is_transparent ;
		u_int  brightness ;
		u_int  fade_ratio ;
		int  font_present ;
		int  use_multi_col_char ;
		int  use_bidi ;
		char *  sb_view_name ;
		char *  xim ;
		char *  locale ;
		char *  wall_pic ;

		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( (int*)&encoding , p))
		{
			goto  end ;
		}

		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( (int*)&iscii_lang , p))
		{
			goto  end ;
		}
		
		if( ( fg_color = kik_str_sep( &input_line , " ")) == NULL)
		{
			goto  end ;
		}
		
		if( ( bg_color = kik_str_sep( &input_line , " ")) == NULL)
		{
			goto  end ;
		}
		
		if( ( sb_fg_color = kik_str_sep( &input_line , " ")) == NULL)
		{
			goto  end ;
		}
		
		if( ( sb_bg_color = kik_str_sep( &input_line , " ")) == NULL)
		{
			goto  end ;
		}

		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_uint( &tabsize , p))
		{
			goto  end ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_uint( &logsize , p))
		{
			goto  end ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_uint( &fontsize , p))
		{
			goto  end ;
		}

		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_uint( &line_space , p))
		{
			goto  end ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_uint( &screen_width_ratio , p))
		{
			goto  end ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_uint( &screen_height_ratio , p))
		{
			goto  end ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &mod_meta_mode , p))
		{
			goto  end ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &bel_mode , p))
		{
			goto  end ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &vertical_mode , p))
		{
			goto  end ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &sb_mode , p))
		{
			goto  end ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &use_char_combining , p))
		{
			goto  end ;
		}

		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &use_dynamic_comb , p))
		{
			goto  end ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &copy_paste_via_ucs , p))
		{
			goto  end ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &is_transparent , p))
		{
			goto  end ;
		}

		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_uint( &brightness , p))
		{
			goto  end ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_uint( &fade_ratio , p))
		{
			goto  end ;
		}

		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &font_present , p))
		{
			goto  end ;
		}

		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &use_multi_col_char , p))
		{
			goto  end ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &use_bidi , p))
		{
			goto  end ;
		}

		if( ( sb_view_name = kik_str_sep( &input_line , " ")) == NULL)
		{
			goto  end ;
		}
		
		if( ( xim = kik_str_sep( &input_line , " ")) == NULL)
		{
			goto  end ;
		}
		
		if( ( locale = kik_str_sep( &input_line , " ")) == NULL)
		{
			goto  end ;
		}
		
		if( ( wall_pic = input_line) == NULL)
		{
			goto  end ;
		}

		if( encoding != config_menu->session->encoding)
		{
			if( config_menu->config_menu_listener->change_char_encoding)
			{
				(*config_menu->config_menu_listener->change_char_encoding)(
					config_menu->config_menu_listener->self , encoding) ;
			}
		}

		if( encoding != config_menu->session->iscii_lang)
		{
			if( config_menu->config_menu_listener->change_iscii_lang)
			{
				(*config_menu->config_menu_listener->change_iscii_lang)(
					config_menu->config_menu_listener->self , iscii_lang) ;
			}
		}

		if( strcmp( fg_color , config_menu->session->fg_color) != 0)
		{
			if( config_menu->config_menu_listener->change_fg_color)
			{
				(*config_menu->config_menu_listener->change_fg_color)(
					config_menu->config_menu_listener->self , fg_color) ;
			}
		}

		if( strcmp( bg_color , config_menu->session->bg_color) != 0)
		{
			if( config_menu->config_menu_listener->change_bg_color)
			{
				(*config_menu->config_menu_listener->change_bg_color)(
					config_menu->config_menu_listener->self , bg_color) ;
			}
		}
		
		if( strcmp( sb_fg_color , config_menu->session->sb_fg_color) != 0)
		{
			if( config_menu->config_menu_listener->change_sb_fg_color)
			{
				(*config_menu->config_menu_listener->change_sb_fg_color)(
					config_menu->config_menu_listener->self , sb_fg_color) ;
			}
		}

		if( strcmp( sb_bg_color , config_menu->session->sb_bg_color) != 0)
		{
			if( config_menu->config_menu_listener->change_sb_bg_color)
			{
				(*config_menu->config_menu_listener->change_sb_bg_color)(
					config_menu->config_menu_listener->self , sb_bg_color) ;
			}
		}
		
		if( tabsize != config_menu->session->tabsize)
		{
			if( config_menu->config_menu_listener->change_tab_size)
			{
				(*config_menu->config_menu_listener->change_tab_size)(
					config_menu->config_menu_listener->self , tabsize) ;
			}
		}

		if( logsize != config_menu->session->logsize)
		{
			if( config_menu->config_menu_listener->change_log_size)
			{
				(*config_menu->config_menu_listener->change_log_size)(
					config_menu->config_menu_listener->self , logsize) ;
			}
		}

		if( fontsize != config_menu->session->fontsize)
		{
			if( config_menu->config_menu_listener->change_font_size)
			{
				(*config_menu->config_menu_listener->change_font_size)(
					config_menu->config_menu_listener->self , fontsize) ;
			}
		}

		if( line_space != config_menu->session->line_space)
		{
			if( config_menu->config_menu_listener->change_line_space)
			{
				(*config_menu->config_menu_listener->change_line_space)(
					config_menu->config_menu_listener->self , line_space) ;
			}
		}
		
		if( screen_width_ratio != config_menu->session->screen_width_ratio)
		{
			if( config_menu->config_menu_listener->change_screen_width_ratio)
			{
				(*config_menu->config_menu_listener->change_screen_width_ratio)(
					config_menu->config_menu_listener->self , screen_width_ratio) ;
			}
		}
		
		if( screen_height_ratio != config_menu->session->screen_height_ratio)
		{
			if( config_menu->config_menu_listener->change_screen_height_ratio)
			{
				(*config_menu->config_menu_listener->change_screen_height_ratio)(
					config_menu->config_menu_listener->self , screen_height_ratio) ;
			}
		}
		
		if( mod_meta_mode != config_menu->session->mod_meta_mode)
		{
			if( config_menu->config_menu_listener->change_mod_meta_mode)
			{
				(*config_menu->config_menu_listener->change_mod_meta_mode)(
					config_menu->config_menu_listener->self , mod_meta_mode) ;
			}
		}

		if( bel_mode != config_menu->session->bel_mode)
		{
			if( config_menu->config_menu_listener->change_bel_mode)
			{
				(*config_menu->config_menu_listener->change_bel_mode)(
					config_menu->config_menu_listener->self , bel_mode) ;
			}
		}

		if( vertical_mode != config_menu->session->vertical_mode)
		{
			if( config_menu->config_menu_listener->change_vertical_mode)
			{
				(*config_menu->config_menu_listener->change_vertical_mode)(
					config_menu->config_menu_listener->self , vertical_mode) ;
			}
		}

		if( sb_mode != config_menu->session->sb_mode)
		{
			if( config_menu->config_menu_listener->change_sb_mode)
			{
				(*config_menu->config_menu_listener->change_sb_mode)(
					config_menu->config_menu_listener->self , sb_mode) ;
			}
		}

		if( use_char_combining != config_menu->session->use_char_combining)
		{
			if( config_menu->config_menu_listener->change_char_combining_flag)
			{
				(*config_menu->config_menu_listener->change_char_combining_flag)(
					config_menu->config_menu_listener->self , use_char_combining) ;
			}
		}

		if( use_dynamic_comb != config_menu->session->use_dynamic_comb)
		{
			if( config_menu->config_menu_listener->change_dynamic_comb_flag)
			{
				(*config_menu->config_menu_listener->change_dynamic_comb_flag)(
					config_menu->config_menu_listener->self , use_dynamic_comb) ;
			}
		}

		if( copy_paste_via_ucs != config_menu->session->copy_paste_via_ucs)
		{
			if( config_menu->config_menu_listener->change_copy_paste_via_ucs_flag)
			{
				(*config_menu->config_menu_listener->change_copy_paste_via_ucs_flag)(
					config_menu->config_menu_listener->self , copy_paste_via_ucs) ;
			}
		}
		
		if( is_transparent != config_menu->session->is_transparent)
		{
			if( config_menu->config_menu_listener->change_transparent_flag)
			{
				(*config_menu->config_menu_listener->change_transparent_flag)(
					config_menu->config_menu_listener->self , is_transparent) ;
			}
		}

		if( brightness != config_menu->session->brightness)
		{
			if( config_menu->config_menu_listener->change_brightness)
			{
				(*config_menu->config_menu_listener->change_brightness)(
					config_menu->config_menu_listener->self , brightness) ;
			}
		}
		
		if( fade_ratio != config_menu->session->fade_ratio)
		{
			if( config_menu->config_menu_listener->change_fade_ratio)
			{
				(*config_menu->config_menu_listener->change_fade_ratio)(
					config_menu->config_menu_listener->self , fade_ratio) ;
			}
		}

		if( font_present != config_menu->session->font_present)
		{
			if( config_menu->config_menu_listener->change_font_present)
			{
				(*config_menu->config_menu_listener->change_font_present)(
					config_menu->config_menu_listener->self , font_present) ;
			}
		}

		if( use_multi_col_char != config_menu->session->use_multi_col_char)
		{
			if( config_menu->config_menu_listener->change_multi_col_char_flag)
			{
				(*config_menu->config_menu_listener->change_multi_col_char_flag)(
					config_menu->config_menu_listener->self , use_multi_col_char) ;
			}
		}
		
		if( use_bidi != config_menu->session->use_bidi)
		{
			if( config_menu->config_menu_listener->change_bidi_flag)
			{
				(*config_menu->config_menu_listener->change_bidi_flag)(
					config_menu->config_menu_listener->self , use_bidi) ;
			}
		}

		if( strcmp( sb_view_name , config_menu->session->sb_view_name) != 0)
		{
			if( config_menu->config_menu_listener->change_sb_view)
			{
				(*config_menu->config_menu_listener->change_sb_view)(
					config_menu->config_menu_listener->self , sb_view_name) ;
			}
		}
		
		if( strcmp( xim , config_menu->session->xim) != 0)
		{
			if( config_menu->config_menu_listener->change_xim)
			{
				if( strcmp( locale , "NULL") == 0)
				{
					locale = NULL ;
				}
				
				(*config_menu->config_menu_listener->change_xim)(
					config_menu->config_menu_listener->self , xim , locale) ;
			}
		}

		if( strcmp( wall_pic , config_menu->session->wall_pic) != 0)
		{
			if( config_menu->config_menu_listener->change_wall_picture)
			{
				(*config_menu->config_menu_listener->change_wall_picture)(
					config_menu->config_menu_listener->self , wall_pic) ;
			}
		}
	}
	else if( strcmp( command , "FONT") == 0 && input_line)
	{
		/*
		 * FONT:(larger|smaller)
		 */
		 
		if( strcmp( input_line , "larger") == 0)
		{
			if( config_menu->config_menu_listener->larger_font_size)
			{
				(*config_menu->config_menu_listener->larger_font_size)(
					config_menu->config_menu_listener->self) ;
			}
		}
		else if( strcmp( input_line , "smaller") == 0)
		{
			if( config_menu->config_menu_listener->smaller_font_size)
			{
				(*config_menu->config_menu_listener->smaller_font_size)(
					config_menu->config_menu_listener->self) ;
			}
		}
	}
	else if( strcmp( command , "FULL_RESET") == 0)
	{
		/*
		 * FULL_RESET
		 */
		 
		if( config_menu->config_menu_listener->full_reset)
		{
			(*config_menu->config_menu_listener->full_reset)(
				config_menu->config_menu_listener->self) ;
		}
	}
#ifdef  DEBUG
	else
	{
		kik_warn_printf( KIK_DEBUG_TAG " unknown command %s\n" , command) ;
	}
#endif

end:
	free( config_menu->session) ;
	config_menu->session = NULL ;
}


/* --- global functions --- */

int
ml_config_menu_init(
	ml_config_menu_t *  config_menu ,
	char *  command_path ,
	ml_config_menu_event_listener_t *  config_menu_listener
	)
{
	if( command_path)
	{
		config_menu->command_path = strdup( command_path) ;
	}
	else
	{
		config_menu->command_path = strdup( MLCONFIG_PATH) ;
	}
	
	config_menu->config_menu_listener = config_menu_listener ;
	config_menu->session = NULL ;

	ml_add_sig_child_listener( config_menu , sig_child) ;

	return  1 ;
}

int
ml_config_menu_final(
	ml_config_menu_t *  config_menu
	)
{
	free( config_menu->command_path) ;
	ml_remove_sig_child_listener( config_menu) ;

	if( config_menu->session)
	{
		free( config_menu->session) ;
	}

	return  1 ;
}

int
ml_config_menu_start(
	ml_config_menu_t *  config_menu ,
	int  x ,
	int  y ,
	ml_char_encoding_t  orig_encoding ,
	ml_iscii_lang_t  orig_iscii_lang ,
	char *  orig_fg_color ,
	char *  orig_bg_color ,
	char *  orig_sb_fg_color ,
	char *  orig_sb_bg_color ,
	u_int  orig_tabsize ,
	u_int  orig_logsize ,
	u_int  orig_fontsize ,
	u_int  min_fontsize ,
	u_int  max_fontsize ,
	u_int  orig_line_space ,
	u_int  orig_screen_width_ratio ,
	u_int  orig_screen_height_ratio ,
	ml_mod_meta_mode_t  orig_mod_meta_mode ,
	ml_bel_mode_t  orig_bel_mode ,
	ml_vertical_mode_t  orig_vertical_mode ,
	ml_sb_mode_t  orig_sb_mode ,
	int  orig_use_char_combining ,
	int  orig_use_dynamic_comb ,
	int  orig_copy_paste_via_ucs ,
	int  orig_is_transparent ,
	u_int  orig_brightness ,
	u_int  orig_fade_ratio ,
	ml_font_present_t  orig_font_present ,
	int  orig_use_multi_col_char ,
	int  orig_use_bidi ,
	char *  orig_sb_view_name ,
	char *  orig_xim ,
	char *  orig_locale ,
	char *  orig_wall_pic
	)
{
	pid_t  pid ;
	int  fds[2] ;
	FILE *  fp ;

	if( config_menu->session)
	{
		/* config_menu window is active now */
		
		return  0 ;
	}

	if( pipe( fds) == -1)
	{
		return  0 ;
	}

	pid = fork() ;
	if( pid == -1)
	{
		return  0 ;
	}

	if( pid == 0)
	{
		/* child process */

		char *  args[6] ;
		char  geom_x[10] ;
		char  geom_y[10] ;
		char  read_fd[10] ;
		char  write_fd[10] ;

		args[0] = config_menu->command_path ;
		
		sprintf( geom_x , "%d" , x) ;
		sprintf( geom_y , "%d" , y) ;

		args[1] = geom_x ;
		args[2] = geom_y ;
		
		sprintf( read_fd , "%d" , fds[0]) ;
		sprintf( write_fd , "%d" , fds[1]) ;
		
		args[3] = read_fd ;
		args[4] = write_fd ;
		args[5] = NULL ;

		if( execv( config_menu->command_path , args) == -1)
		{
			/* failed */

			kik_msg_printf( "%s is not found.\n" , config_menu->command_path) ;
			
			exit(1) ;
		}
	}

	/* parent process */

	if( ( config_menu->session = malloc( sizeof( ml_config_menu_session_t))) == NULL)
	{
		return  0 ;
	}

	if( ( fp = fdopen( fds[1] , "w")) == NULL)
	{
		close( fds[0]) ;
		close( fds[1]) ;
		
		free( config_menu->session) ;
		config_menu->session = NULL ;
		
		return  0 ;
	}

	/*
	 * [encoding] [iscii lang] [fg color] [bg color] [sb fg color] [sb bg color] [tabsize] \
	 * [logsize] [font size] [min font size] [max font size] [line space] [screen width ratio] \
	 * [screen height ratio] [mod meta mode] [bel mode] [vertical mode] [sb mode] \
	 * [char combining] [dynamic comb] [copy paste via ucs] [is transparent] [shade ratio] \
	 * [fade ratio] [font present] [use multi col char] [use bidi] [use multi col char] \
	 * [sb view name] [xim] [locale] [wall pic][LF]
	 */
	fprintf( fp ,
		"%d %d %s %s %s %s %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d "
		"%s %s %s %s\n" ,
		orig_encoding , orig_iscii_lang , orig_fg_color , orig_bg_color ,
		orig_sb_fg_color , orig_sb_bg_color , orig_tabsize , orig_logsize , orig_fontsize ,
		min_fontsize , max_fontsize , orig_line_space ,
		orig_screen_width_ratio , orig_screen_height_ratio ,
		orig_mod_meta_mode , orig_bel_mode , orig_vertical_mode , orig_sb_mode ,
		orig_use_char_combining , orig_use_dynamic_comb , orig_copy_paste_via_ucs ,
		orig_is_transparent , orig_brightness , orig_fade_ratio , orig_font_present ,
		orig_use_multi_col_char , orig_use_bidi , orig_sb_view_name , orig_xim ,
		orig_locale , orig_wall_pic) ;
	fclose( fp) ;

	/*
	 * saving session status.
	 */
	 
	config_menu->session->pid = pid ;
	config_menu->session->fd = fds[0] ;
	
	config_menu->session->encoding = orig_encoding ;
	config_menu->session->iscii_lang = orig_iscii_lang ;
	config_menu->session->fg_color = orig_fg_color ;
	config_menu->session->bg_color = orig_bg_color ;
	config_menu->session->sb_fg_color = orig_sb_fg_color ;
	config_menu->session->sb_bg_color = orig_sb_bg_color ;
	config_menu->session->tabsize = orig_tabsize ;
	config_menu->session->logsize = orig_logsize ;
	config_menu->session->fontsize = orig_fontsize ;
	config_menu->session->line_space = orig_line_space ;
	config_menu->session->screen_width_ratio = orig_screen_width_ratio ;
	config_menu->session->screen_height_ratio = orig_screen_height_ratio ;
	config_menu->session->mod_meta_mode = orig_mod_meta_mode ;
	config_menu->session->bel_mode = orig_bel_mode ;
	config_menu->session->vertical_mode = orig_vertical_mode ;
	config_menu->session->sb_mode = orig_sb_mode ;
	config_menu->session->use_char_combining = orig_use_char_combining ;
	config_menu->session->use_dynamic_comb = orig_use_dynamic_comb ;
	config_menu->session->copy_paste_via_ucs = orig_copy_paste_via_ucs ;
	config_menu->session->is_transparent = orig_is_transparent ;
	config_menu->session->brightness = orig_brightness ;
	config_menu->session->fade_ratio = orig_fade_ratio ;
	config_menu->session->font_present = orig_font_present ;
	config_menu->session->use_multi_col_char = orig_use_multi_col_char ;
	config_menu->session->use_bidi = orig_use_bidi ;
	config_menu->session->sb_view_name = orig_sb_view_name ;
	config_menu->session->xim = orig_xim ;
	config_menu->session->locale = orig_locale ;
	config_menu->session->wall_pic = orig_wall_pic ;

	return  1 ;
}
