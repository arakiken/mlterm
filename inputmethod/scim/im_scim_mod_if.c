/*
 * im_scim_mod_if.c - SCIM plugin for mlterm (part of module interface)
 *
 * Copyright (C) 2005 Seiichi SATO <ssato@sh.rim.or.jp>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of any author may not be used to endorse or promote
 *    products derived from this software without their specific prior
 *    written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 *	$Id$
 */
#include  <kiklib/kik_locale.h>	/* kik_get_locale */
#include  <kiklib/kik_debug.h>	/* kik_error_printf, kik_warn_printf */
#include  <kiklib/kik_str.h>	/* kik_str_alloca_dup */

#include  <x_im.h>
#include  "../im_common.h"
#include  "../im_info.h"

#include  "im_scim.h"

#if  1
#define  IM_SCIM_DEBUG  1
#endif

typedef struct  im_scim
{
	/* input method common object */
	x_im_t  im ;

	im_scim_context_t  context ;

	ml_char_encoding_t  term_encoding ;

	mkf_parser_t *  parser_term ;	/* for term encoding  */
	mkf_conv_t *  conv ;

	x_im_candidate_screen_t *  hidden_cand_screen ;

} im_scim_t ;


/* --- static variables --- */

static int  ref_count = 0 ;
static int  initialized = 0 ;
static x_im_export_syms_t *  syms = NULL ; /* mlterm internal symbols */
static mkf_parser_t *  parser_utf8 = NULL ;
static panel_fd = -1 ;

/* --- static functions --- */

/*
 * methods of x_im_t
 */

static int
delete(
	x_im_t *  im
	)
{
	im_scim_t *  scim ;

	scim = (im_scim_t*) im ;

	im_scim_destroy_context( scim->context) ;

	if( scim->parser_term)
	{
		( *scim->parser_term->delete)( scim->parser_term) ;
	}

	if( scim->conv)
	{
		( *scim->conv->delete)( scim->conv) ;
	}

	ref_count -- ;

	if( ref_count == 0)
	{
		if( panel_fd >= 0)
		{
			(*syms->x_term_manager_remove_fd)( panel_fd) ;
			panel_fd = -1 ;
		}

		im_scim_finalize() ;

		if( parser_utf8)
		{
			(*parser_utf8->delete)( parser_utf8) ;
			parser_utf8 = NULL ;
		}

		initialized = 0 ;
	}

	return  ref_count ;
}

static int
key_event(
	x_im_t *  im ,
	u_char  key_char ,
	KeySym  ksym ,
	XKeyEvent *  event
	)
{
	return  im_scim_key_event( ( (im_scim_t*) im)->context , ksym , event) ;
}

static int
switch_mode(
	x_im_t *  im
	)
{
	return  0 ;
}

static void
focused(
	x_im_t *  im
	)
{
	im_scim_focused( ( (im_scim_t*) im)->context) ;
}

static void
unfocused(
	x_im_t *  im
	)
{
	im_scim_unfocused( ( (im_scim_t*) im)->context) ;
}


/*
 * callbacks (im_scim.cpp --> im_scim_mod_if.c)
 */

static void
commit(
	void *  ptr ,
	char *  utf8_str
	)
{
	im_scim_t *  scim ;
	u_char  conv_buf[256] ;
	size_t  filled_len ;

	scim = (im_scim_t *) ptr ;

	if( scim->term_encoding == ML_UTF8)
	{
		(*scim->im.listener->write_to_term)( scim->im.listener->self ,
						     (u_char*) utf8_str ,
						     strlen( utf8_str)) ;
		goto  skip ;
	}

	(*parser_utf8->init)( parser_utf8) ;
	(*parser_utf8->set_str)( parser_utf8 ,
				 (u_char*) utf8_str ,
				 strlen( utf8_str)) ;

	(*scim->conv->init)( scim->conv) ;

	while( ! parser_utf8->is_eos)
	{
		filled_len = (*scim->conv->convert)( scim->conv ,
						    conv_buf ,
						    sizeof( conv_buf) ,
						    parser_utf8) ;

		if( filled_len == 0)
		{
			/* finished converting */
			break ;
		}

		(*scim->im.listener->write_to_term)( scim->im.listener->self ,
						    conv_buf ,
						    filled_len) ;
	}

skip:
	if( scim->im.cand_screen)
	{
		(*scim->im.cand_screen->delete)( scim->im.cand_screen) ;
		scim->im.cand_screen = NULL ;
	}
}

static void
preedit_update(
	void *  ptr ,
	char *  utf8_str ,
	int  cursor_offset
	)
{
	im_scim_t *  scim ;
	u_char *  str ;
	ml_char_t *  p ;
	mkf_char_t  ch ;
	u_int  count = 0 ;
	u_int  index = 0 ;
	int  saved_segment_offset ;

	scim = (im_scim_t *) ptr ;

	if( scim->im.preedit.chars)
	{
		(*syms->ml_str_delete)( scim->im.preedit.chars ,
					scim->im.preedit.num_of_chars) ;
		scim->im.preedit.chars = NULL ;
	}

	saved_segment_offset = scim->im.preedit.cursor_offset ;

	scim->im.preedit.num_of_chars = 0 ;
	scim->im.preedit.filled_len = 0 ;
	scim->im.preedit.segment_offset = -1 ;
	scim->im.preedit.cursor_offset = X_IM_PREEDIT_NOCURSOR ;

	if( utf8_str == NULL)
	{
		goto  draw ;
	}

	if( ! strlen( utf8_str))
	{
		goto  draw ;
	}

	if( scim->term_encoding != ML_UTF8)
	{
		/* utf8 -> term encoding */
		(*parser_utf8->init)( parser_utf8) ;
		if( ! (im_convert_encoding( parser_utf8 , scim->conv ,
					    (u_char*) utf8_str , &str ,
					    strlen( utf8_str) + 1)))
		{
			return ;
		}
	}
	else
	{
		str = (u_char*) utf8_str ;
	}


	/*
	 * count number of characters to allocate im.preedit.chars
	 */

	(*scim->parser_term->init)( scim->parser_term) ;
	(*scim->parser_term->set_str)( scim->parser_term ,
				      (u_char*) str ,
				      strlen( str)) ;

	while( (*scim->parser_term->next_char)( scim->parser_term , &ch))
	{
		count++ ;
	}

	/*
	 * allocate im.preedit.chars
	 */
	if( ! ( scim->im.preedit.chars = malloc( sizeof(ml_char_t) * count)))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif

		scim->im.preedit.chars = NULL ;
		scim->im.preedit.num_of_chars = 0 ;
		scim->im.preedit.filled_len = 0 ;

		if( scim->term_encoding != ML_UTF8)
		{
			free( str) ;
		}

		return ;
	}

	scim->im.preedit.num_of_chars = count;


	/*
	 * u_char --> ml_char_t
	 */

	p = scim->im.preedit.chars;
	(*syms->ml_str_init)( p , count);

	(*scim->parser_term->init)( scim->parser_term) ;
	(*scim->parser_term->set_str)( scim->parser_term ,
				      (u_char*) str ,
				      strlen( str)) ;

	index = 0 ;

	while( (*scim->parser_term->next_char)( scim->parser_term , &ch))
	{
		ml_color_t  fg_color = ML_FG_COLOR ;
		ml_color_t  bg_color = ML_BG_COLOR ;
		u_int  attr ;
		int  is_biwidth = 0 ;
		int  is_comb = 0 ;
		int  is_underline = 0 ;
		int  is_bold = 0 ;

		if( index == cursor_offset)
		{
			scim->im.preedit.cursor_offset = cursor_offset ;
		}

		if( ch.cs == ISO10646_UCS4_1)
		{
			if( ch.property & MKF_BIWIDTH)
			{
				is_biwidth = 1 ;
			}
			else if( ch.property & MKF_AWIDTH)
			{
				/* TODO: check col_size_of_width_a */
				is_biwidth = 1 ;
			}
		}

		if( ch.property & MKF_COMBINING)
		{
			is_comb = 1 ;
		}

		attr = im_scim_preedit_char_attr( scim->context , index) ;
		if( attr & CHAR_ATTR_UNDERLINE)
		{
			is_underline = 1 ;
		}
		if( attr & CHAR_ATTR_REVERSE)
		{
			if( scim->im.preedit.segment_offset == -1)
			{
				scim->im.preedit.segment_offset = index ;
			}
			fg_color = ML_BG_COLOR ;
			bg_color = ML_FG_COLOR ;
		}
		if( attr & CHAR_ATTR_BOLD)
		{
			is_bold = 1 ;
		}

		if( is_comb)
		{
			if( (*syms->ml_char_combine)( p - 1 , ch.ch ,
						      ch.size , ch.cs ,
						      is_biwidth ,
						      is_comb ,
						      fg_color ,
						      bg_color ,
						      is_bold ,
						      is_underline))
			{
				index ++ ;
				continue;
			}

			/*
			 * if combining failed , char is normally appended.
			 */
		}

		if( (*syms->ml_is_msb_set)( ch.cs))
		{
			SET_MSB( ch.ch[0]) ;
		}

		(*syms->ml_char_set)( p , ch.ch , ch.size , ch.cs ,
				      is_biwidth , is_comb ,
				      fg_color , bg_color ,
				      is_bold , is_underline) ;

		p++ ;
		scim->im.preedit.filled_len++;
		index++ ;
	}

	if( scim->term_encoding != ML_UTF8)
	{
		free( str) ;
	}

	if( scim->im.preedit.filled_len &&
	    scim->im.preedit.cursor_offset == X_IM_PREEDIT_NOCURSOR)
	{
		scim->im.preedit.cursor_offset = scim->im.preedit.filled_len ;
	}

draw:
	(*scim->im.listener->draw_preedit_str)( scim->im.listener->self ,
						scim->im.preedit.chars ,
						scim->im.preedit.filled_len ,
						scim->im.preedit.cursor_offset) ;

	/* Drop the current candidates since the segment is changed */
	if( saved_segment_offset != scim->im.preedit.segment_offset &&
	    scim->im.cand_screen)
	{
		(*scim->im.cand_screen->delete)( scim->im.cand_screen) ;
		scim->im.cand_screen = NULL ;
	}
}

static void
candidate_update(
	void *  ptr ,
	int  is_vertical_lookup ,
	uint  num_of_candiate ,
	char **  str ,
	int  index
	)
{
	im_scim_t *  scim ;
	int  x ;
	int  y ;
	int  i ;

	scim = (im_scim_t *)  ptr ;

	(*scim->im.listener->get_spot)( scim->im.listener->self ,
					scim->im.preedit.chars ,
					scim->im.preedit.segment_offset ,
					&x , &y) ;

	if( scim->im.cand_screen == NULL)
	{
		u_int  line_height ;


		if( ! ( scim->im.cand_screen = (*syms->x_im_candidate_screen_new)(
				(*scim->im.listener->get_win_man)(scim->im.listener->self) ,
				(*scim->im.listener->get_font_man)(scim->im.listener->self) ,
				(*scim->im.listener->get_color_man)(scim->im.listener->self) ,
				(*scim->im.listener->is_vertical)(scim->im.listener->self) ,
				is_vertical_lookup ,
				(*scim->im.listener->get_line_height)(scim->im.listener->self) ,
				x , y)))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " x_im_candidate_screen_new() failed.\n") ;
		#endif
			
			return ;
		}

		scim->im.cand_screen->listener.self = scim ;
		scim->im.cand_screen->listener.selected = NULL ; /* TODO */
	}

	if( ! (*scim->im.cand_screen->init)( scim->im.cand_screen ,
					     num_of_candiate , 10)) /* 16? */
	{
		(*scim->im.cand_screen->delete)( scim->im.cand_screen) ;
		scim->im.cand_screen = NULL ;

		return ;
	}

	(*scim->im.cand_screen->set_spot)( scim->im.cand_screen , x , y) ;

	for( i = 0 ; i < num_of_candiate ; i++)
	{
		u_char *  p = NULL ;

	#ifdef  IM_UIM_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " %d| %s\n", i , _p) ;
	#endif

		if( scim->term_encoding != ML_UTF8)
		{
			(*parser_utf8->init)( parser_utf8) ;
			if( im_convert_encoding( parser_utf8 , scim->conv ,
						 str[i] , &p ,
						 strlen( str[i]) + 1))
			{
				(*scim->im.cand_screen->set)(
							scim->im.cand_screen ,
							scim->parser_term ,
							p , i) ;
				free( p) ;
			}
		}
		else
		{
			(*scim->im.cand_screen->set)( scim->im.cand_screen ,
						     scim->parser_term ,
						     str[i] , i) ;
		}
	}

	(*scim->im.cand_screen->select)( scim->im.cand_screen , index) ;
}

static void
candidate_show(
	void *  ptr
	)
{
	im_scim_t *  scim ;

	scim = (im_scim_t *)  ptr ;

	if( scim->im.cand_screen)
	{
		(*scim->im.cand_screen->show)( scim->im.cand_screen) ;
	}
}

static void
candidate_hide(
	void *  ptr
	)
{
	im_scim_t *  scim ;

	scim = (im_scim_t *)  ptr ;

	if( scim->im.cand_screen)
	{
		(*scim->im.cand_screen->hide)( scim->im.cand_screen) ;
	}
}


static im_scim_callbacks_t callbacks = {
	commit ,
	preedit_update ,
	candidate_update ,
	candidate_show ,
	candidate_hide ,
	NULL
} ;


/*
 * panel
 */

static void
panel_read_handler( void)
{
	if( ! im_scim_receive_panel_event())
	{
		(*syms->x_term_manager_remove_fd)( panel_fd) ;
		panel_fd = -1 ;
	}
}


/* --- module entry point --- */

x_im_t *
im_scim_new(
	u_int64_t  magic ,
	ml_char_encoding_t  term_encoding ,
	x_im_export_syms_t *  export_syms ,
	char *  unused ,
	u_int  mod_ignore_mask
	)
{
	im_scim_t *  scim = NULL ;

	if( magic != (u_int64_t) IM_API_COMPAT_CHECK_MAGIC)
	{
		kik_error_printf( "Incompatible input method API.\n") ;

		return  NULL ;
	}

#if 1
#define  RESTORE_LOCALE
#endif

	if( ! initialized)
	{
		char *  cur_locale ;

	#ifdef  RESTORE_LOCALE
		/*
		 * Workaround against make_locale() of m17nlib.
		 */
		cur_locale = kik_str_alloca_dup( kik_get_locale()) ;
	#else
		cur_locale = kik_get_locale() ;
	#endif
		if( ! im_scim_initialize( cur_locale))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " failed to initialize SCIM.") ;
		#endif

			return  NULL ;
		}
	#ifdef  RESTORE_LOCALE
		/* restoring */
		/*
		 * TODO: remove valgrind warning.
		 * The memory space pointed to by sys_locale in kik_locale.c
		 * was freed by setlocale() in m17nlib.
		 */
		kik_locale_init( cur_locale) ;
	#endif

		syms = export_syms ;

		if( ( panel_fd = im_scim_get_panel_fd()) >= 0)
		{
			(*syms->x_term_manager_add_fd)( panel_fd ,
							panel_read_handler) ;
		}

		if( ! ( parser_utf8 = (*syms->ml_parser_new)( ML_UTF8)))
		{
			goto  error ;
		}

		initialized = 1 ;
	}

	if( ! ( scim = malloc( sizeof( im_scim_t))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif

		goto  error ;
	}

	scim->context = NULL ;
	scim->term_encoding = term_encoding ;
	scim->conv = NULL ;
	scim->hidden_cand_screen = NULL ;

	if( scim->term_encoding != ML_UTF8)
	{
		if( ! ( scim->conv = (*syms->ml_conv_new)( term_encoding)))
		{
			goto  error ;
		}
	}

	if( ! ( scim->parser_term = (*syms->ml_parser_new)( term_encoding)))
	{
		goto  error ;
	}

	if( ! ( scim->context = im_scim_create_context( scim , &callbacks)))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " im_scim_create_context failed.\n") ;
	#endif

		goto  error ;
	}

	/*
	 * set methods of x_im_t
	 */
	scim->im.delete = delete ;
	scim->im.key_event = key_event ;
	scim->im.switch_mode = switch_mode ;
	scim->im.focused = focused ;
	scim->im.unfocused = unfocused ;

	ref_count ++ ;

	return  (x_im_t *) scim ;

error:

	if( scim)
	{
		if( scim->context)
		{
			im_scim_destroy_context( scim->context) ;
		}

		if( scim->conv)
		{
			(*scim->conv->delete)( scim->conv) ;
		}

		if( scim->parser_term)
		{
			(*scim->parser_term->delete)( scim->parser_term) ;
		}

		free( scim) ;
	}

	if( ref_count == 0)
	{
		if( panel_fd >= 0)
		{
			(*syms->x_term_manager_remove_fd)( panel_fd) ;
			panel_fd = -1 ;
		}

		im_scim_finalize() ;

		if( parser_utf8)
		{
			(*parser_utf8->delete)( parser_utf8) ;
			parser_utf8 = NULL ;
		}

	}

	return  NULL ;
}


/* --- module entry point for external tools --- */

im_info_t *
im_scim_get_info(
	char *  locale ,
	char *  encoding
	)
{
	im_info_t *  result ;

	if( ! ( result = malloc( sizeof( im_info_t))))
	{
		return  NULL ;
	}

	result->id = strdup( "scim") ;
	result->name = strdup( "SCIM") ;
	result->num_of_args = 0;
	result->args = NULL ;
	result->readable_args = NULL ;

	return  result;
}

