/*
 *	$Id$
 */

#include  "ml_term.h"

#include  <kiklib/kik_mem.h>	/* malloc/free */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* strdup */

#include  "ml_pty.h"
#include  "ml_vt100_parser.h"
#include  "ml_screen.h"


/* --- global variables --- */

#ifdef  ENABLE_SIXEL
/* XXX */
void (*ml_term_pty_closed_event)( ml_term_t *) ;
#endif


/* --- global functions --- */

ml_term_t *
ml_term_new(
	u_int  cols ,
	u_int  rows ,
	u_int  tab_size ,
	u_int  log_size ,
	ml_char_encoding_t  encoding ,
	int  is_auto_encoding ,
	ml_unicode_policy_t  policy ,
	u_int  col_size_a ,
	int  use_char_combining ,
	int  use_multi_col_char ,
	int  use_bidi ,
	ml_bidi_mode_t  bidi_mode ,
	int  use_ind ,
	int  use_bce ,
	int  use_dynamic_comb ,
	ml_bs_mode_t  bs_mode ,
	ml_vertical_mode_t  vertical_mode ,
	int  use_local_echo
	)
{
	ml_term_t *  term ;

	if( ( term = calloc( 1 , sizeof( ml_term_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif
	
		return  NULL ;
	}

	if( ( term->screen = ml_screen_new( cols , rows ,
				tab_size , log_size , use_bce , bs_mode)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_screen_new failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( ( term->parser = ml_vt100_parser_new( term->screen , encoding , policy , col_size_a ,
				use_char_combining , use_multi_col_char)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_vt100_parser_new failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( ! ml_config_menu_init( &term->config_menu))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_config_menu_init failed.\n") ;
	#endif
	
		goto  error ;
	}

	term->vertical_mode = vertical_mode ;
	term->bidi_mode = bidi_mode ;
	term->use_bidi = use_bidi ;
	term->use_ind = use_ind ;
	term->use_dynamic_comb = use_dynamic_comb ;

	term->is_auto_encoding = is_auto_encoding ;

	term->use_local_echo = use_local_echo ;

	return  term ;

error:
	if( term->screen)
	{
		ml_screen_delete( term->screen) ;
	}

	if( term->parser)
	{
		ml_vt100_parser_delete( term->parser) ;
	}

	free( term) ;

	return  NULL ;
}

int
ml_term_delete(
	ml_term_t *  term
	)
{
#ifdef  ENABLE_SIXEL
	if( ml_term_pty_closed_event)
	{
		(*ml_term_pty_closed_event)( term) ;
	}
#endif

	if( term->pty)
	{
		ml_pty_delete( term->pty) ;
	}
	
	if( term->shape)
	{
		(*term->shape->delete)( term->shape) ;
	}

	free( term->win_name) ;
	free( term->icon_name) ;
	free( term->icon_path) ;

	ml_screen_delete( term->screen) ;
	ml_vt100_parser_delete( term->parser) ;

	ml_config_menu_final( &term->config_menu) ;

	free( term) ;

	return  1 ;
}

int
ml_term_zombie(
	ml_term_t *  term
	)
{
	if( term->pty)
	{
		ml_pty_ptr_t  pty ;

		pty = term->pty ;

		/* Should be NULL because ml_pty_delete calls term->pty_listener->closed. */
		term->pty = NULL ;

		ml_pty_delete( pty) ;
	}
#ifdef  DEBUG
	else
	{
		kik_debug_printf( KIK_DEBUG_TAG " term is already zombie.\n") ;
	}
#endif

	return  1 ;
}

int
ml_term_open_pty(
	ml_term_t *  term ,
	const char *  cmd_path ,
	char **  argv ,
	char **  env ,
	const char *  host ,
	const char *  pass ,
	const char *  pubkey ,
	const char *  privkey
	)
{
	if( ! term->pty)
	{
		ml_pty_ptr_t  pty ;

		if( ! ( pty = ml_pty_new( cmd_path , argv , env , host , pass , pubkey , privkey ,
				ml_screen_get_logical_cols( term->screen) ,
				ml_screen_get_logical_rows( term->screen))))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_pty_new failed.\n") ;
		#endif

			return  0 ;
		}

		ml_term_plug_pty( term , pty) ;
	}

	return  1 ;
}

int
ml_term_plug_pty(
	ml_term_t *  term ,
	ml_pty_ptr_t  pty	/* Not NULL */
	)
{
	if( ! term->pty)
	{
		term->pty = pty ;

		if( term->pty_listener)
		{
			ml_pty_set_listener( term->pty, term->pty_listener) ;
			term->pty_listener = NULL ;
		}

		ml_vt100_parser_set_pty( term->parser , term->pty) ;
	}
	
	return  1 ;
}

int
ml_term_attach(
	ml_term_t *  term ,
	ml_xterm_event_listener_t *  xterm_listener ,
	ml_config_event_listener_t *  config_listener ,
	ml_screen_event_listener_t *  screen_listener ,
	ml_pty_event_listener_t *  pty_listener
	)
{
  	if( term->is_attached)
        {
          	/* already attached */
          	return  0 ;
        }

	ml_vt100_parser_set_xterm_listener( term->parser , xterm_listener) ;
	ml_vt100_parser_set_config_listener( term->parser , config_listener) ;
	ml_screen_set_listener( term->screen , screen_listener) ;
	
	if( term->pty)
	{
		ml_pty_set_listener( term->pty , pty_listener) ;
	}
	else
	{
		term->pty_listener = pty_listener ;
	}

	term->is_attached = 1 ;

	return  1 ;
}

int
ml_term_detach(
	ml_term_t *  term
	)
{
  	if( ! term->is_attached)
        {
          	/* already detached. */
          	return  0 ;
        }

	ml_vt100_parser_set_xterm_listener( term->parser , NULL) ;
	ml_vt100_parser_set_config_listener( term->parser , NULL) ;
	ml_screen_set_listener( term->screen , NULL) ;

	if( term->pty)
	{
		ml_pty_set_listener( term->pty , NULL) ;
	}
	else
	{
		term->pty_listener = NULL ;
	}

	term->is_attached = 0 ;

	return  1 ;
}

int
ml_term_set_auto_encoding(
	ml_term_t *  term ,
	int  is_auto_encoding
	)
{
	term->is_auto_encoding = is_auto_encoding ;

	return  1 ;
}

int
ml_term_set_use_bidi(
	ml_term_t *  term ,
	int  flag
	)
{
	term->use_bidi = flag ;

	return  1 ;
}

int
ml_term_set_bidi_mode(
	ml_term_t *  term ,
	ml_bidi_mode_t  mode
	)
{
	term->bidi_mode = mode ;

	return  1 ;
}

int
ml_term_set_use_ind(
	ml_term_t *  term ,
	int  flag
	)
{
	term->use_ind = flag ;

	return  1 ;
}

int
ml_term_set_vertical_mode(
	ml_term_t *  term ,
	ml_vertical_mode_t  mode
	)
{
	term->vertical_mode = mode ;

	return  1 ;
}

int
ml_term_set_use_dynamic_comb(
	ml_term_t *  term ,
	int  flag
	)
{
	term->use_dynamic_comb = flag ;

	return  1 ;
}

int
ml_term_set_use_local_echo(
	ml_term_t *  term ,
	int  flag
	)
{
	if( term->use_local_echo != flag && ! ( term->use_local_echo = flag))
	{
		ml_screen_logical( term->screen) ;
		ml_screen_disable_local_echo( term->screen) ;
		ml_screen_visual( term->screen) ;
	}

	return  1 ;
}

int
ml_term_get_master_fd(
	ml_term_t *  term
	)
{
	if( term->pty == NULL)
	{
		return  -1 ;
	}
	
	return  ml_pty_get_master_fd( term->pty) ;
}

int
ml_term_get_slave_fd(
	ml_term_t *  term
	)
{
	if( term->pty == NULL)
	{
		return  -1 ;
	}
	
	return  ml_pty_get_slave_fd( term->pty) ;
}

/*
 * Always return non-NULL value.
 * XXX Static data can be returned. (Not reentrant)
 */
char *
ml_term_get_slave_name(
	ml_term_t *  term
	)
{
	if( term->pty == NULL)
	{
		return  "/dev/zombie" ;
	}

	return  ml_pty_get_slave_name( term->pty) ;
}

pid_t
ml_term_get_child_pid(
	ml_term_t *  term
	)
{
	if( term->pty == NULL)
	{
		return  -1 ;
	}
	
	return  ml_pty_get_pid( term->pty) ;
}

size_t
ml_term_write(
	ml_term_t *  term ,
	u_char *  buf ,
	size_t  len ,
	int  to_menu
	)
{
	if( to_menu)
	{
		return  ml_config_menu_write( &term->config_menu , buf , len) ;
	}
	else
	{
		if( term->pty == NULL)
		{
			return  0 ;
		}

		if( term->use_local_echo)
		{
			ml_vt100_parser_local_echo( term->parser , buf , len) ;
		}

		return  ml_write_to_pty( term->pty , buf , len) ;
	}
}

int
ml_term_resize(
	ml_term_t *  term ,
	u_int  cols ,
	u_int  rows
	)
{
	if( term->pty)
	{
		ml_set_pty_winsize( term->pty , cols , rows) ;
	}

	ml_screen_logical( term->screen) ;
	ml_screen_resize( term->screen , cols , rows) ;
	ml_screen_render( term->screen) ;
	ml_screen_visual( term->screen) ;

	return  1 ;
}

int
ml_term_unhighlight_cursor(
	ml_term_t *  term ,
	int  revert_visual
	)
{
	ml_line_t *  line ;
	int  ret ;

#ifdef  DEBUG
	if( term->screen->logvis && ! term->screen->logvis->is_visual)
	{
		kik_debug_printf( KIK_DEBUG_TAG
			" ml_term_unhighlight_cursor() should be called in visual context but"
			" is called in logical context.\n") ;
	}
#endif

	ml_screen_logical( term->screen) ;
	
	if( ( line = ml_screen_get_cursor_line( term->screen)) == NULL || ml_line_is_empty( line))
	{
		ret = 0 ;
	}
	else
	{
		ml_line_set_modified( line , ml_screen_cursor_char_index( term->screen) ,
			ml_screen_cursor_char_index( term->screen)) ;

		ret = 1 ;
	}

	if( revert_visual)
	{
		/* ml_screen_render( term->screen) ; */
		ml_screen_visual( term->screen) ;
	}

	return  ret ;
}

/*
 * Not implemented yet.
 */
#if  0
int
ml_term_set_modified_region(
	ml_term_t *  term ,
	int  beg_char_index ,
	int  beg_row ,
	u_int  nchars ,
	u_int  nrows
	)
{
	return  0 ;
}
#endif

/*
 * Not used.
 */
#if  0
int
ml_term_set_modified_region_in_screen(
	ml_term_t *  term ,
	int  beg_char_index ,
	int  beg_row ,
	u_int  nchars ,
	u_int  nrows
	)
{
	int  row ;
	ml_line_t *  line ;

#ifdef  DEBUG
	if( term->screen->logvis && ! term->screen->logvis->is_visual)
	{
		kik_debug_printf( KIK_DEBUG_TAG
			" ml_term_set_modified_lines() should be called in visual context but"
			" is called in logical context.\n") ;
	}
#endif

	if( ! ml_screen_logical_visual_is_reversible( term->screen))
	{
		ml_screen_logical( term->screen) ;
	}

	for( row = beg_row ; row < beg_row + nrows ; row ++)
	{
		if( ( line = ml_screen_get_line_in_screen( term->screen , row)))
		{
			ml_line_set_modified( line , beg_char_index , beg_char_index + nchars - 1) ;
		}
	}

	if( ! ml_screen_logical_visual_is_reversible( term->screen))
	{
		/* ml_screen_render( term->screen) ; */
		ml_screen_visual( term->screen) ;
	}

	return  1 ;
}
#endif

int
ml_term_set_modified_lines(
	ml_term_t *  term ,
	u_int  beg ,
	u_int  end
	)
{
	int  row ;
	ml_line_t *  line ;

#ifdef  DEBUG
	if( term->screen->logvis && ! term->screen->logvis->is_visual)
	{
		kik_debug_printf( KIK_DEBUG_TAG
			" ml_term_set_modified_lines() should be called in visual context but"
			" is called in logical context.\n") ;
	}
#endif

	if( ! ml_screen_logical_visual_is_reversible( term->screen))
	{
		ml_screen_logical( term->screen) ;
	}

	for( row = beg ; row <= end ; row ++)
	{
		if( ( line = ml_screen_get_line( term->screen , row)))
		{
			ml_line_set_modified_all( line) ;
		}
	}

	if( ! ml_screen_logical_visual_is_reversible( term->screen))
	{
		/* ml_screen_render( term->screen) ; */
		ml_screen_visual( term->screen) ;
	}

	return  1 ;
}

int
ml_term_set_modified_lines_in_screen(
	ml_term_t *  term ,
	u_int  beg ,
	u_int  end
	)
{
	int  row ;
	ml_line_t *  line ;

#ifdef  DEBUG
	if( term->screen->logvis && ! term->screen->logvis->is_visual)
	{
		kik_debug_printf( KIK_DEBUG_TAG
			" ml_term_set_modified_lines_in_screen() should be called in visual "
			" context but is called in logical context.\n") ;
	}
#endif

	if( ! ml_screen_logical_visual_is_reversible( term->screen))
	{
		ml_screen_logical( term->screen) ;
	}

	for( row = beg ; row <= end ; row ++)
	{
		if( ( line = ml_screen_get_line_in_screen( term->screen , row)))
		{
			ml_line_set_modified_all( line) ;
		}
	}

	if( ! ml_screen_logical_visual_is_reversible( term->screen))
	{
		/* ml_screen_render( term->screen) ; */
		ml_screen_visual( term->screen) ;
	}

	return  1 ;
}

int
ml_term_set_modified_all_lines_in_screen(
	ml_term_t *  term
	)
{
#ifdef  DEBUG
	if( term->screen->logvis && ! term->screen->logvis->is_visual)
	{
		kik_debug_printf( KIK_DEBUG_TAG
			" ml_term_set_modified_all_lines_in_screen() should be called in "
			" visual context but is called in logical context.\n") ;
	}
#endif

	if( ! ml_screen_logical_visual_is_reversible( term->screen))
	{
		ml_screen_logical( term->screen) ;
	}

	ml_screen_set_modified_all( term->screen) ;

	if( ! ml_screen_logical_visual_is_reversible( term->screen))
	{
		/* ml_screen_render( term->screen) ; */
		ml_screen_visual( term->screen) ;
	}

	return  1 ;
}

int
ml_term_updated_all(
	ml_term_t *  term
	)
{
	int  row ;
	ml_line_t *  line ;

#ifdef  DEBUG
	if( term->screen->logvis && ! term->screen->logvis->is_visual)
	{
		kik_debug_printf( KIK_DEBUG_TAG
			" ml_term_updated_all() should be called in visual context but"
			" is called in logical context.\n") ;
	}
#endif

	if( ! ml_screen_logical_visual_is_reversible( term->screen))
	{
		ml_screen_logical( term->screen) ;
	}

	for( row = 0 ; row < ml_edit_get_rows( term->screen->edit) ; row ++)
	{
		if( ( line = ml_screen_get_line_in_screen( term->screen , row)))
		{
			ml_line_set_updated( line) ;
		}
	}

	if( ! ml_screen_logical_visual_is_reversible( term->screen))
	{
		/* ml_screen_render( term->screen) ; */
		ml_screen_visual( term->screen) ;
	}

	return  1 ;
}

/*
 * Return value:
 *  1 => Updated
 *  0 => Not updated(== not necessary to redraw)
 */
int
ml_term_update_special_visual(
	ml_term_t *  term
	)
{
	ml_logical_visual_t *  logvis ;
	int  had_logvis = 0 ;
	int  has_logvis = 0 ;
	int  need_comb = 0 ;

	if( term->shape)
	{
		(*term->shape->delete)( term->shape) ;
		term->shape = NULL ;
	}

	term->screen->use_dynamic_comb = 0 ;
	had_logvis = ml_screen_delete_logical_visual( term->screen) ;

	if( term->use_dynamic_comb)
	{
		if( ( logvis = ml_logvis_comb_new()))
		{
			if( ml_screen_add_logical_visual( term->screen , logvis))
			{
				has_logvis = 1 ;
				need_comb = 1 ;
				term->screen->use_dynamic_comb = 1 ;
			}
			else
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" ml_screen_add_logical_visual failed.\n") ;
			#endif

				(*logvis->delete)( logvis) ;
			}
		}
	#ifdef  DEBUG
		else
		{
			kik_warn_printf( KIK_DEBUG_TAG " ml_logvis_comb_new() failed.\n") ;
		}
	#endif
	}

	/* Vertical mode, BiDi and ISCII can't coexist. */

	/* Similar if-else conditions exist in update_special_visual in x_screen.c. */
	if( term->vertical_mode)
	{
		if( ( logvis = ml_logvis_vert_new( term->vertical_mode)))
		{
			if( ml_screen_add_logical_visual( term->screen , logvis))
			{
				has_logvis = 1 ;
			}
			else
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" ml_screen_add_logical_visual failed.\n") ;
			#endif

				(*logvis->delete)( logvis) ;
			}
		}
	#ifdef  DEBUG
		else
		{
			kik_warn_printf( KIK_DEBUG_TAG " ml_logvis_vert_new() failed.\n") ;
		}
	#endif
	}
	else if( term->use_bidi && ml_term_get_encoding( term) == ML_UTF8)
	{
		if( ( term->shape = ml_arabic_shape_new()) &&
		    ( logvis = ml_logvis_bidi_new( term->bidi_mode)))
		{
			if( ml_screen_add_logical_visual( term->screen , logvis))
			{
				has_logvis = 1 ;
				need_comb = 1 ;
			}
			else
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" ml_screen_add_logical_visual failed.\n") ;
			#endif

				(*logvis->delete)( logvis) ;
			}
		}
	#ifdef  DEBUG
		else
		{
			kik_warn_printf( KIK_DEBUG_TAG
				" x_arabic_shape_new()/ml_logvis_bidi_new() failed.\n") ;
		}
	#endif
	}
	else if( IS_ISCII_ENCODING( ml_term_get_encoding( term))
	         || (/* ! term->use_bidi && */ term->use_ind &&
	             ml_term_get_encoding( term) == ML_UTF8) )
	{
		if( ( term->shape = ml_iscii_shape_new()) &&
		    ( logvis = ml_logvis_iscii_new()))
		{
			if( ml_screen_add_logical_visual( term->screen , logvis))
			{
				has_logvis = 1 ;
				need_comb = 1 ;
			}
			else
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" ml_screen_add_logical_visual failed.\n") ;
			#endif

				(*logvis->delete)( logvis) ;
			}
		}
	#ifdef  DEBUG
		else
		{
			kik_warn_printf( KIK_DEBUG_TAG
				" ml_iscii_shape_new()/ml_logvis_iscii_new() failed.\n") ;
		}
	#endif
	}

	if( need_comb && ! ml_term_is_using_char_combining( term))
	{
		kik_msg_printf( "Set use_combining=true forcibly.\n") ;
		ml_term_set_use_char_combining( term , 1) ;
	}

	if( ! has_logvis)
	{
		if( term->shape)
		{
			(*term->shape->delete)( term->shape) ;
			term->shape = NULL ;
		}

		term->screen->use_dynamic_comb = 0 ;
		ml_screen_delete_logical_visual( term->screen) ;
	}
	
	if( had_logvis || has_logvis)
	{
		ml_screen_render( term->screen) ;
		ml_screen_visual( term->screen) ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
ml_term_enter_backscroll_mode(
	ml_term_t *  term
	)
{
	/* XXX */
	if( term->vertical_mode)
	{
		kik_msg_printf( "Not supported backscrolling in vertical mode.\n") ;
		
		return  0 ;
	}
	
	return  ml_enter_backscroll_mode( term->screen) ;
}

int
ml_term_set_window_name(
	ml_term_t *  term ,
	char *  name
	)
{
	free( term->win_name) ;
	term->win_name = strdup( name) ;

	return  1 ;
}

int
ml_term_set_icon_name(
	ml_term_t *  term ,
	char *  name
	)
{
	free( term->icon_name) ;
	term->icon_name = strdup( name) ;

	return  1 ;
}

int
ml_term_set_icon_path(
	ml_term_t *  term ,
	char *  path
	)
{
	free( term->icon_path) ;
	term->icon_path = strdup( path) ;

	return 1 ;
}

int
ml_term_start_config_menu(
	ml_term_t *  term ,
	char *  cmd_path ,
	int  x ,
	int  y ,
	char *  display
	)
{
	return  ml_config_menu_start( &term->config_menu , cmd_path ,
				x , y , display , term->pty) ;
}
