/*
 * im_uim.c - uim plugin for mlterm
 *
 * Copyright (C) 2004 Seiichi SATO <ssato@sh.rim.or.jp>
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

#include  <stdio.h>
#include  <uim.h>
#include  <uim-helper.h>
#include  <uim-im-switcher.h>

#include  <kiklib/kik_mem.h>	/* malloc/alloca/free */
#include  <kiklib/kik_str.h>	/* kik_str_alloca_dup kik_str_sep kik_snprintf*/
#include  <kiklib/kik_locale.h>	/* kik_get_locale */
#include  <kiklib/kik_slist.h>

#include  <x_im.h>

#include  "../im_common.h"
#include  "../im_info.h"

#if  0
#define  IM_UIM_DEBUG  1
#endif

#if  0
/* Experimental (for twm, framebuffer etc where it's impossible to show status on system tray.) */
#define  SHOW_STATUS_SCREEN
#endif


/*
 * When uim encoding is the same as terminal, parser_uim and conv are NULL,
 * so encoding of received string will not be converted.
 */
#define  NEED_TO_CONV(uim)  ((uim)->parser_uim && (uim)->conv)

typedef struct im_uim
{
	/* input method common object */
	x_im_t  im ;

	uim_context  context ;

	ml_char_encoding_t  term_encoding ;

	char *  encoding_name ;		/* encoding of conversion engine */

	/* parser_uim and conv are NULL if term_encoding == uim encoding  */
	mkf_parser_t *  parser_uim ;	/* for uim encoding  */
	mkf_parser_t *  parser_term ;	/* for term encoding */
	mkf_conv_t *  conv ;		/* for term encoding */

	u_int  pressing_mod_key ;
	u_int  mod_ignore_mask ;

	u_int  cand_limit ;

	int  is_mozc ;

#ifdef  SHOW_STATUS_SCREEN
	int  mode ;
#endif

	struct im_uim *  next ;

}  im_uim_t ;


/* --- static variables --- */

static im_uim_t *  uim_list = NULL ;
static int  ref_count = 0 ;
static int  initialized = 0 ;
static int  helper_fd = -1 ;
static im_uim_t *  focused_uim = NULL ;
static x_im_export_syms_t *  syms = NULL ; /* mlterm internal symbols */
static int  mod_key_debug = 0 ;


/* --- static functions --- */

#ifdef  SHOW_STATUS_SCREEN
static void
update_stat_screen(
	im_uim_t *  uim ,
	int  mode_changed
	)
{
	if( ! uim->im.cand_screen)
	{
		if( uim->mode == 0 /* direct input */ /* || uim->im.preedit.filled_len == 0 */)
		{
			if( uim->im.stat_screen)
			{
				(*uim->im.stat_screen->delete)( uim->im.stat_screen) ;
				uim->im.stat_screen = NULL ;
			}

			return ;
		}

		if( ! uim->im.stat_screen)
		{
			int  x ;
			int  y ;

			(*uim->im.listener->get_spot)( uim->im.listener->self ,
						NULL , 0 , &x , &y) ;

			if( ! ( uim->im.stat_screen = (*syms->x_im_status_screen_new)(
							uim->im.disp , uim->im.font_man ,
							uim->im.color_man ,
							(*uim->im.listener->is_vertical)(
								uim->im.listener->self) ,
							(*uim->im.listener->get_line_height)(
								uim->im.listener->self) ,
							x , y)))
			{
				return ;
			}
		}
		else if( ! mode_changed)
		{
			return ;
		}

		(*uim->im.stat_screen->set)( uim->im.stat_screen ,
			uim->parser_uim ,
			(u_char*)uim_get_mode_name( uim->context , uim->mode)) ;
	}
}
#else
#define  update_stat_screen( uim , mode_changed)  (0)
#endif

static int
find_engine(
	const char *  engine ,
	char **  encoding_name
	)
{
	uim_context  u ;
	int  i ;
	int  result ;
	
	if( encoding_name == NULL)
	{
		return  0 ;
	}

	if( ! ( u = uim_create_context( NULL , "UTF-8" , NULL , NULL ,
					NULL , NULL)))
	{
		return  0 ;
	}
	result = 0 ;

	for( i = 0 ; i < uim_get_nr_im( u) ; i++)
	{
		if( strcmp( uim_get_im_name( u , i) , engine) == 0)
		{
			*encoding_name = (char*) uim_get_im_encoding( u , i) ;

			if( *encoding_name && (*encoding_name = strdup(*encoding_name)))
			{
			#ifdef  IM_UIM_DEBUG
				kik_debug_printf( KIK_DEBUG_TAG "conversion engine: %s, native encoding: %s\n" , engine , *encoding_name);
			#endif

				result = 1 ;

				break ;
			}
		}
	}

	uim_release_context( u) ;

	return  result ;
}

static int
xksym_to_ukey(
	KeySym ksym
	)
{
	/* Latin 1 */
	if( 0x20 <= ksym && ksym <= 0x7e)
	{
		return  ksym ;
	}

	switch( ksym)
	{
	/* TTY Functions */
	case  XK_BackSpace:
		return  UKey_Backspace ;
	case  XK_Tab:
		return  UKey_Tab ;
	case  XK_Return:
		return  UKey_Return ;
	case  XK_Escape:
		return  UKey_Escape ;
	case  XK_Delete:
		return  UKey_Delete ;
#ifdef  XK_Multi_key
	/* International & multi-key character composition */
	case  XK_Multi_key:
		return  UKey_Multi_key ;
#endif
	/* Japanese keyboard support */
	case  XK_Muhenkan:
		return  UKey_Muhenkan ;
	case  XK_Henkan_Mode:
		return  UKey_Henkan_Mode ;
	case  XK_Zenkaku_Hankaku:
		return  UKey_Zenkaku_Hankaku ;
	case  XK_Hiragana_Katakana:
		return  UKey_Hiragana_Katakana ;
	/* Cursor control & motion */
	case  XK_Home:
		return  UKey_Home ;
	case  XK_Left:
		return  UKey_Left ;
	case  XK_Up:
		return  UKey_Up ;
	case  XK_Right:
		return  UKey_Right ;
	case  XK_Down:
		return  UKey_Down ;
	case  XK_Prior:
		return  UKey_Prior ;
	case  XK_Next:
		return  UKey_Next ;
	case  XK_End:
		return  UKey_End ;
	case  XK_F1:
		return  UKey_F1 ;
	case  XK_F2:
		return  UKey_F2 ;
	case  XK_F3:
		return  UKey_F3 ;
	case  XK_F4:
		return  UKey_F4 ;
	case  XK_F5:
		return  UKey_F5 ;
	case  XK_F6:
		return  UKey_F6 ;
	case  XK_F7:
		return  UKey_F7 ;
	case  XK_F8:
		return  UKey_F8 ;
	case  XK_F9:
		return  UKey_F9 ;
	case  XK_F10:
		return  UKey_F10 ;
	case  XK_F11:
		return  UKey_F11 ;
	case  XK_F12:
		return  UKey_F12 ;
	case  XK_F13:
		return  UKey_F13 ;
	case  XK_F14:
		return  UKey_F14 ;
	case  XK_F15:
		return  UKey_F15 ;
	case  XK_F16:
		return  UKey_F16 ;
	case  XK_F17:
		return  UKey_F17 ;
	case  XK_F18:
		return  UKey_F18 ;
	case  XK_F19:
		return  UKey_F19 ;
	case  XK_F20:
		return  UKey_F20 ;
	case  XK_F21:
		return  UKey_F21 ;
	case  XK_F22:
		return  UKey_F22 ;
	case  XK_F23:
		return  UKey_F23 ;
	case  XK_F24:
		return  UKey_F24 ;
#ifdef  XK_F25
	case  XK_F25:
		return  UKey_F25 ;
	case  XK_F26:
		return  UKey_F26 ;
	case  XK_F27:
		return  UKey_F27 ;
	case  XK_F28:
		return  UKey_F28 ;
	case  XK_F29:
		return  UKey_F29 ;
	case  XK_F30:
		return  UKey_F30 ;
	case  XK_F31:
		return  UKey_F31 ;
	case  XK_F32:
		return  UKey_F32 ;
	case  XK_F33:
		return  UKey_F33 ;
	case  XK_F34:
		return  UKey_F34 ;
	case  XK_F35:
		return  UKey_F35 ;
#endif
#ifdef  XK_KP_Space
	case  XK_KP_Space:
		return  ' ' ;
#endif
#ifdef  XK_KP_Tab
	case  XK_KP_Tab:
		return  UKey_Tab ;
#endif
#ifdef  XK_KP_Enter
	case  XK_KP_Enter:
		return  UKey_Return ;
#endif
	case  XK_KP_F1:
		return  UKey_F1 ;
	case  XK_KP_F2:
		return  UKey_F2 ;
	case  XK_KP_F3:
		return  UKey_F3 ;
	case  XK_KP_F4:
		return  UKey_F4 ;
	case  XK_KP_Home:
		return  UKey_Home ;
	case  XK_KP_Left:
		return  UKey_Left ;
	case  XK_KP_Up:
		return  UKey_Up ;
	case  XK_KP_Right:
		return  UKey_Right ;
	case  XK_KP_Down:
		return  UKey_Down ;
	case  XK_KP_Prior:
		return  UKey_Prior ;
	case  XK_KP_Next:
		return  UKey_Next ;
	case  XK_KP_End:
		return  UKey_End ;
	case  XK_KP_Delete:
		return  UKey_Delete ;
#ifdef  XK_KP_Equal
	case  XK_KP_Equal:
		return  '=' ;
#endif
	case  XK_KP_Multiply:
		return  '*' ;
	case  XK_KP_Add:
		return  '+' ;
	case  XK_KP_Separator:
		return  ',' ;
	case  XK_KP_Subtract:
		return  '-' ;
	case  XK_KP_Decimal:
		return  '.' ;
	case  XK_KP_Divide:
		return  '/' ;
	/* keypad numbers */
	case  XK_KP_0:
		return  '0' ;
	case  XK_KP_1:
		return  '1' ;
	case  XK_KP_2:
		return  '2' ;
	case  XK_KP_3:
		return  '3' ;
	case  XK_KP_4:
		return  '4' ;
	case  XK_KP_5:
		return  '5' ;
	case  XK_KP_6:
		return  '6' ;
	case  XK_KP_7:
		return  '7' ;
	case  XK_KP_8:
		return  '8' ;
	case  XK_KP_9:
		return  '9' ;
	default:
		return  UKey_Other ;
	}
}

static void
helper_disconnected( void)
{
	(*syms->x_event_source_remove_fd)( helper_fd) ;

	helper_fd = -1 ;
}

static void
prop_list_update(
	void *  p ,
	const char *  str
	)
{
	im_uim_t *  uim = NULL ;
	char  buf[BUFSIZ] ;
	int  len ;

#ifdef  IM_UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " str: %s\n", str);
#endif

	uim = (im_uim_t*) p ;

	if( focused_uim != uim)
	{
		return ;
	}

#define  PROP_LIST_FORMAT  "prop_list_update\ncharset=%s\n%s"

	len = strlen( PROP_LIST_FORMAT) + strlen( uim->encoding_name) +
	      strlen( str) + 1 ;

	if( len > sizeof( buf))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " property list string is too long.");
	#endif
		return ;
	}

	kik_snprintf( buf , sizeof( buf) , PROP_LIST_FORMAT ,
		      uim->encoding_name , str) ;

	uim_helper_send_message( helper_fd , buf) ;

#undef  PROP_LIST_FORMAT
}

static void
prop_label_update(
	void *  p ,
	const char *  str
	)
{
	im_uim_t *  uim = NULL ;
	char  buf[BUFSIZ] ;
	int  len ;

#ifdef  IM_UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " prop_label_update(), str: %s\n", str);
#endif

	uim = (im_uim_t*) p ;

	if( focused_uim != uim)
	{
		return ;
	}

#define  PROP_LABEL_FORMAT "prop_label_update\ncharset=%s\n%s"

	len = strlen(PROP_LABEL_FORMAT) + strlen( uim->encoding_name) +
	      strlen( str) + 1 ;

	if( len > sizeof( buf))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " property label string is too long.");
	#endif

		return ;
	}

	kik_snprintf( buf , sizeof(buf) , PROP_LABEL_FORMAT ,
		      uim->encoding_name , str) ;

	uim_helper_send_message( helper_fd , buf) ;

#undef  PROP_LABEL_FORMAT
}

#ifdef  SHOW_STATUS_SCREEN
static void
mode_update(
	void *  p ,
	int  mode
	)
{
	im_uim_t *  uim = NULL ;

	uim = (im_uim_t*) p ;

	uim->mode = mode ;

	update_stat_screen( uim , 1) ;
}
#endif

static void
commit(
	void *  p ,
	const char *  str
	)
{
	im_uim_t *  uim ;
	u_char  conv_buf[256] ;
	size_t  filled_len ;
	size_t  len ;

#ifdef  IM_UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "str: %s\n", str);
#endif

	uim = (im_uim_t*) p ;

	len = strlen( str) ;

	if( ! NEED_TO_CONV(uim))
	{
		(*uim->im.listener->write_to_term)( uim->im.listener->self ,
						    (u_char*)str , len) ;

		return ;
	}

	(*uim->parser_uim->init)( uim->parser_uim) ;
	(*uim->parser_uim->set_str)( uim->parser_uim , (u_char*)str , len) ;

	(*uim->conv->init)( uim->conv) ;

	while( ! uim->parser_uim->is_eos)
	{
		filled_len = (*uim->conv->convert)( uim->conv , conv_buf ,
						    sizeof( conv_buf) ,
						    uim->parser_uim) ;

		if( filled_len == 0)
		{
			/* finished converting */
			break ;
		}

		(*uim->im.listener->write_to_term)( uim->im.listener->self ,
						    conv_buf , filled_len) ;
	}
}

static void
preedit_clear(
	void *  ptr
	)
{
	im_uim_t *  uim ;

#ifdef  IM_UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	uim = (im_uim_t*) ptr ;

	if( uim->im.preedit.chars)
	{
		(*syms->ml_str_delete)( uim->im.preedit.chars ,
					uim->im.preedit.num_of_chars) ;
		uim->im.preedit.chars = NULL ;
	}

	uim->im.preedit.num_of_chars = 0 ;
	uim->im.preedit.filled_len = 0 ;
	uim->im.preedit.segment_offset = 0 ;
	uim->im.preedit.cursor_offset = X_IM_PREEDIT_NOCURSOR ;

	update_stat_screen( uim , 0) ;
}

static void
preedit_pushback(
	void *  ptr ,
	int  attr ,
	const char *  _str
	)
{
	im_uim_t *  uim ;
	u_char *  str ;
	ml_char_t *  p ;
	mkf_char_t  ch ;
	ml_color_t  fg_color = ML_FG_COLOR ;
	ml_color_t  bg_color = ML_BG_COLOR ;
	int  is_underline = 0 ;
	u_int  count = 0 ;
	size_t  len ;

#ifdef  IM_UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " attr: %d, _str:%s, length:%d\n" , attr , _str, strlen( _str)) ;
#endif

	uim = (im_uim_t*) ptr ;

	if( attr & UPreeditAttr_Cursor)
	{
		uim->im.preedit.cursor_offset = uim->im.preedit.filled_len ;
	}

	if( ! ( len = strlen( _str)))
	{
		return ;
	}

	if( attr & UPreeditAttr_Reverse)
	{
		uim->im.preedit.segment_offset = uim->im.preedit.filled_len ;
		uim->im.preedit.cursor_offset = X_IM_PREEDIT_NOCURSOR ;
		fg_color = ML_BG_COLOR ;
		bg_color = ML_FG_COLOR ;
	}

	if( attr & UPreeditAttr_UnderLine)
	{
		is_underline = 1 ;
	}

	/* TODO: UPreeditAttr_Separator */

	if( NEED_TO_CONV(uim))
	{
		/* uim encoding -> term encoding */
		(*uim->parser_uim->init)( uim->parser_uim) ;
		if( ! im_convert_encoding( uim->parser_uim , uim->conv ,
					    (u_char*)_str , &str , len + 1))
		{
			return ;
		}
	}
	else
	{
		str = (u_char*) _str ;
	}

	len = strlen( str) ;

	/*
	 * count number of characters to re-allocate im.preedit.chars
	 */

	(*uim->parser_term->init)( uim->parser_term) ;
	(*uim->parser_term->set_str)( uim->parser_term , (u_char*) str , len) ;

	while( (*uim->parser_term->next_char)( uim->parser_term , &ch))
	{
		count++ ;
	}

	/* no space left? (current array size < new array size?)*/
	if( uim->im.preedit.num_of_chars < uim->im.preedit.filled_len + count)
	{
		if( ! ( p = realloc( uim->im.preedit.chars ,
				     sizeof(ml_char_t) * ( uim->im.preedit.filled_len + count))))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " realloc failed.\n") ;
		#endif

			(*syms->ml_str_delete)( uim->im.preedit.chars ,
						uim->im.preedit.num_of_chars) ;
			uim->im.preedit.chars = NULL ;
			uim->im.preedit.num_of_chars = 0 ;
			uim->im.preedit.filled_len = 0 ;

			if( NEED_TO_CONV(uim))
			{
				free( str) ;
			}

			return ;
		}

		uim->im.preedit.chars = p ;
		uim->im.preedit.num_of_chars = uim->im.preedit.filled_len + count;
	}

	/*
	 * u_char --> ml_char_t
	 */

	p = &uim->im.preedit.chars[uim->im.preedit.filled_len];
	(*syms->ml_str_init)( p , count);

	(*uim->parser_term->init)( uim->parser_term) ;
	(*uim->parser_term->set_str)( uim->parser_term , (u_char*)str , len) ;

	count = 0 ;

	while( (*uim->parser_term->next_char)( uim->parser_term , &ch))
	{
		int  is_fullwidth = 0 ;
		int  is_comb = 0 ;

		if( (*syms->ml_convert_to_internal_ch)( &ch ,
			(*uim->im.listener->get_unicode_policy)(uim->im.listener->self) ,
			US_ASCII) <= 0)
		{
			continue ;
		}

		if( ch.property & MKF_FULLWIDTH)
		{
			is_fullwidth = 1 ;
		}
		else if( ch.property & MKF_AWIDTH)
		{
			/* TODO: check col_size_of_width_a */
			is_fullwidth = 1 ;
		}

		if( ch.property & MKF_COMBINING)
		{
			is_comb = 1 ;

			if( (*syms->ml_char_combine)( p - 1 , mkf_char_to_int(&ch) ,
						ch.cs , is_fullwidth , is_comb ,
						fg_color , bg_color ,
						0 , 0 , is_underline , 0 , 0))
			{
				continue ;
			}

			/*
			 * if combining failed , char is normally appended.
			 */
		}

		(*syms->ml_char_set)( p , mkf_char_to_int(&ch) , ch.cs ,
				      is_fullwidth , is_comb ,
				      fg_color , bg_color ,
				      0 , 0 , is_underline , 0 , 0) ;

		p++ ;
		uim->im.preedit.filled_len++;
	}

	if( NEED_TO_CONV( uim))
	{
		free( str) ;
	}
}

static void
preedit_update(
	void *  ptr
	)
{
	im_uim_t *  uim ;

	uim = (im_uim_t*) ptr ;

	(*uim->im.listener->draw_preedit_str)( uim->im.listener->self ,
					       uim->im.preedit.chars ,
					       uim->im.preedit.filled_len ,
					       uim->im.preedit.cursor_offset) ;
	update_stat_screen( uim , 0) ;
}


/*
 * callback for candidate screen events
 */

static void
candidate_selected(
	void *  p ,
	u_int  index
	)
{
#ifdef  IM_UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " index : %d\n" , index) ;
#endif
	uim_set_candidate_index( ((im_uim_t*)p)->context , index) ;
}


/*
 * callbacks for candidate selector
 */

static void
candidate_activate(
	void *  p ,
	int  num ,
	int  limit
	)
{
	im_uim_t *  uim ;
	int  x ;
	int  y ;
	int  i ;

#ifdef  IM_UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " num: %d limit: %d\n", num , limit) ;
#endif

	uim = (im_uim_t*) p ;

	(*uim->im.listener->get_spot)( uim->im.listener->self ,
				       uim->im.preedit.chars ,
				       uim->im.preedit.segment_offset ,
				       &x , &y) ;

	if( uim->im.stat_screen)
	{
		(*uim->im.stat_screen->delete)( uim->im.stat_screen) ;
		uim->im.stat_screen = NULL ;
	}

	if( uim->im.cand_screen == NULL)
	{
		if( ! ( uim->im.cand_screen = (*syms->x_im_candidate_screen_new)(
				uim->im.disp , uim->im.font_man , uim->im.color_man ,
				(*uim->im.listener->is_vertical)(uim->im.listener->self) ,
				1 ,
				(*uim->im.listener->get_unicode_policy)(uim->im.listener->self) ,
				(*uim->im.listener->get_line_height)(uim->im.listener->self) ,
				x , y)))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " x_im_candidate_screen_new() failed.\n") ;
		#endif
			
			return ;
		}

		uim->im.cand_screen->listener.self = uim ;
		uim->im.cand_screen->listener.selected = candidate_selected ;
	}

	if( ! (*uim->im.cand_screen->init)( uim->im.cand_screen , num , limit))
	{
		(*uim->im.cand_screen->delete)( uim->im.cand_screen) ;
		uim->im.cand_screen = NULL ;

		return ;
	}

	(*uim->im.cand_screen->set_spot)( uim->im.cand_screen , x , y) ;

	for( i = 0 ; i < num ; i++)
	{
		uim_candidate  c ;
		u_char *  _p ;
		u_char *  p = NULL ;
		const char *  heading ;
		u_int info ;

		c = uim_get_candidate( uim->context , i , i) ;
		_p = (u_char*)uim_candidate_get_cand_str( c) ;
		heading = uim_candidate_get_heading_label( c) ;
		if( heading && heading[0])
		{
			/* heading[1] may be '\0' */
			info = (((u_int)heading[0]) << 16) | (((u_int)heading[1]) << 24) | i;
		}
		else
		{
			info = (((u_int)' ') << 16) | i;
		}

	#ifdef  IM_UIM_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " %d%s%s%s| %s\n" , i ,
			(heading && heading[0]) ? "(" : "" ,
			(heading && heading[0]) ? heading : "" ,
			(heading && heading[0]) ? ")" : "" , _p) ;
	#endif

		if( NEED_TO_CONV( uim))
		{
			(*uim->parser_uim->init)( uim->parser_uim) ;
			if( im_convert_encoding( uim->parser_uim , uim->conv ,
						 _p , &p , strlen( _p) + 1))
			{
				(*uim->im.cand_screen->set)( uim->im.cand_screen ,
							     uim->parser_term ,
							     p , info) ;
				free( p) ;
			}
		}
		else
		{
			(*uim->im.cand_screen->set)( uim->im.cand_screen ,
						     uim->parser_term ,
						     _p , i) ;
		}

		uim_candidate_free( c) ;
	}

	(*uim->im.cand_screen->select)( uim->im.cand_screen , 0) ;
	uim->cand_limit = limit ;
}

static void
candidate_select(
	void *  p ,
	int  index
	)
{
	im_uim_t *  uim ;

#ifdef  IM_UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " index: %d\n", index) ;
#endif

	uim = (im_uim_t*) p ;

	if( uim->im.cand_screen)
	{
		/*
		 * XXX Hack for uim-mozc (1.11.1522.102)
		 * If candidate_activate() is called with num == 20 and limit = 10,
		 * uim_get_candidate() on mozc doesn't returns 20 candidates but 10 ones.
		 * (e.g. uim_get_candidate(0) and uim_get_candidate(10) returns the same.)
		 */
		if( uim->is_mozc &&
		    uim->im.cand_screen->index != index &&
		    uim->im.cand_screen->index / uim->cand_limit != index / uim->cand_limit &&
		    (index % uim->cand_limit) == 0)
		{
			candidate_activate( p ,
				uim->im.cand_screen->num_of_candidates ,
				uim->cand_limit) ;
		}

		(*uim->im.cand_screen->select)( uim->im.cand_screen , index) ;
	}
}

static void
candidate_shift_page(
	void *  p ,
	int  direction
	)
{
	im_uim_t *  uim ;
	int index ;

#ifdef  IM_UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " direction: %s\n", direction ? "next" : "prev") ;
#endif

	uim = (im_uim_t*) p ;

	if( ! uim->im.cand_screen)
	{
		return ;
	}

	index = (int) uim->im.cand_screen->index ;

	if( ! direction && index < uim->cand_limit)
	{
		/* top page -> last page */
		index = (uim->im.cand_screen->num_of_candidates / uim->cand_limit) * uim->cand_limit + index ;
	}
	else if( direction &&
		 ((index / uim->cand_limit) + 1) * uim->cand_limit > uim->im.cand_screen->num_of_candidates)
	{
		/* last page -> top page */
		index = index % uim->cand_limit ;
	}
	else
	{
		/* shift page according to the direction */
		index += (direction ? uim->cand_limit : -(uim->cand_limit)) ;
	}

	if( index < 0)
	{
		index = 0 ;
	}
	else if( index >= uim->im.cand_screen->num_of_candidates)
	{
		index = uim->im.cand_screen->num_of_candidates - 1 ;
	}

	(*uim->im.cand_screen->select)( uim->im.cand_screen , index) ;
	uim_set_candidate_index( uim->context , index) ;
}

static void
candidate_deactivate(
	void *  p
	)
{
	im_uim_t *  uim ;

#ifdef  IM_UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	uim = (im_uim_t*) p ;

	if( uim->im.cand_screen)
	{
		(*uim->im.cand_screen->delete)( uim->im.cand_screen) ;
		uim->im.cand_screen = NULL ;
	}
}

/*
 * methods of x_im_t
 */

static int
delete(
	x_im_t *  im
	)
{
	im_uim_t *  uim ;

	uim = (im_uim_t*) im ;

	if( focused_uim == uim)
	{
		focused_uim = NULL ;
	}

	if( uim->parser_uim)
	{
		(*uim->parser_uim->delete)( uim->parser_uim) ;
	}

	(*uim->parser_term->delete)( uim->parser_term) ;

	if( uim->conv)
	{
		(*uim->conv->delete)( uim->conv) ;
	}

	uim_release_context( uim->context) ;

	ref_count -- ;

#ifdef  IM_UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " An object was deleted. ref_count: %d\n", ref_count) ;
#endif

	kik_slist_remove( uim_list , uim) ;

	free( uim->encoding_name);
	free( uim) ;

	if( ref_count == 0 && initialized)
	{
		(*syms->x_event_source_remove_fd)( helper_fd) ;
		uim_helper_close_client_fd( helper_fd) ;
		helper_fd = -1 ;

		uim_quit() ;

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
	im_uim_t *  uim ;
	int  key = 0 ;
	int  state = 0 ;
	int  ret ;

	int  is_shift ;
	int  is_lock ;
	int  is_ctl ;
	int  is_alt ;
	int  is_meta ;
	int  is_super ;
	int  is_hyper ;

	uim = (im_uim_t*) im ;

	if( mod_key_debug)
	{
		kik_msg_printf( ">>--------------------------------\n") ;
		kik_msg_printf( ">>event->state    : %.8x\n" , event->state) ;
		kik_msg_printf( ">>mod_ignore_mask : %.8x\n" , uim->mod_ignore_mask) ;
		kik_msg_printf( ">>ksym            : %.8x\n" , ksym) ;
	}

#if  defined(USE_FRAMEBUFFER) || defined(USE_CONSOLE)
	uim->pressing_mod_key = ~0 ;
#else
	if( ! ( event->state & uim->mod_ignore_mask))
	{
		uim->pressing_mod_key = 0 ;
	}

	switch( ksym)
	{
	case XK_Shift_L:
	case XK_Shift_R:
		uim->pressing_mod_key |= UMod_Shift ;
		break ;
	case XK_Control_L:
	case XK_Control_R:
		uim->pressing_mod_key |= UMod_Control ;
		break ;
	case XK_Alt_L:
	case XK_Alt_R:
		uim->pressing_mod_key |= UMod_Alt ;
		break ;
	case XK_Meta_L:
	case XK_Meta_R:
		uim->pressing_mod_key |= UMod_Meta ;
		break ;
	case XK_Super_L:
	case XK_Super_R:
		uim->pressing_mod_key |= UMod_Super ;
		break ;
	case XK_Hyper_L:
	case XK_Hyper_R:
		uim->pressing_mod_key |= UMod_Hyper ;
		break ;
	default:
		break ;
	}
#endif

	(*uim->im.listener->compare_key_state_with_modmap)(
							uim->im.listener->self ,
							event->state ,
							&is_shift , &is_lock ,
							&is_ctl , &is_alt ,
							&is_meta , NULL , &is_super ,
							&is_hyper) ;

	if( is_shift && (uim->pressing_mod_key & UMod_Shift))
	{
		state |= UMod_Shift ;
	}
	if( is_ctl && (uim->pressing_mod_key & UMod_Control))
	{
		state |= UMod_Control ;
	}
	if( is_alt && (uim->pressing_mod_key & UMod_Alt))
	{
		state |= UMod_Alt ;
	}
	if( is_meta && (uim->pressing_mod_key & UMod_Meta))
	{
		state |= UMod_Meta ;
	}
	if( is_super && (uim->pressing_mod_key & UMod_Super))
	{
		state |= UMod_Super ;
	}
	if( is_hyper && (uim->pressing_mod_key & UMod_Hyper))
	{
		state |= UMod_Hyper ;
	}

	if( mod_key_debug)
	{
		kik_msg_printf( ">>pressing_mod_key: %.8x\n" , uim->pressing_mod_key) ;
		kik_msg_printf( ">>state           : %.8x\n" , state) ;
		kik_msg_printf( ">>--------------------------------\n") ;
	}

	key = xksym_to_ukey(ksym) ;

	ret = uim_press_key( uim->context , key , state) ;
	uim_release_key( uim->context , key , state) ;

	return  ret;
}

static int
switch_mode(
	x_im_t *  im
	)
{
	return  0 ;
}

static int
is_active(
	x_im_t *  im
	)
{
#ifdef  SHOW_STATUS_SCREEN
	if( ((im_uim_t*)im)->mode > 0)
	{
		return  1 ;
	}
	else
#endif
	{
		return  0 ;
	}
}

static void
focused(
	x_im_t *  im
	)
{
	im_uim_t *  uim ;

	uim =  (im_uim_t*)  im ;

	uim_helper_client_focus_in( uim->context) ;

	focused_uim = uim ;

	uim_prop_list_update( uim->context) ;
	uim_prop_label_update( uim->context) ;

	if( uim->im.cand_screen)
	{
		(*uim->im.cand_screen->show)( uim->im.cand_screen) ;
	}

	update_stat_screen( uim , 0) ;

	uim->pressing_mod_key = 0 ;
}

static void
unfocused(
	x_im_t *  im
	)
{
	im_uim_t *  uim ;

	uim = (im_uim_t*)  im ;

	uim_helper_client_focus_out( uim->context) ;

	if( uim->im.cand_screen)
	{
		(*uim->im.cand_screen->hide)( uim->im.cand_screen) ;
	}

#ifdef  SHOW_STATUS_SCREEN
	if( uim->im.stat_screen)
	{
		(*uim->im.stat_screen->hide)( uim->im.stat_screen) ;
	}
#endif

	uim->pressing_mod_key = 0 ;
}


/*
 * helper
 */

static void
helper_send_imlist(void)
{
	const char *  selected_name ;
	const char *  name ;
	const char *  lang ;
	const char *  dsc ;
	char *  buf = NULL ;
	int  i ;
	u_int  len = 0 ;
	u_int  filled_len = 0 ;

	if( ! focused_uim)
	{
		return ;
	}

#define  HEADER_FORMAT  "im_list\ncharset=%s\n"

	len += strlen( HEADER_FORMAT) + strlen( focused_uim->encoding_name) ;

	selected_name = uim_get_current_im_name( focused_uim->context) ;
	len += strlen( selected_name) ;
	len += strlen( "selected") ;

	for( i = 0 ; i < uim_get_nr_im( focused_uim->context) ; i++)
	{
		name = uim_get_im_name( focused_uim->context , i) ;
		lang = uim_get_im_language( focused_uim->context , i) ;
		dsc = uim_get_im_short_desc( focused_uim->context , i) ;

		len += name ? strlen( name) : 0 ;
		len += lang ? strlen( lang) : 0 ;
		len += dsc ? strlen( dsc) : 0 ;
		len += strlen( "\t\t\t\n") ;
	}

	len++ ;

	if( ! (buf = alloca( sizeof(char) * len )))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca failed\n") ;
	#endif
		return ;
	}

	filled_len = kik_snprintf( buf , len , HEADER_FORMAT ,
				   focused_uim->encoding_name) ;

#undef  HEADER_FORMAT

	for( i = 0 ; i < uim_get_nr_im( focused_uim->context) ; i++)
	{
		name = uim_get_im_name( focused_uim->context , i) ;
		lang = uim_get_im_language( focused_uim->context , i) ;
		dsc = uim_get_im_short_desc( focused_uim->context , i) ;

		filled_len += kik_snprintf( &buf[filled_len] ,
					    len - filled_len ,
					    "%s\t%s\t%s\t%s\n" ,
					    name ? name : "" ,
					    lang ? lang : "" ,
					    dsc ? dsc : "" ,
					    strcmp( name , selected_name) == 0 ?  "selected" : "") ;
	}

#ifdef  IM_UIM_DEBUG
	kik_debug_printf( "----\n%s----\n" , buf) ;
#endif

	uim_helper_send_message( helper_fd , buf) ;
}

static void
helper_im_changed(
	char *  request ,
	char *  engine_name
	)
{
	char *  buf ;
	size_t  len ;

	len = strlen(engine_name) + 5 ;

	if( ! ( buf = alloca( len)))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca failed\n");
	#endif
		return ;
	}

	kik_snprintf( buf , len , "uim:%s" , engine_name) ;

	/*
	 * we don't use uim_change_input_method_engine(), since it cannot
	 * specify encoding.
	 */

	if( strcmp( request , "im_change_this_text_area_only") == 0)
	{
		if( focused_uim)
		{
			(*focused_uim->im.listener->im_changed)(
						focused_uim->im.listener->self ,
						buf) ;
		}
	}
	else if( strcmp( request , "im_change_whole_desktop") == 0 ||
		 strcmp( request , "im_change_this_application_only") == 0)
	{
		im_uim_t *  uim ;

		uim = uim_list ;
		while( uim)
		{
			(*uim->im.listener->im_changed)(
						uim->im.listener->self ,
						buf) ;
			uim = kik_slist_next( uim) ;
		}
	}
}

static void
helper_update_custom(
	char *  custom ,
	char *  value
	)
{
	im_uim_t *  uim ;

	uim = uim_list ;
	while( uim)
	{
		uim_prop_update_custom( uim->context , custom , value) ;
		uim = kik_slist_next( uim) ;
	}

}

static void
helper_commit_string(
	u_char *  str	/* UTF-8? */
	)
{
	mkf_parser_t *  parser_utf8 ;
	mkf_conv_t *  conv ;
	u_char  conv_buf[256] ;
	size_t  filled_len ;

	if( ! focused_uim)
	{
		return ;
	}

	if( focused_uim->term_encoding == ML_UTF8)
	{
		(*focused_uim->im.listener->write_to_term)(
						focused_uim->im.listener->self ,
						str , strlen( str)) ;
		return ;
	}

	if( ! ( conv = (*syms->ml_conv_new)( focused_uim->term_encoding)))
	{
		return ;
	}

	if( ! ( parser_utf8 = (*syms->ml_parser_new)( ML_UTF8)))
	{
		(*conv->delete)( conv) ;
		return ;
	}

	(*parser_utf8->init)( parser_utf8) ;
	(*parser_utf8->set_str)( parser_utf8 , str , strlen( str)) ;

	(*conv->init)( conv) ;

	while( ! parser_utf8->is_eos)
	{
		filled_len = (*conv->convert)( conv , conv_buf ,
					       sizeof( conv_buf) ,
					       parser_utf8) ;

		if( filled_len == 0)
		{
			/* finished converting */
			break ;
		}

		(*focused_uim->im.listener->write_to_term)(
						focused_uim->im.listener->self ,
						conv_buf , filled_len) ;
	}

	(*parser_utf8->delete)( parser_utf8) ;
	(*conv->delete)( conv) ;
}

static void
helper_read_handler(void)
{
	char *  message ;

	uim_helper_read_proc( helper_fd);

	while( ( message = uim_helper_get_message()))
	{
		char *  first_line ;
		char *  second_line ;

	#ifdef  IM_UIM_DEBUG
		kik_debug_printf( "message recieved from helper: %s\n" , message);
	#endif

		if( ( first_line = kik_str_sep( &message , "\n")))
		{
			if( strcmp( first_line , "prop_activate") == 0)
			{
				second_line = kik_str_sep( &message , "\n") ;
				if( second_line && focused_uim)
				{
					uim_prop_activate( focused_uim->context , second_line) ;
				}
			}
			else if( strcmp( first_line , "im_list_get") == 0)
			{
				helper_send_imlist() ;
			}
			else if( strncmp( first_line , "im_change_" , 10) == 0)
			{
				if( ( second_line = kik_str_sep( &message , "\n")))
				{
					helper_im_changed( first_line , second_line) ;
				}
			}
			else if( strcmp( first_line , "prop_update_custom") == 0)
			{
				if( ( second_line = kik_str_sep( &message , "\n")))
				{
					helper_update_custom( second_line , message) ;
				}
			}
			else if( strcmp( first_line , "focus_in") == 0)
			{
				focused_uim = NULL ;
			}
			else if( strcmp( first_line , "commit_string") == 0)
			{
				if( ( second_line = kik_str_sep( &message , "\n")))
				{
					helper_commit_string( second_line) ;
				}
			}

			message = first_line ; /* for free() */
		}

		free( message) ;
	}
}


/* --- global functions --- */

x_im_t *
im_uim_new(
	u_int64_t  magic ,
	ml_char_encoding_t  term_encoding ,
	x_im_export_syms_t *  export_syms ,
	char *  engine ,
	u_int  mod_ignore_mask
	)
{
	im_uim_t *  uim ;
	char *  encoding_name ;
	ml_char_encoding_t  encoding ;

	if( magic != (u_int64_t) IM_API_COMPAT_CHECK_MAGIC)
	{
		kik_error_printf( "Incompatible input method API.\n") ;

		return  NULL ;
	}

	uim = NULL ;
	encoding_name = NULL ;

	if( getenv( "MOD_KEY_DEBUG"))
	{
		mod_key_debug = 1 ;
	}

#if 1
#define  RESTORE_LOCALE
#endif

	if( ! initialized)
	{
	#ifdef  RESTORE_LOCALE
		/*
		 * Workaround against make_locale() of m17nlib.
		 */
		char *  cur_locale ;
		cur_locale = kik_str_alloca_dup( kik_get_locale()) ;
	#endif

		if( uim_init() == -1)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " failed to initialize uim.") ;
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

		initialized = 1 ;
	}

	/*
	 * create I/O chanel for uim_helper_server
	 */
	if( helper_fd == -1 && syms && syms->x_event_source_add_fd &&
	    syms->x_event_source_remove_fd)
	{
		helper_fd = uim_helper_init_client_fd( helper_disconnected) ;

		(*syms->x_event_source_add_fd)( helper_fd , helper_read_handler) ;
	}

	if( (engine == NULL) || (strlen( engine) == 0))
	{ 
		engine = (char*)uim_get_default_im_name(kik_get_locale()) ;
		/* The returned string's storage is invalidated when we
		 * call uim next, so we need to make a copy.  */
		engine = kik_str_alloca_dup (engine);
	}

	if( ! find_engine( engine , &encoding_name))
	{
		kik_error_printf( "%s: No such conversion engine.\n" , engine) ;
		goto  error ;
	}

	if( (encoding = (*syms->ml_get_char_encoding)( encoding_name)) == ML_UNKNOWN_ENCODING)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s is unknown encoding.\n" , encoding_name) ;
	#endif
		goto  error ;
	}

	if( ! ( uim = calloc( 1 , sizeof( im_uim_t))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif

		goto  error ;
	}

	uim->term_encoding = term_encoding ;
	uim->encoding_name = encoding_name ;
	uim->mod_ignore_mask =  mod_ignore_mask ;

	if( uim->term_encoding != encoding)
	{
		if( ! ( uim->parser_uim = (*syms->ml_parser_new)( encoding)))
		{
			goto  error ;
		}

		if( ! ( uim->conv = (*syms->ml_conv_new)( term_encoding)))
		{
			goto  error ;
		}
	}

	if( ! ( uim->parser_term = (*syms->ml_parser_new)( term_encoding)))
	{
		goto  error ;
	}

	if( ! ( uim->context = uim_create_context( uim ,
						   encoding_name ,
						   NULL ,
						   engine ,
						   NULL ,
						   commit)))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " could not create uim context.\n") ;
	#endif

		goto  error ;
	}

	uim->is_mozc = (strcmp( engine , "mozc") == 0) ;

	uim_set_preedit_cb( uim->context ,
			    preedit_clear ,
			    preedit_pushback ,
			    preedit_update) ;

	uim_set_candidate_selector_cb( uim->context ,
				       candidate_activate ,
				       candidate_select ,
				       candidate_shift_page ,
				       candidate_deactivate) ;

	uim_set_prop_list_update_cb( uim->context , prop_list_update) ;
	uim_set_prop_label_update_cb( uim->context , prop_label_update) ;

#ifdef  SHOW_STATUS_SCREEN
	uim_set_mode_cb( uim->context , mode_update) ;
#endif

	focused_uim = uim ;
	uim_prop_list_update( uim->context) ;

	/*
	 * set methods of x_im_t
	 */
	uim->im.delete = delete ;
	uim->im.key_event = key_event ;
	uim->im.switch_mode = switch_mode ;
	uim->im.is_active = is_active ;
	uim->im.focused = focused ;
	uim->im.unfocused = unfocused ;

	kik_slist_insert_head( uim_list , uim) ;

	ref_count ++;

#ifdef  IM_UIM_DEBUG
	kik_debug_printf("New object was created. ref_count is %d.\n", ref_count) ;
#endif

	return  (x_im_t*) uim ;

error:
	if( helper_fd != -1)
	{
		(*syms->x_event_source_remove_fd)( helper_fd) ;

		uim_helper_close_client_fd( helper_fd) ;

		helper_fd = -1 ;
	}

	if( initialized && ref_count == 0)
	{
		uim_quit() ;

		initialized = 0 ;
	}

	free( encoding_name);

	if( uim)
	{
		if( uim->parser_uim)
		{
			(*uim->parser_uim->delete)( uim->parser_uim) ;
		}

		if( uim->parser_term)
		{
			(*uim->parser_term->delete)( uim->parser_term) ;
		}

		if( uim->conv)
		{
			(*uim->conv->delete)( uim->conv) ;
		}

		free( uim) ;
	}

	return  NULL ;
}


/* --- API for external tools --- */

im_info_t *
im_uim_get_info(
	char *  locale ,
	char *  encoding
	)
{
	im_info_t *  result = NULL ;
	uim_context  u ;
	int  i ;

	if( uim_init() == -1)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " failed to initialize uim.") ;
	#endif

		return  NULL ;
	}

	if( ! ( u = uim_create_context( NULL , "UTF-8" , NULL , NULL ,
					NULL , NULL)))
	{
		goto  error ;
	}

	if( ! ( result = malloc( sizeof( im_info_t))))
	{
		goto  error ;
	}

	result->num_of_args = uim_get_nr_im( u) + 1 ;
	if( ! ( result->args = calloc( result->num_of_args , sizeof( char*))))
	{
		goto  error ;
	}

	if( ! ( result->readable_args = calloc( result->num_of_args , sizeof( char*))))
	{
		free( result->args) ;
		goto  error ;
	}

	result->args[0] = strdup( "") ;
	result->readable_args[0] = strdup( uim_get_default_im_name( locale)) ;

	for( i = 1 ; i < result->num_of_args; i++)
	{
		const char *  im_name ;
		const char *  lang_id ;
		size_t  len ;

		im_name = uim_get_im_name( u , i - 1) ;
		lang_id = uim_get_im_language( u , i - 1) ;

		result->args[i] = strdup( im_name) ;

		len = strlen( im_name) + strlen( lang_id) + 4 ;

		if( ( result->readable_args[i] = malloc(len)))
		{
			kik_snprintf( result->readable_args[i] , len ,
				      "%s (%s)" , im_name , lang_id) ;
		}
		else
		{
			result->readable_args[i] = strdup( "error") ;
		}
	}

	uim_release_context( u) ;

	uim_quit() ;

	result->id = strdup( "uim") ;
	result->name = strdup( "uim") ;

	return  result ;

error:

	if( u)
	{
		uim_release_context( u) ;
	}

	uim_quit() ;

	if( result)
	{
		if( result->args)
		{
			free( result->args) ;
		}

		if( result->readable_args)
		{
			free( result->readable_args) ;
		}

		free( result) ;
	}

	return  NULL ;
}

