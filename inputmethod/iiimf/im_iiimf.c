/*
 * im_iiimf.c - iiimf plugin for mlterm
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
 *	$Id$
 */


#define HAVE_STDINT_H 1		/* FIXME */
#include  <iiimcf.h>
#include  <string.h>		/* strncmp */
#include  <X11/keysym.h>	/* XK_xxx */
#include  <kiklib/kik_str.h>	/* kik_str_sep, kik_snprintf */
#include  <kiklib/kik_locale.h>	/* kik_get_lang */
#include  <kiklib/kik_debug.h>
#include  <mkf/mkf_utf16_parser.h>

#include  <x_im.h>
#include  "im_iiimf_keymap.h"
#include  "../im_common.h"

#if  0
#define IM_IIIMF_DEBUG 1
#endif

/*
 * taken from Minami-san's hack in x_dnd.c
 * Note: The byte order is same as client.
 *       (see lib/iiimp/data/im-connect.c:iiimp_connect_new())
 */
#define PARSER_INIT_WITH_BOM(parser)				\
do {								\
	u_int16_t BOM[] = {0xfeff};				\
	(*(parser)->init)( ( parser)) ;				\
	(*(parser)->set_str)( ( parser) , (u_char*)BOM , 2) ;	\
	(*(parser)->next_char)( ( parser) , NULL) ;		\
} while(0)

/* XXX: it seems to undefined in iiim*.h. why? */
#define  IIIMF_SHIFT_MODIFIER	1
#define  IIIMF_CONTROL_MODIFIER	2
#define  IIIMF_META_MODIFIER	4
#define  IIIMF_ALT_MODIFIER	8


typedef struct im_iiimf
{
	/* input method common object */
	x_im_t  im ;

	IIIMCF_context  context ;

	mkf_parser_t *  parser_term ;
	mkf_conv_t *  conv ;

}  im_iiimf_t ;

/* --- static variables --- */

static int  ref_count = 0 ;
static int  initialized = 0 ;
static mkf_parser_t *  parser_utf16 = NULL ;
static IIIMCF_handle  handle = NULL ;

/* mlterm internal symbols */
static x_im_export_syms_t *  mlterm_syms = NULL ;

/* --- static functions --- */

static size_t
strlen_utf16(
	const IIIMP_card16 *  str
	)
{
	int  len = 0 ;

	if( ! str)
	{
		return 0 ;
	}

	while ( *str)
	{
		len += 2 ;
		str ++ ;
		if( len == 0xffff) /* prevent infinity loop */
		{
			return  0 ;
		}
	}

	return  (size_t) len ;
}

static IIIMCF_language
find_language(
	char *  language_id
	)
{
	IIIMCF_language *  array ;
	IIIMCF_language  result = NULL ;
	char *  p = NULL ;
	char *  language = NULL;
	char *  country = NULL ;
	int  num ;
	int  i ;

	if( language_id)
	{
		p = strdup( language_id) ;

		if( ( country = kik_str_sep( &p , ":")))
		{
			p = country ; /* hold for free() */
			language = kik_str_sep( &country , "_") ;
		}
	}

	if( language == NULL && country == NULL)
	{
		language = kik_get_lang() ;
		country = kik_get_country() ;
	}

	if( iiimcf_get_supported_languages( handle , &num , &array) == IIIMF_STATUS_SUCCESS)
	{
		for(i = 0 ; i < num ; i++)
		{
			const char *  lang_id ;

			if( iiimcf_get_language_id( array[i] , &lang_id) != IIIMF_STATUS_SUCCESS)
			{
				continue ;
			}

			/* At first, compare with lang_COUNTRY like "zh_CN" */
			if( country)
			{
				char  buf[16] ;

				kik_snprintf( buf, sizeof(buf) , "%s_%s" ,
					      language , country) ;

				if( strcmp( buf , lang_id) == 0)
				{
					result = array[i] ;
					break ;
				}
			}

			/* Second, compare with lang like "ja" */
			if( strcmp( language , lang_id) == 0)
			{
				result = array[i] ;
				break ;
			}
		}
	}


	if( p)
	{
		free( p) ;
	}

	return  result ;
}

static IIIMCF_input_method
find_language_engine(
	char *  le_name
       )
{
	IIIMCF_input_method *  array ;
	IIIMCF_input_method  result = NULL ;
	char *  le_name = NULL ;
	mkf_conv_t *  conv ;
	int  num ;
	int  i ;

	if( ! le_name)
	{
		return  NULL ;
	}

	if( ! ( le_name = strstr( le_name , ":")))
	{
		return  NULL ;
	}

	le_name ++;	/* ex. ":CannaLE" -> "CannaLE" */

	if( ! ( conv = (*mlterm_syms->ml_conv_new)( ML_ISO8859_1)))
	{
		return  NULL ;
	}

	if( iiimcf_get_supported_input_methods( handle , &num , &array) == IIIMF_STATUS_SUCCESS)
	{
		for( i = 0 ; i < num ; i++)
		{
			const IIIMP_card16 * id ;
			const IIIMP_card16 * hrn ; /* human readable name */
			const IIIMP_card16 * domain ;

			if( iiimcf_get_input_method_desc( array[i] ,
							  &id ,
							  &hrn ,
							  &domain)
							== IIIMF_STATUS_SUCCESS)
			{
				size_t  len ;
				u_int  filled_len ;
				u_char  buf[256] ;

				len = strlen_utf16( id) ;
				PARSER_INIT_WITH_BOM( parser_utf16) ;
				(*parser_utf16->set_str)( parser_utf16 ,
							  (u_char*) id ,
							  len) ;
				filled_len = (*conv->convert)( conv ,
							       buf ,
							       sizeof(buf) - 1 ,
							       parser_utf16) ;
				buf[filled_len] = '\0' ;

				if( strncmp( le_name , buf , filled_len) == 0)
				{
				#ifdef  IM_IIIMF_DEBUG
					kik_warn_printf( KIK_DEBUG_TAG " found %s.\n", le_name) ;
				#endif

					result = array[i] ;
				}
			}
		}
	}

	(*conv->delete)( conv) ;

	return  result ;
}


/*
 * processing event from IIIMSF
 */

static void
commit(
	im_iiimf_t *  iiimf
	)
{
	IIIMCF_text  iiimcf_text ;
	const IIIMP_card16  *utf16str ;
	u_char  buf[256] ;
	size_t  filled_len ;

#ifdef  IM_UIM_DEBUG
	kik_warn_printf( KIK_DEBUG_TAG "\n");
#endif

	if( iiimcf_get_committed_text( iiimf->context , &iiimcf_text) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Cound not get committed text\n") ;
	#endif
		return ;
	}

	if( iiimcf_get_text_utf16string( iiimcf_text , &utf16str) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Cound not get utf16 string\n") ;
	#endif
		return ;
	}

	PARSER_INIT_WITH_BOM( parser_utf16) ;
	(*parser_utf16->set_str)( parser_utf16 ,
				  (u_char*) utf16str ,
				  strlen_utf16( utf16str)) ;

	(*iiimf->conv->init)( iiimf->conv) ;

	while( ! parser_utf16->is_eos)
	{
		filled_len = (*iiimf->conv->convert)( iiimf->conv ,
						      buf ,
						      sizeof( buf) ,
						      parser_utf16) ;

		if( filled_len == 0)
		{
			/* finished converting */
			break ;
		}

		(*iiimf->im.listener->write_to_term)( iiimf->im.listener->self ,
						      buf , filled_len) ;
	}
}

static void
preedit_start(
	im_iiimf_t *  iiimf
	)
{
#ifdef  IM_IIIMF_DEBUG
	kik_warn_printf( KIK_DEBUG_TAG "\n");
#endif

	if( iiimf->im.preedit.chars)
	{
		(*mlterm_syms->ml_str_delete)( iiimf->im.preedit.chars ,
					       iiimf->im.preedit.num_of_chars) ;
	}

	iiimf->im.preedit.num_of_chars = 0 ;
	iiimf->im.preedit.filled_len = 0 ;
	iiimf->im.preedit.segment_offset = 0 ;
	iiimf->im.preedit.cursor_offset = X_IM_PREEDIT_NOCURSOR ;
}

static void
preedit_change(
	im_iiimf_t *  iiimf
	)
{
	IIIMCF_text  iiimcf_text ;
	const IIIMP_card16 *  utf16str ;
	u_char *  str = NULL ;
	ml_char_t *  p ;
	mkf_char_t  ch ;
	int  is_underline = 0 ;
	int  caret_pos ;
	u_int  count = 0 ;
	int  in_reverse = 0 ;

#ifdef  IM_IIIMF_DEBUG
	kik_warn_printf( KIK_DEBUG_TAG "\n");
#endif

	/*
	 * get preedit string without attribute.
	 */

	if( iiimcf_get_preedit_text( iiimf->context , &iiimcf_text , &caret_pos) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
				 " Cound not get preedit text\n") ;
	#endif
		return ;
	}

	if( iiimcf_get_text_utf16string( iiimcf_text , &utf16str) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
				 " Cound not get utf16 string\n") ;
	#endif
		return ;
	}

	if( strlen_utf16( utf16str) == 0)
	{
		/* clear */
		(*iiimf->im.listener->draw_preedit_str)(
						iiimf->im.listener->self ,
						NULL , 0 , 0) ;

		if( iiimf->im.preedit.chars)
		{
			(*mlterm_syms->ml_str_delete)( iiimf->im.preedit.chars ,
						       iiimf->im.preedit.num_of_chars) ;
			iiimf->im.preedit.chars = NULL ;
			iiimf->im.preedit.num_of_chars = 0 ;
			iiimf->im.preedit.filled_len = 0 ;
		}

		return  ;
	}

	/* utf16 -> term encoding */
	PARSER_INIT_WITH_BOM( parser_utf16) ;
	im_convert_encoding( parser_utf16 , iiimf->conv ,
			     (u_char*) utf16str , &str ,
			     strlen_utf16( utf16str)) ;

	/*
	 * count number of characters to re-allocate im.preedit.chars
	 */

	(*iiimf->parser_term->init)( iiimf->parser_term) ;
	(*iiimf->parser_term->set_str)( iiimf->parser_term ,
					str , strlen( str)) ;

	while( (*iiimf->parser_term->next_char)( iiimf->parser_term , &ch))
	{
		count ++ ;
	}

	/*
	 * allocate im.preedit.chars
	 */
	if( iiimf->im.preedit.chars)
	{
		(*mlterm_syms->ml_str_delete)( iiimf->im.preedit.chars ,
					       iiimf->im.preedit.num_of_chars) ;
		iiimf->im.preedit.chars = NULL ;
		iiimf->im.preedit.num_of_chars = 0 ;
		iiimf->im.preedit.filled_len = 0 ;
	}

	if( ! ( iiimf->im.preedit.chars = malloc( sizeof(ml_char_t) * count)))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG "malloc failed.\n") ;
	#endif

		return ;
	}

	iiimf->im.preedit.num_of_chars = count ;
	iiimf->im.preedit.filled_len = 0 ;

	(*mlterm_syms->ml_str_init)( iiimf->im.preedit.chars ,
				     iiimf->im.preedit.num_of_chars) ;

	/*
	 * IIIMP_card16(utf16) -> ml_char_t
	 */

	p = iiimf->im.preedit.chars ;
	(*mlterm_syms->ml_str_init)( p , iiimf->im.preedit.num_of_chars) ;

	(*iiimf->parser_term->init)( iiimf->parser_term) ;
	(*iiimf->parser_term->set_str)( iiimf->parser_term ,
					str , strlen( str)) ;

	count = 0 ;

	/* TODO: move to inputmethod/common/im_common.c */
	while( (*iiimf->parser_term->next_char)( iiimf->parser_term , &ch))
	{
		IIIMP_card16  iiimcf_ch ;
		const IIIMP_card32 *  feedback_ids ;
		const IIIMP_card32 *  feedbacks ;
		int  num_of_feedbacks ;
		ml_color_t  fg_color = ML_FG_COLOR ;
		ml_color_t  bg_color = ML_BG_COLOR ;
		int  is_biwidth = 0 ;
		int  is_comb = 0 ;
		int  is_bold = 0 ;

		iiimf->im.preedit.cursor_offset = 0 ;

		if( iiimcf_get_char_with_feedback(
					iiimcf_text ,
					iiimf->im.preedit.filled_len ,
					&iiimcf_ch ,
					&num_of_feedbacks ,
					&feedback_ids ,
					&feedbacks) == IIIMF_STATUS_SUCCESS)
		{
			int  i ;

			for( i = 0 ; i < num_of_feedbacks ; i++)
			{
				/* IIIMP_FEEDBACK_[1,2,3...]_* don't exist? */
				if(feedback_ids[i] != IIIMP_FEEDBACK_0_ID)
				{
					continue ;
				}

				switch(feedbacks[i])
				{
				case IIIMP_FEEDBACK_0_REVERSE_VIDEO:
					if( ! in_reverse)
					{
						iiimf->im.preedit.segment_offset = iiimf->im.preedit.filled_len ;
						in_reverse = 1 ;
					}
					iiimf->im.preedit.cursor_offset =
							X_IM_PREEDIT_NOCURSOR ;
					fg_color = ML_BG_COLOR ;
					bg_color = ML_FG_COLOR ;
					break ;
				case IIIMP_FEEDBACK_0_UNDERLINE:
					is_underline = 1 ;
					break ;
				case IIIMP_FEEDBACK_0_HIGHLIGHT:
					is_bold = 1 ;
					break ;
				case IIIMP_FEEDBACK_0_NORMAL_VIDEO:
				case IIIMP_FEEDBACK_0_PRIMARY:
				case IIIMP_FEEDBACK_0_SECONDARY:
				case IIIMP_FEEDBACK_0_TERTIARY:
					/* not implemented yet */
				default:
					break ;
				}
			}
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

		if( is_comb)
		{
			if( (*mlterm_syms->ml_char_combine)( p - 1 , ch.ch ,
							     ch.size , ch.cs ,
							     is_biwidth ,
							     is_comb ,
							     fg_color ,
							     bg_color ,
							     is_bold ,
							     is_underline))
			{
				continue;
			}

			/*
			 * if combining failed , char is normally appended.
			 */
		}

		if( (*mlterm_syms->ml_is_msb_set)( ch.cs))
		{
			SET_MSB( ch.ch[0]) ;
		}

		(*mlterm_syms->ml_char_set)( p , ch.ch , ch.size , ch.cs ,
					     is_biwidth , is_comb ,
					     fg_color , bg_color ,
					     is_bold , is_underline) ;

		p++ ;
		iiimf->im.preedit.filled_len++;
	}

	if( str)
	{
		free( str) ;
	}

	if( iiimf->im.preedit.cursor_offset != X_IM_PREEDIT_NOCURSOR)
	{
		iiimf->im.preedit.cursor_offset = iiimf->im.preedit.filled_len ;
	}

	(*iiimf->im.listener->draw_preedit_str)(
					iiimf->im.listener->self ,
					iiimf->im.preedit.chars ,
					iiimf->im.preedit.filled_len ,
					iiimf->im.preedit.cursor_offset) ;
}

static void
preedit_done(
	im_iiimf_t *  iiimf
	)
{
#ifdef  IM_IIIMF_DEBUG
	kik_warn_printf( KIK_DEBUG_TAG "\n");
#endif

	if( iiimf->im.preedit.chars)
	{
		(*mlterm_syms->ml_str_delete)( iiimf->im.preedit.chars ,
					       iiimf->im.preedit.num_of_chars) ;
		iiimf->im.preedit.chars = NULL ;
		iiimf->im.preedit.num_of_chars = 0 ;
		iiimf->im.preedit.filled_len = 0 ;
	}

	(*iiimf->im.listener->draw_preedit_str)(
					iiimf->im.listener->self ,
					iiimf->im.preedit.chars ,
					iiimf->im.preedit.filled_len ,
					iiimf->im.preedit.cursor_offset) ;
}

static void
lookup_choice_start(
	im_iiimf_t *  iiimf
	)
{
}

static void
lookup_choice_change(
	im_iiimf_t *  iiimf
	)
{
	IIIMCF_lookup_choice  lookup_choice ;
	int  size ;
	int  index_first ;
	int  index_last ;
	int  index_current ;
	int  is_vertical ;
	u_int  line_height ;
	int  x ;
	int  y ;
	int  i ;

	if( iiimcf_get_lookup_choice( iiimf->context , &lookup_choice) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Cound not get lookup table\n") ;
	#endif
		return ;
	}

	if( iiimcf_get_lookup_choice_size( lookup_choice ,
					   &size ,
					   &index_first ,
					   &index_last ,
					   &index_current) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Cound not get lookup table\n") ;
	#endif
		return ;
	}

	if( index_first == 0 && index_last == 0)
	{
		return ;
	}

		(*iiimf->im.listener->get_spot)(
					iiimf->im.listener->self ,
					iiimf->im.preedit.chars ,
					iiimf->im.preedit.segment_offset ,
					&x , &y) ;

	if( iiimf->im.cand_screen)
	{
		(*iiimf->im.cand_screen->delete)( iiimf->im.cand_screen) ;
		iiimf->im.cand_screen = NULL ;
	}

	if( ! ( iiimf->im.cand_screen = (*mlterm_syms->x_im_candidate_screen_new)(
			(*iiimf->im.listener->get_win_man)(iiimf->im.listener->self) ,
			(*iiimf->im.listener->get_font_man)(iiimf->im.listener->self) ,
			(*iiimf->im.listener->get_color_man)(iiimf->im.listener->self) ,
			(*iiimf->im.listener->is_vertical)(iiimf->im.listener->self) ,
			(*iiimf->im.listener->get_line_height)(iiimf->im.listener->self) ,
			x , y)))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_im_candidate_screen_new() failed.\n") ;
	#endif
		
		return ;
	}

	iiimf->im.cand_screen->listener.self = iiimf ;
	iiimf->im.cand_screen->listener.selected = NULL ; /* XXX */

	if( ! (*iiimf->im.cand_screen->init)( iiimf->im.cand_screen , size))
	{
		(*iiimf->im.cand_screen->delete)( iiimf->im.cand_screen) ;
		iiimf->im.cand_screen = NULL ;
		return ;
	}

	(*iiimf->im.cand_screen->set_spot)( iiimf->im.cand_screen , x , y) ;

	for( i = 0 ; i <= index_last ; i++)
	{
		IIIMCF_text  iiimcf_text_cand ;
		IIIMCF_text  iiimcf_text_label ;
		const IIIMP_card16 *  utf16str ;
		u_char *  str = NULL ;
		int flag ;

		if( iiimcf_get_lookup_choice_item( lookup_choice , i ,
						   &iiimcf_text_cand ,
						   &iiimcf_text_label ,
						   &flag) != IIIMF_STATUS_SUCCESS)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " Cound not candidate item\n") ;
		#endif
			continue ;
		}

		if( iiimcf_get_text_utf16string(
					iiimcf_text_cand ,
					&utf16str) != IIIMF_STATUS_SUCCESS)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
					 " Cound not get utf16 string\n") ;
		#endif
			continue ;
		}

		PARSER_INIT_WITH_BOM( parser_utf16) ;
		if( im_convert_encoding( parser_utf16 , iiimf->conv ,
					 (u_char*)utf16str , &str ,
					 strlen_utf16( utf16str)))
		{
			(*iiimf->im.cand_screen->set)(
						iiimf->im.cand_screen ,
						iiimf->parser_term ,
						str , i) ;
			free( str) ;
		}
	}

	(*iiimf->im.cand_screen->select)( iiimf->im.cand_screen ,
					  index_current) ;
}

static void
lookup_choice_done(
	im_iiimf_t *  iiimf
	)
{
	if( iiimf->im.cand_screen)
	{
		(*iiimf->im.cand_screen->delete)( iiimf->im.cand_screen) ;
		iiimf->im.cand_screen = NULL ;
	}
}

static void
dispatch(
	im_iiimf_t *  iiimf ,
	IIIMCF_event_type  type
	)
{
	switch( type)
	{
	case IIIMCF_EVENT_TYPE_DESTROY:
	case IIIMCF_EVENT_TYPE_RESET:
	case IIIMCF_EVENT_TYPE_EVENTLIKE:
/*	case IIIMCF_EVENT_TYPE_KEYEVENT: */
	case IIIMCF_EVENT_TYPE_TRIGGER_NOTIFY:
	case IIIMCF_EVENT_TYPE_OPERATION:
	case IIIMCF_EVENT_TYPE_SETICFOCUS:
	case IIIMCF_EVENT_TYPE_UNSETICFOCUS:
#if 0	/* XXX: for Fedora Core 2 */
	case IIIMCF_EVENT_TYPE_HOTKEY_NOTIFY:
#endif
		/* not implemented yet */
		break ;
	case IIIMCF_EVENT_TYPE_UI_PREEDIT_START:
		preedit_start( iiimf) ;
		break ;
	case IIIMCF_EVENT_TYPE_UI_PREEDIT_CHANGE:
		preedit_change( iiimf) ;
		break ;
	case IIIMCF_EVENT_TYPE_UI_PREEDIT_DONE:
		preedit_done( iiimf) ;
		break ;
	case IIIMCF_EVENT_TYPE_UI_LOOKUP_CHOICE_START:
		lookup_choice_start( iiimf) ;
		break ;
	case IIIMCF_EVENT_TYPE_UI_LOOKUP_CHOICE_CHANGE:
		lookup_choice_change( iiimf) ;
		break ;
	case IIIMCF_EVENT_TYPE_UI_LOOKUP_CHOICE_DONE:
		lookup_choice_done( iiimf) ;
		break ;
	case IIIMCF_EVENT_TYPE_UI_STATUS_START:
	case IIIMCF_EVENT_TYPE_UI_STATUS_CHANGE:
	case IIIMCF_EVENT_TYPE_UI_STATUS_DONE:
		/* not implemented yet */
		break;
	case IIIMCF_EVENT_TYPE_UI_COMMIT:
		commit( iiimf);
		break;
	case IIIMCF_EVENT_TYPE_AUX_START:
	case IIIMCF_EVENT_TYPE_AUX_DRAW:
#if 0   /* XXX: for Fedora Core 2 */
	case IIIMCF_EVENT_TYPE_AUX_SETVALUES:
#endif
	case IIIMCF_EVENT_TYPE_AUX_DONE:
	case IIIMCF_EVENT_TYPE_AUX_GETVALUES:
		/* not implemented yet */
		break;
	default:
		break ;
	}
}

/*
 * methods of x_im_t
 */

static void
delete(
	x_im_t *  im
	)
{
	im_iiimf_t *  iiimf ;

	iiimf = (im_iiimf_t*) im ;

	if( iiimf->parser_term)
	{
		(*iiimf->parser_term->delete)( iiimf->parser_term) ;
	}

	if( iiimf->conv)
	{
		(*iiimf->conv->delete)( iiimf->conv) ;
	}

	if( iiimf->im.cand_screen)
	{
		(*iiimf->im.cand_screen->delete)( iiimf->im.cand_screen) ;
	}

	if( iiimf->context)
	{
		iiimcf_destroy_context( iiimf->context) ;
	}

	free( iiimf) ;

	ref_count-- ;

#ifdef  IM_IIIMF_DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " An object was deleted. ref_count: %d\n", ref_count) ;
#endif

	if( ref_count == 0 && initialized)
	{
		iiimcf_destroy_handle( handle) ;

		handle = NULL ;

		iiimcf_finalize() ;

		initialized = 0 ;
	}
}

static int
key_event(
	x_im_t *  im ,
	KeySym  ksym ,
	XKeyEvent *  xevent
	)
{
	im_iiimf_t *  iiimf ;
	IIIMCF_keyevent  key ;
	IIIMCF_event  key_event ;
	IIIMF_status  status ;
	int  ret ;

	iiimf = (im_iiimf_t*) im ;

	key.keycode = 0 ;
	key.keychar = 0 ;
	key.modifier = 0 ;

	if( xevent->state & ShiftMask)
		key.modifier |= IIIMF_SHIFT_MODIFIER ;
	if( xevent->state & ControlMask)
		key.modifier |= IIIMF_CONTROL_MODIFIER ;
	if( xevent->state & Mod1Mask)
		key.modifier |= IIIMF_ALT_MODIFIER ; /* XXX */
	if( xevent->state & Mod3Mask)
		key.modifier |= IIIMF_META_MODIFIER ; /* XXX */

	xksym_to_iiimfkey( ksym , &key.keychar , &key.keycode) ;

	key.time_stamp = xevent->time ;

	if( iiimcf_create_keyevent( &key , &key_event) == IIIMF_STATUS_SUCCESS)
	{
		status = iiimcf_forward_event( iiimf->context , key_event) ;

		switch( status)
		{
		case IIIMF_STATUS_SUCCESS:
			goto  dispatch ;
		case IIIMF_STATUS_EVENT_NOT_FORWARDED:
			break ;
		case IIIMF_STATUS_IC_INVALID:
			kik_error_printf( "invalid IIIMCF context\n");
			break ;
		case IIIMF_STATUS_MALLOC:
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " iiimcf internal error "
					 "[IIIMF_STATUS_MALLOC]\n");
		#endif
			break ;
		default:
			kik_error_printf( "Cound not send key event to IIIMSF\n");
			break ;
		}
	}

	return  1;

dispatch:

	while( 1)
	{
		IIIMCF_event  received_event ;
		IIIMCF_event_type  type ;

		if( iiimcf_get_next_event( iiimf->context , &received_event) != IIIMF_STATUS_SUCCESS)
		{
			break ;
		}

		if( iiimcf_get_event_type( received_event , &type) != IIIMF_STATUS_SUCCESS)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " Cound not get event type\n");
		#endif

			return  0 ;
		}

		if( type == IIIMCF_EVENT_TYPE_KEYEVENT)
		{
			/* don't dispatch this event */
			ret = 1 ;
		}
		else
		{
			dispatch( iiimf , type) ;
			ret = 0 ; /* key event was swallowed by IIIMSF */
		}

		/*
		 * from 'IIICF library specification':'IIIMCF object life-cycle'
		 *
		 * NOTICE: old version of libiiimcf had discarded any events
		 * dispatched by iiimcf_dispatch_event. But in the new
		 * version, you MUST call iiimcf_ignore_event to all events
		 * that are obtained by iiimcf_get_next_event even though they
		 * are dispatched by iiimcf_dispatch_event.
		 */
		if( iiimcf_dispatch_event( iiimf->context , received_event) != IIIMF_STATUS_SUCCESS)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " Cound not dispatch event\n");
		#endif
		}

		if( iiimcf_ignore_event( received_event) != IIIMF_STATUS_SUCCESS)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " Cound not dispatch event\n");
		#endif
		}
	}

	return  ret ;
}

static void
focused(
	x_im_t *  im
	)
{
	im_iiimf_t *  iiimf ;
	IIIMCF_event  event ;

#ifdef  IM_IIIMF_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n");
#endif

	iiimf =  (im_iiimf_t*)  im ;

	if( iiimcf_create_seticfocus_event( &event) == IIIMF_STATUS_SUCCESS)
	{
		IIIMF_status  status ;

		status = iiimcf_forward_event( iiimf->context , event) ;
		if( status != IIIMF_STATUS_SUCCESS)
		{
			kik_error_printf( "Cound not forward focus event to "
					  "IIIMSF [status = %d]\n", status) ;
		}
	}
}

static void
unfocused(
	x_im_t *  im
	)
{
	im_iiimf_t *  iiimf ;
	IIIMCF_event  event ;

#ifdef  IM_IIIMF_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n");
#endif

	iiimf = (im_iiimf_t*)  im ;

	if( iiimcf_create_unseticfocus_event( &event) == IIIMF_STATUS_SUCCESS)
	{
		IIIMF_status  status ;

		status = iiimcf_forward_event( iiimf->context , event) ;
		if( status != IIIMF_STATUS_SUCCESS)
		{
			kik_error_printf( "Cound not forward unfocus event to "
					  "IIIMSF [status = %d]\n", status) ;
		}
	}
}


/* --- global functions --- */

x_im_t *
im_new(
	u_int64_t  magic ,
	ml_char_encoding_t  term_encoding ,
	x_im_export_syms_t *  export_syms ,
	char *  lang	/* <language id(rfc1766)>:<language engine> */
	)
{
	im_iiimf_t *  iiimf = NULL ;
	IIIMCF_attr  attr = NULL ;
	IIIMCF_language language ;
	IIIMCF_input_method  language_engine ;

	if( magic != (u_int64_t) IM_API_COMPAT_CHECK_MAGIC)
	{
		kik_error_printf( "Incompatible input method API.\n") ;

		return  NULL ;
	}

	if( ! initialized)
	{
		if( iiimcf_initialize( IIIMCF_ATTR_NULL) != IIIMF_STATUS_SUCCESS)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " Cound not initialize\n") ;
		#endif
			return  NULL ;
		}

		initialized = 1 ;

		if( iiimcf_create_attr( &attr) != IIIMF_STATUS_SUCCESS)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " Cound not create attribute\n") ;
		#endif
			goto  error ;
		}

		if( iiimcf_attr_put_string_value( attr ,
						  IIIMCF_ATTR_CLIENT_TYPE ,
						  "IIIMF plugin for mlterm")
							!= IIIMF_STATUS_SUCCESS)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " Cound not append a string to the attribute\n") ;
		#endif
			goto  error ;
		}

		if( iiimcf_create_handle( attr , &handle) != IIIMF_STATUS_SUCCESS)
		{
			kik_error_printf( "Cound not create handle for IIIMF\n") ;
			goto  error ;
		}

		iiimcf_destroy_attr( attr) ;
		attr = NULL ;

		if( ! ( parser_utf16 = mkf_utf16_parser_new()))
		{
			goto  error ;
		}

		mlterm_syms = export_syms ;
	}

	language = find_language( lang) ;
	language_engine = find_language_engine( lang) ;

	if( ! ( iiimf = malloc( sizeof( im_iiimf_t))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif

		goto  error ;
	}

	/*
	 * set methods of x_im_t
	 */
	iiimf->im.delete = delete ;
	iiimf->im.key_event = key_event ;
	iiimf->im.focused = focused ;
	iiimf->im.unfocused = unfocused ;

	if( ! ( iiimf->parser_term = (*mlterm_syms->ml_parser_new)( term_encoding)))
	{
		goto  error ;
	}

	if( ! ( iiimf->conv = (*mlterm_syms->ml_conv_new)( term_encoding)))
	{
		goto  error ;
	}

	if( iiimcf_create_attr( &attr) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Cound not create attribute\n") ;
	#endif
		goto  error ;
	}

	if( language)
	{
		if( iiimcf_attr_put_ptr_value( attr ,
					       IIIMCF_ATTR_INPUT_LANGUAGE ,
					       language)
							!= IIIMF_STATUS_SUCCESS)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " Cound not append value to the attribute\n") ;
		#endif
		}
	}

	if( language_engine)
	{
		if( iiimcf_attr_put_ptr_value( attr ,
					       IIIMCF_ATTR_INPUT_METHOD ,
					       language_engine)
							!= IIIMF_STATUS_SUCCESS)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " Cound not append value to the attribute\n") ;
		#endif
		}
	}

	if( iiimcf_create_context( handle , attr , &iiimf->context) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Cound not create context\n") ;
	#endif
		goto  error ;
	}

	iiimcf_destroy_attr( attr) ;
	attr = NULL ;

	ref_count++ ;

#ifdef  IM_IIIMF_DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " New object was created. ref_count is %d.\n" , ref_count);
#endif

	return  (x_im_t*) iiimf ;

error:

	if( attr)
	{
		iiimcf_destroy_attr( attr) ;
	}

	if( initialized && ref_count == 0)
	{
		if( handle)
		{
			iiimcf_destroy_handle( handle) ;
		}

		handle = NULL ;

		if( parser_utf16)
		{
			(*parser_utf16->delete)( parser_utf16) ;
		}

		iiimcf_finalize() ;

		initialized = 0 ;
	}

	if( iiimf)
	{
		if( iiimf->parser_term)
		{
			(*iiimf->parser_term->delete)( iiimf->parser_term) ;
		}

		if( iiimf->conv)
		{
			(*iiimf->conv->delete)( iiimf->conv) ;
		}

		free( iiimf) ;
	}

	return  NULL ;
}

