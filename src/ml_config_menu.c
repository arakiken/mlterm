/*
 *	$Id$
 */

#include  "ml_config_menu.h"

#include  <sys/wait.h>
#include  <stdio.h>
#include  <unistd.h>	/* pipe/close */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_str.h>	/* kik_str_sep/kik_str_to_int */
#include  <kiklib/kik_mem.h>	/* alloca */


#ifndef  LIBEXECDIR
#define  MLCONFIG_PATH  "/usr/local/libexec/mlconfig"
#else
#define  MLCONFIG_PATH  LIBEXECDIR "/mlconfig"
#endif


/* --- global --- */

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

	return  1 ;
}

int
ml_config_menu_final(
	ml_config_menu_t *  config_menu
	)
{
	free( config_menu->command_path) ;

	return  1 ;
}

int
ml_config_menu_start(
	ml_config_menu_t *  config_menu ,
	int  x ,
	int  y ,
	ml_encoding_type_t  orig_encoding ,
	ml_color_t  orig_fg_color ,
	ml_color_t  orig_bg_color ,
	u_int  orig_tabsize ,
	u_int  orig_logsize ,
	u_int  orig_fontsize ,
	u_int  min_fontsize ,
	u_int  max_fontsize ,
	ml_mod_meta_mode_t  orig_mod_meta_mode ,
	ml_bel_mode_t  orig_bel_mode ,
	int  orig_is_combining_char ,
	int  orig_pre_conv_xct_to_ucs ,
	int  orig_is_transparent ,
	int  orig_is_aa ,
	int  orig_is_bidi ,
	char *  orig_xim ,
	char *  orig_locale
	)
{
	char *  command ;
	int  fds[2] ;
	pid_t  pid ;
	int  state ;
	FILE *  fp ;
	kik_file_t *  kin ;
	char *  p ;
	char *  input_line ;
	size_t  len ;

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

	if( ( fp = fdopen( fds[1] , "w")) == NULL)
	{
		close( fds[0]) ;
		close( fds[1]) ;
		
		return  0 ;
	}

	/*
	 * output format
	 * [encoding] [fg color] [bg color] [tabsize] [logsize] [font size] [min font size] \
	 * [max font size] [mod meta mode] [bel mode] [is combining char] [pre conv xct to ucs]
	 * [is transparent] [is aa] [is bidi] [xim] [locale][LF]
	 */
	fprintf( fp , "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %s %s\n" ,
		orig_encoding , orig_fg_color , orig_bg_color , orig_tabsize ,
		orig_logsize , orig_fontsize , min_fontsize , max_fontsize ,
		orig_mod_meta_mode , orig_bel_mode , orig_is_combining_char ,
		orig_pre_conv_xct_to_ucs , orig_is_transparent , orig_is_aa , orig_is_bidi ,
		orig_xim , orig_locale) ;
	fclose( fp) ;
	
	waitpid( pid , &state , WUNTRACED) ;
	
	if( ( fp = fdopen( fds[0] , "r")) == NULL)
	{
		return  0 ;
	}

	if( ( kin = kik_file_new( fp)) == NULL)
	{
		fclose( fp) ;
	
		return  0 ;
	}
	
	if( ( p = kik_file_get_line( kin , &len)) == NULL || len == 0)
	{
		kik_file_delete( kin) ;
		fclose( fp) ;
			
		return  0 ;
	}

	if( ( input_line = alloca( len)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif
	
		kik_file_delete( kin) ;
		fclose( fp) ;
		
		return  0 ;
	}
	
	strncpy( input_line , p , len - 1) ;
	input_line[len - 1] = '\0' ;

	kik_file_delete( kin) ;
	fclose( fp) ;

	if( ( command = kik_str_sep( &input_line , ":")) == NULL)
	{
		return  0 ;
	}

	if( strcmp( command , "CONFIG") == 0)
	{
		int  encoding ;
		int  fg_color ;
		int  bg_color ;
		u_int  tabsize ;
		u_int  logsize ;
		u_int  fontsize ;
		int  mod_meta_mode ;
		int  bel_mode ;
		int  is_combining_char ;
		int  pre_conv_xct_to_ucs ;
		int  is_transparent ;
		int  is_aa ;
		int  is_bidi ;
		char *  xim ;
		char *  locale ;

		/*
		 * input_line format
		 * CONFIG:[encoding] [tabsize] [logsize] [fontsize] [mod meta mode] \
		 * [is combining char] [pre conv xct to ucs] [is transparent] [is aa] \
		 * [xim] [locale][LF]
		 */
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( (int*)&encoding , p))
		{
			return  0 ;
		}

		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( (int*)&fg_color , p))
		{
			return  0 ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( (int*)&bg_color , p))
		{
			return  0 ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &tabsize , p))
		{
			return  0 ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &logsize , p))
		{
			return  0 ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &fontsize , p))
		{
			return  0 ;
		}

		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &mod_meta_mode , p))
		{
			return  0 ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &bel_mode , p))
		{
			return  0 ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &is_combining_char , p))
		{
			return  0 ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &pre_conv_xct_to_ucs , p))
		{
			return  0 ;
		}
		
		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &is_transparent , p))
		{
			return  0 ;
		}

		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &is_aa , p))
		{
			return  0 ;
		}

		if( ( p = kik_str_sep( &input_line , " ")) == NULL ||
			! kik_str_to_int( &is_bidi , p))
		{
			return  0 ;
		}
		
		if( ( xim = kik_str_sep( &input_line , " ")) == NULL)
		{
			return  0 ;
		}
		
		if( ( locale = input_line) == NULL)
		{
			return  0 ;
		}

		if( encoding != orig_encoding)
		{
			if( config_menu->config_menu_listener->change_encoding)
			{
				(*config_menu->config_menu_listener->change_encoding)(
					config_menu->config_menu_listener->self , encoding) ;
			}
		}

		if( fg_color != orig_fg_color)
		{
			if( config_menu->config_menu_listener->change_fg_color)
			{
				(*config_menu->config_menu_listener->change_fg_color)(
					config_menu->config_menu_listener->self , fg_color) ;
			}
		}

		if( bg_color != orig_bg_color)
		{
			if( config_menu->config_menu_listener->change_bg_color)
			{
				(*config_menu->config_menu_listener->change_bg_color)(
					config_menu->config_menu_listener->self , bg_color) ;
			}
		}
		
		if( tabsize != orig_tabsize)
		{
			if( config_menu->config_menu_listener->change_tab_size)
			{
				(*config_menu->config_menu_listener->change_tab_size)(
					config_menu->config_menu_listener->self , tabsize) ;
			}
		}

		if( logsize != orig_logsize)
		{
			if( config_menu->config_menu_listener->change_log_size)
			{
				(*config_menu->config_menu_listener->change_log_size)(
					config_menu->config_menu_listener->self , logsize) ;
			}
		}

		if( fontsize != orig_fontsize)
		{
			if( config_menu->config_menu_listener->change_font_size)
			{
				(*config_menu->config_menu_listener->change_font_size)(
					config_menu->config_menu_listener->self , fontsize) ;
			}
		}

		if( mod_meta_mode != orig_mod_meta_mode)
		{
			if( config_menu->config_menu_listener->change_mod_meta_mode)
			{
				(*config_menu->config_menu_listener->change_mod_meta_mode)(
					config_menu->config_menu_listener->self , mod_meta_mode) ;
			}
		}

		if( bel_mode != orig_bel_mode)
		{
			if( config_menu->config_menu_listener->change_bel_mode)
			{
				(*config_menu->config_menu_listener->change_bel_mode)(
					config_menu->config_menu_listener->self , bel_mode) ;
			}
		}

		if( is_combining_char != orig_is_combining_char)
		{
			if( config_menu->config_menu_listener->change_char_combining_flag)
			{
				(*config_menu->config_menu_listener->change_char_combining_flag)(
					config_menu->config_menu_listener->self , is_combining_char) ;
			}
		}

		if( pre_conv_xct_to_ucs != orig_pre_conv_xct_to_ucs)
		{
			if( config_menu->config_menu_listener->change_pre_conv_xct_to_ucs_flag)
			{
				(*config_menu->config_menu_listener->change_pre_conv_xct_to_ucs_flag)(
					config_menu->config_menu_listener->self , pre_conv_xct_to_ucs) ;
			}
		}
		
		if( is_transparent != orig_is_transparent)
		{
			if( config_menu->config_menu_listener->change_transparent_flag)
			{
				(*config_menu->config_menu_listener->change_transparent_flag)(
					config_menu->config_menu_listener->self , is_transparent) ;
			}
		}

		if( is_aa != orig_is_aa)
		{
			if( config_menu->config_menu_listener->change_aa_flag)
			{
				(*config_menu->config_menu_listener->change_aa_flag)(
					config_menu->config_menu_listener->self , is_aa) ;
			}
		}

		if( is_bidi != orig_is_bidi)
		{
			if( config_menu->config_menu_listener->change_bidi_flag)
			{
				(*config_menu->config_menu_listener->change_bidi_flag)(
					config_menu->config_menu_listener->self , is_bidi) ;
			}
		}
		
		if( strcmp( xim , orig_xim) != 0)
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
		else
		{
			return  0 ;
		}
	}
	else if( strcmp( command , "WALL_PIC") == 0 && input_line)
	{
		if( strcmp( input_line , "off") == 0)
		{
			if( config_menu->config_menu_listener->unset_wall_picture)
			{
				(*config_menu->config_menu_listener->unset_wall_picture)(
					config_menu->config_menu_listener->self) ;
			}
		}
		else
		{
			if( config_menu->config_menu_listener->change_wall_picture)
			{
				(*config_menu->config_menu_listener->change_wall_picture)(
					config_menu->config_menu_listener->self , input_line) ;
			}
		}
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " unknown command %s\n" , command) ;
	#endif
	
		return  0 ;
	}

	return  1 ;
}
