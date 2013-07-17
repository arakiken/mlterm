/*
 *	$Id$
 */
#include  <stdio.h>		/* sprintf */
#include  <kiklib/kik_mem.h>	/* malloc/alloca/free */
#include  <kiklib/kik_str.h>	/* kik_str_alloca_dup kik_str_sep */
#include  <kiklib/kik_locale.h>

#include  "x_im.h"
#include  "x_event_source.h"

#ifdef  USE_IM_PLUGIN

#ifndef  LIBDIR
#define  IM_DIR  "/usr/local/lib/mlterm/"
#else
#define  IM_DIR  LIBDIR "/mlterm/"
#endif

typedef  x_im_t * (*x_im_new_func_t)( u_int64_t  magic ,
				      ml_char_encoding_t  term_encoding ,
				      x_im_export_syms_t *  syms ,
				      char *  engine ,
				      u_int  mod_ignore_mask) ;


/* --- static variables --- */

static  x_im_export_syms_t  im_export_syms =
{
	ml_str_init ,
	ml_str_delete ,
	ml_char_combine ,
	ml_char_set ,
	ml_get_char_encoding_name ,
	ml_get_char_encoding ,
	ml_convert_to_internal_ch ,
	ml_isciikey_state_new ,
	ml_isciikey_state_delete ,
	ml_convert_ascii_to_iscii ,
	ml_parser_new ,
	ml_conv_new ,
	x_im_candidate_screen_new ,
	x_im_status_screen_new ,
	x_event_source_add_fd ,
	x_event_source_remove_fd

} ;

#if  1
/* restroing locale which was overwritten by SCIM */
#  define  RESTORE_LOCALE  1
#endif

/* --- static functions --- */

static void *
im_dlopen(
	char *  im_name
	)
{
	char *  libname ;
	void *  handle ;

	if( ! ( libname = alloca( strlen( im_name) + 4)))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif

		return  NULL ;
	}

	sprintf( libname , "im-%s" , im_name) ;

	if( ! ( handle = kik_dl_open( IM_DIR , libname)))
	{
		handle = kik_dl_open( "" , libname) ;
	}

	return  handle ;
}

static int
dlsym_im_new_func(
	char *  im_name ,
	x_im_new_func_t *  func ,
	kik_dl_handle_t *  handle
	)
{
	char * symname ;
#ifdef  USE_FRAMEBUFFER
	char *  fb_im_name ;
#endif

	if( ! im_name || ! ( symname = alloca( strlen( im_name) + 8)))
	{
		return  0 ;
	}

	sprintf( symname , "im_%s_new" , im_name) ;

#ifdef  USE_FRAMEBUFFER
	if( ( fb_im_name = alloca( strlen( im_name) + 3 + 1)))
	{
		sprintf( fb_im_name , "%s-fb" , im_name) ;

		if( ! ( *handle = im_dlopen( fb_im_name)))
		{
#endif
			if( ! ( *handle = im_dlopen( im_name)))
			{
				return  0 ;
			}
#ifdef  USE_FRAMEBUFFER
		}
	}
#endif

	if( ! ( *func = (x_im_new_func_t) kik_dl_func_symbol( *handle , symname)))
	{
		kik_dl_close( *handle) ;

		return  0 ;
	}

	return  1 ;
}


/* --- global functions --- */

x_im_t *
x_im_new(
	x_display_t *  disp ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	ml_char_encoding_t  term_encoding ,
	x_im_event_listener_t *  im_listener ,
	char *  input_method ,
	u_int  mod_ignore_mask
	)
{
	x_im_t *  im ;
	x_im_new_func_t  func ;
	kik_dl_handle_t  handle ;
	char *  im_name ;
	char *  im_attr ;
#ifdef  RESTORE_LOCALE
	char *  cur_locale ;
#endif

	if( input_method == NULL)
	{
		return  NULL ;
	}

	if( strcmp( input_method , "none") == 0)
	{
		return  NULL ;
	}

	if( strchr( input_method , ':'))
	{
		im_attr = kik_str_alloca_dup( input_method) ;

		if( ( im_name = kik_str_sep( &im_attr , ":")) == NULL)
		{
		#ifdef  DEBUG
			kik_error_printf( "%s is illegal input method.\n" , input_method) ;
		#endif

			return  NULL ;
		}
	}
	else
	{
		im_name = kik_str_alloca_dup( input_method) ;
		im_attr = NULL ;
	}

#ifdef  RESTORE_LOCALE
	cur_locale = kik_str_alloca_dup( kik_get_locale()) ;
#endif

	if ( ! dlsym_im_new_func( im_name , &func , &handle))
	{
	#ifdef  RESTORE_LOCALE
		kik_locale_init( cur_locale) ;
	#endif
		kik_error_printf( "%s: Could not load.\n" , im_name) ;
		return  NULL ;
	}

#ifdef  RESTORE_LOCALE
	kik_locale_init( cur_locale) ;
#endif

	if( ! ( im = (*func)( IM_API_COMPAT_CHECK_MAGIC , term_encoding ,
			      &im_export_syms , im_attr , mod_ignore_mask)))
	{
		kik_error_printf( "%s: Could not open.\n" , im_name) ;

		/*
		 * Even if ibus daemon was not found, ibus_init() has been
		 * already called in im_ibus_new().
		 * So if im-ibus module is unloaded here, ibus_init()
		 * will be called again and segfault will happen when
		 * im-ibus module is loaded next time.
		 */
		if( strcmp( im_name , "ibus") != 0)
		{
			kik_dl_close( handle) ;
		}
		else
		{
			kik_dl_close_at_exit( handle) ;
		}

		return  NULL ;
	}


	/*
	 * initializations for x_im_t
	 */
	im->handle = handle ;
	im->name = strdup( im_name) ;
	im->disp = disp ;
	im->font_man = font_man ;
	im->color_man = color_man ;
	im->listener = im_listener ;
	im->cand_screen = NULL ;
	im->stat_screen = NULL ;
	im->preedit.chars = NULL ;
	im->preedit.num_of_chars = 0 ;
	im->preedit.filled_len = 0 ;
	im->preedit.segment_offset = 0 ;
	im->preedit.cursor_offset = X_IM_PREEDIT_NOCURSOR ;

	return  im ;
}

void
x_im_delete(
	x_im_t *  im
	)
{
	kik_dl_handle_t  handle ;
	int  is_ibus ;

	if( strcmp( im->name , "ibus") == 0)
	{
		is_ibus = 1 ;
	}
	else
	{
		is_ibus = 0 ;
	}

	free( im->name) ;

	if( im->cand_screen)
	{
		(*im->cand_screen->delete)( im->cand_screen) ;
	}

	if( im->stat_screen)
	{
		(*im->stat_screen->delete)( im->stat_screen) ;
	}

	if( im->preedit.chars)
	{
		ml_str_delete( im->preedit.chars , im->preedit.num_of_chars) ;
	}

	handle = im->handle ;

	(*im->delete)( im) ;

	/*
	 * Don't unload libim-ibus.so because it depends on glib which works
	 * unexpectedly.
	 */
	if( ! is_ibus)
	{
		kik_dl_close( handle) ;
	}
}

void
x_im_redraw_preedit(
	x_im_t *  im ,
	int  is_focused
	)
{
	(*im->listener->draw_preedit_str)( im->listener->self ,
					   im->preedit.chars ,
					   im->preedit.filled_len ,
					   im->preedit.cursor_offset) ;

	if( ! im->cand_screen && ! im->stat_screen)
	{
		return ;
	}

	if( is_focused)
	{
		int  x ;
		int  y ;

		if( (*im->listener->get_spot)( im->listener->self ,
					       im->preedit.chars ,
					       im->preedit.segment_offset ,
					       &x , &y))
		{
			if( im->stat_screen &&
			    (im->cand_screen && im->preedit.filled_len))
			{
				(*im->stat_screen->hide)( im->stat_screen) ;
				(*im->cand_screen->show)( im->cand_screen) ;
				(*im->cand_screen->set_spot)( im->cand_screen ,
							      x , y) ;
			}
			else if( im->stat_screen)
			{
				(*im->stat_screen->show)( im->stat_screen) ;
				(*im->stat_screen->set_spot)( im->stat_screen ,
							      x , y) ;
			}
			else if( im->cand_screen && im->preedit.filled_len)
			{
				(*im->cand_screen->show)( im->cand_screen) ;
				(*im->cand_screen->set_spot)( im->cand_screen ,
							      x , y) ;
			}
		}
	}
	else
	{
		if( im->cand_screen)
		{
			(*im->cand_screen->hide)( im->cand_screen) ;
		}

		if( im->stat_screen)
		{
			(*im->stat_screen->hide)( im->stat_screen) ;
		}
	}
}

#else  /* ! USE_IM_PLUGIN */

x_im_t *
x_im_new(
	x_display_t *  disp ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	ml_char_encoding_t  term_encoding ,
	x_im_event_listener_t *  im_listener ,
	char *  input_method ,
	u_int  mod_ignore_mask
	)
{
	return  NULL ;
}

void
x_im_delete(
	x_im_t *  im
	)
{
}

void
x_im_redraw_preedit(
	x_im_t *  im ,
	int  is_focused
	)
{
}

#endif

