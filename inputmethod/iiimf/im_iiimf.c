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

#include  <kiklib/kik_types.h> /* HAVE_STDINT_H */
#include  <iiimcf.h>

#include  <string.h>		/* strncmp */
#include  <X11/keysym.h>	/* XK_xxx */
#include  <kiklib/kik_mem.h>	/* malloc/alloca/free */
#include  <kiklib/kik_str.h>	/* kik_str_sep kik_str_alloca_dup kik_snprintf*/
#include  <kiklib/kik_locale.h>	/* kik_get_lang */
#include  <kiklib/kik_debug.h>
#include  <mkf/mkf_utf16_parser.h>
#include  <mkf/mkf_iso8859_conv.h>

#include  <x_im.h>
#include  "im_iiimf_keymap.h"
#include  "../im_common.h"
#include  "../im_info.h"

#if  0
#define  IM_IIIMF_DEBUG 1
#endif

#if  1	/* TODO: should be replaced by handling aux. */
#define  IM_IIIMF_ATOKX_LOOKUP_HACK 1
#endif

/*
 * taken from Minami-san's hack in x_dnd.c
 * Note: The byte order is the same as client.
 *       (see lib/iiimp/data/im-connect.c:iiimp_connect_new())
 */
#define PARSER_INIT_WITH_BOM(parser)				\
do {								\
	u_int16_t BOM[] = {0xfeff};				\
	(*(parser)->init)( ( parser)) ;				\
	(*(parser)->set_str)( ( parser) , (u_char*)BOM , 2) ;	\
	(*(parser)->next_char)( ( parser) , NULL) ;		\
} while(0)

/* XXX: it seems to be undefined in iiim*.h. why? */
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
static x_im_export_syms_t *  syms = NULL ;

/* --- static functions --- */

static size_t
strlen_utf16(
	const IIIMP_card16 *  str
	)
{
	size_t  len = 0 ;

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

	return  len ;
}

static IIIMCF_language
find_language(
	char *  lang
	)
{
	IIIMCF_language *  array ;
	IIIMCF_language  best_result = NULL ;
	IIIMCF_language  better_result = NULL ;
	char *  p = NULL ;
	char *  language = NULL;
	char *  country = NULL ;
	int  num ;
	int  i ;

	if( lang)
	{
		p = kik_str_alloca_dup( lang) ;

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
					best_result = array[i] ;
				}
			}

			/* Next, compare with lang like "ja" */
			if( strcmp( language , lang_id) == 0)
			{
				better_result = array[i] ;
			}
		}
	}

	return  (best_result ? best_result : better_result) ;
}

static IIIMCF_input_method
find_language_engine(
	char *  lang
       )
{
	IIIMCF_input_method *  array ;
	IIIMCF_input_method  result = NULL ;
	char *  le_name = NULL ;
	mkf_conv_t *  conv ;
	int  num ;
	int  i ;

	if( ! lang)
	{
		return  NULL ;
	}

	if( ! ( le_name = strstr( lang , ":")))
	{
		return  NULL ;
	}

	le_name ++;	/* remove ":". ex. ":CannaLE" -> "CannaLE" */

	if( ! ( conv = (*syms->ml_conv_new)( ML_ISO8859_1)))
	{
		return  NULL ;
	}

	if( iiimcf_get_supported_input_methods( handle ,
						&num ,
						&array) == IIIMF_STATUS_SUCCESS)
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
				u_char *  str ;

				PARSER_INIT_WITH_BOM( parser_utf16) ;
				im_convert_encoding( parser_utf16 , conv ,
						     (u_char*) id , &str ,
						     strlen_utf16( id) + 1) ;

				if( strcmp( le_name , str) == 0)
				{
				#ifdef  IM_IIIMF_DEBUG
					kik_debug_printf( KIK_DEBUG_TAG " found le [%s].\n", le_name) ;
				#endif

					result = array[i] ;
				}

				free(str);
			}
		}
	}

	(*conv->delete)( conv) ;

	return  result ;
}


static void
show_iiimcf_version( void)
{
#ifdef  IM_IIIMF_DEBUG
#define  SHOW_VER( flag , str)						\
do { 									\
	int  ver ;							\
	if( iiimcf_get_version_number( handle , (flag) , &ver)		\
						== IIIMF_STATUS_ARGUMENT)\
	{								\
		kik_debug_printf( KIK_DEBUG_TAG "%s\t\t:%d\n" , (str) , ver) ;\
	}								\
	else								\
	{								\
		kik_debug_printf( KIK_DEBUG_TAG "%s\t\t:none\n" , (str)) ;\
	}								\
} while (0)

	SHOW_VER( IIIMCF_LIBRARY_VERSION , "library version") ;
	SHOW_VER( IIIMCF_PROTOCOL_VERSION , "protocol version") ;
	SHOW_VER( IIIMCF_MINOR_VERSION , "minor version") ;
	SHOW_VER( IIIMCF_MAJOR_VERSION , "major version") ;
#endif
}

static void
show_available_gui_objects(void)
{
#ifdef  IM_IIIMF_DEBUG
	const IIIMCF_object_descriptor *  objdesc_list ;
	IIIMCF_object_descriptor **  bin_objdesc_list = NULL ;
	IIIMCF_downloaded_object * objs = NULL ;
	IIIMF_status  status ;
	int  i ;
	int  num_of_objs ;
	int  num_of_bin_objs = 0 ;
	mkf_conv_t *  conv = NULL ;

	if( ! ( conv = (*syms->ml_conv_new)( ML_ISO8859_1)))
	{
		return ;
	}

	if( iiimcf_get_object_descriptor_list(
					handle ,
					&num_of_objs ,
					&objdesc_list) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " iiimcf_get_object_descriptor_list failed.\n");
	#endif
		goto  error ;
	}

	bin_objdesc_list = malloc( sizeof(IIIMCF_object_descriptor*) * num_of_objs) ;
	objs = malloc( sizeof(IIIMCF_downloaded_object) * num_of_objs) ;

	if( ! bin_objdesc_list || ! objs)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " malloc failed.\n");
	#endif
		goto  error ;
	}

	for( i = 0 ; i < num_of_objs; i++)
	{
		if( objdesc_list[i].predefined_id == IIIMP_IMATTRIBUTE_BINARY_GUI_OBJECT)
		{
			bin_objdesc_list[num_of_bin_objs] = &objdesc_list[i] ;
			num_of_bin_objs++ ;
		}
	}

	if( iiimcf_get_downloaded_objects( handle ,
					   num_of_bin_objs ,
					   bin_objdesc_list ,
					   objs) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG "iiimcf_get_downloaded_objects failed\n");
	#endif
		goto  error ;
	}

	for( i = 0 ; i < num_of_bin_objs ; i++)
	{
		const IIIMP_card16 *  obj_name ;
		u_char *  str = NULL ;

		if( iiimcf_get_downloaded_object_filename(
					objs[i] ,
					&obj_name) != IIIMF_STATUS_SUCCESS)
		{
			continue ;
		}

		PARSER_INIT_WITH_BOM( parser_utf16) ;
		im_convert_encoding( parser_utf16, conv ,
				     (u_char*)obj_name, &str,
				     strlen_utf16( obj_name) + 1) ;
		kik_debug_printf( "%d: object_name: %s\n" , i , str) ;
		free( str) ;
		str = NULL ;
	}

	conv->delete( conv);

	return ;

error:
	if( conv)
	{
		conv->delete( conv) ;
	}

	if( bin_objdesc_list)
	{
		free( bin_objdesc_list) ;
	}

	if( objs)
	{
		free( objs) ;
	}
#endif
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
	im_iiimf_t *  iiimf ;

#ifdef  IM_IIIMF_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " index : %d\n" , index) ;
#endif

	/* not implemented yet. */
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

#ifdef  IM_IIIMF_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n");
#endif

	if( iiimcf_get_committed_text( iiimf->context , &iiimcf_text) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Could not get committed text.\n") ;
	#endif
		return ;
	}

	if( iiimcf_get_text_utf16string( iiimcf_text , &utf16str) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Could not get utf16 string.\n") ;
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
	kik_debug_printf( KIK_DEBUG_TAG "\n");
#endif

	if( iiimf->im.preedit.chars)
	{
		(*syms->ml_str_delete)( iiimf->im.preedit.chars ,
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
	u_char *  str ;
	ml_char_t *  p ;
	mkf_char_t  ch ;
	int  is_underline = 0 ;
	int  caret_pos ;
	u_int  count = 0 ;
	int  in_reverse = 0 ;

#ifdef  IM_IIIMF_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n");
#endif

	/*
	 * get preedit string without attribute.
	 */

	if( iiimcf_get_preedit_text( iiimf->context , &iiimcf_text , &caret_pos) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
				 " Could not get preedit text\n") ;
	#endif
		return ;
	}

	if( iiimcf_get_text_utf16string( iiimcf_text , &utf16str) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
				 " Could not get utf16 string\n") ;
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
			(*syms->ml_str_delete)( iiimf->im.preedit.chars ,
					        iiimf->im.preedit.num_of_chars) ;
			iiimf->im.preedit.chars = NULL ;
			iiimf->im.preedit.num_of_chars = 0 ;
			iiimf->im.preedit.filled_len = 0 ;
			iiimf->im.preedit.segment_offset = 0 ;
			iiimf->im.preedit.cursor_offset = X_IM_PREEDIT_NOCURSOR ;
		}

		return  ;
	}

	/* utf16 -> term encoding */
	PARSER_INIT_WITH_BOM( parser_utf16) ;
	im_convert_encoding( parser_utf16 , iiimf->conv ,
			     (u_char*) utf16str , &str ,
			     strlen_utf16( utf16str) + 1) ;

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
		(*syms->ml_str_delete)( iiimf->im.preedit.chars ,
					iiimf->im.preedit.num_of_chars) ;
		iiimf->im.preedit.chars = NULL ;
		iiimf->im.preedit.num_of_chars = 0 ;
		iiimf->im.preedit.filled_len = 0 ;
		iiimf->im.preedit.segment_offset = 0 ;
		iiimf->im.preedit.cursor_offset = X_IM_PREEDIT_NOCURSOR ;
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

	(*syms->ml_str_init)( iiimf->im.preedit.chars ,
			      iiimf->im.preedit.num_of_chars) ;

	/*
	 * IIIMP_card16(utf16) -> ml_char_t
	 */

	p = iiimf->im.preedit.chars ;
	(*syms->ml_str_init)( p , iiimf->im.preedit.num_of_chars) ;

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
			if( (*syms->ml_char_combine)( p - 1 , ch.ch ,
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

		if( (*syms->ml_is_msb_set)( ch.cs))
		{
			SET_MSB( ch.ch[0]) ;
		}

		(*syms->ml_char_set)( p , ch.ch , ch.size , ch.cs ,
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
	kik_debug_printf( KIK_DEBUG_TAG "\n");
#endif

	if( iiimf->im.preedit.chars)
	{
		(*syms->ml_str_delete)( iiimf->im.preedit.chars ,
					iiimf->im.preedit.num_of_chars) ;
		iiimf->im.preedit.chars = NULL ;
		iiimf->im.preedit.num_of_chars = 0 ;
		iiimf->im.preedit.filled_len = 0 ;
		iiimf->im.preedit.segment_offset = 0 ;
		iiimf->im.preedit.cursor_offset = X_IM_PREEDIT_NOCURSOR ;
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
	int  num_per_window ;
	int  row ;
	int  col ;
	int  direction ;

#ifdef  IM_IIIMF_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n");
#endif

	if( iiimcf_get_lookup_choice( iiimf->context , &lookup_choice) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Could not get lookup table\n") ;
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
		kik_warn_printf( KIK_DEBUG_TAG " Could not get lookup table.\n") ;
	#endif
		return ;
	}

	if( iiimcf_get_lookup_choice_configuration(
					lookup_choice ,
					&num_per_window ,
					&row ,
					&col ,
					&direction) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Could not get lookup information.\n") ;
	#endif
		return ;
	}

#ifdef  IM_IIIMF_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "size:          %d\n" , size) ;
	kik_debug_printf( KIK_DEBUG_TAG "index_first:   %d\n" , index_first) ;
	kik_debug_printf( KIK_DEBUG_TAG "index_last:    %d\n" , index_last) ;
	kik_debug_printf( KIK_DEBUG_TAG "index_current: %d\n" , index_current) ;
	kik_debug_printf( KIK_DEBUG_TAG "num_per_window:%d\n" , num_per_window);
	kik_debug_printf( KIK_DEBUG_TAG "row:           %d\n" , row) ;
	kik_debug_printf( KIK_DEBUG_TAG "col:           %d\n" , col) ;
#endif

	if( index_first == 0 && index_last == 0)
	{
		return ;
	}

	(*iiimf->im.listener->get_spot)(
				iiimf->im.listener->self ,
				iiimf->im.preedit.chars ,
				iiimf->im.preedit.segment_offset ,
				&x , &y) ;

	if( ! iiimf->im.cand_screen)
	{
		int  is_vertical_direction = 0 ;

		if( direction == IIIMCF_LOOKUP_CHOICE_VERTICAL_DIRECTION)
		{
			is_vertical_direction = 1 ;
		}

		if( ! ( iiimf->im.cand_screen = (*syms->x_im_candidate_screen_new)(
				(*iiimf->im.listener->get_win_man)(iiimf->im.listener->self) ,
				(*iiimf->im.listener->get_font_man)(iiimf->im.listener->self) ,
				(*iiimf->im.listener->get_color_man)(iiimf->im.listener->self) ,
				(*iiimf->im.listener->is_vertical)(iiimf->im.listener->self) ,
				is_vertical_direction ,
				(*iiimf->im.listener->get_line_height)(iiimf->im.listener->self) ,
				x , y)))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " x_im_candidate_screen_new() failed.\n") ;
		#endif
			
			return ;
		}

		iiimf->im.cand_screen->listener.self = iiimf ;
		iiimf->im.cand_screen->listener.selected = candidate_selected ;
	}

	if( ! (*iiimf->im.cand_screen->init)( iiimf->im.cand_screen , size ,
				              num_per_window))
	{
		(*iiimf->im.cand_screen->delete)( iiimf->im.cand_screen) ;
		iiimf->im.cand_screen = NULL ;
		return ;
	}

	(*iiimf->im.cand_screen->set_spot)( iiimf->im.cand_screen , x , y) ;

	for( i = index_first ; i <= index_last ; i++)
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
			kik_warn_printf( KIK_DEBUG_TAG " Could not candidate item\n") ;
		#endif
			continue ;
		}

		if( iiimcf_get_text_utf16string(
					iiimcf_text_cand ,
					&utf16str) != IIIMF_STATUS_SUCCESS)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " Could not get utf16 string\n") ;
		#endif
			continue ;
		}

		PARSER_INIT_WITH_BOM( parser_utf16) ;
		if( im_convert_encoding( parser_utf16 , iiimf->conv ,
					 (u_char*)utf16str , &str ,
					 strlen_utf16( utf16str) + 1))
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

#ifdef  IM_IIIMF_ATOKX_LOOKUP_HACK
static void
atokx_lookup_show(
	im_iiimf_t *  iiimf
	)
{
}

static void
atokx_lookup_hide(
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
atokx_lookup_select(
	im_iiimf_t *  iiimf ,
	int  index
	)
{
	(*iiimf->im.cand_screen->select)( iiimf->im.cand_screen , index) ;
}

static void
atokx_lookup_set(
	im_iiimf_t *  iiimf ,
	const IIIMP_card16 **  candidates ,
	int  num_of_candidate ,
	int  num_per_window ,
	int  index
	)
{
	u_char *  str = NULL ;
	int  top ;
	int  x ;
	int  y ;
	int  i ;

	(*iiimf->im.listener->get_spot)(
				iiimf->im.listener->self ,
				iiimf->im.preedit.chars ,
				iiimf->im.preedit.segment_offset ,
				&x , &y) ;

	if( ! iiimf->im.cand_screen)
	{
		if( ! ( iiimf->im.cand_screen = (*syms->x_im_candidate_screen_new)(
				(*iiimf->im.listener->get_win_man)(iiimf->im.listener->self) ,
				(*iiimf->im.listener->get_font_man)(iiimf->im.listener->self) ,
				(*iiimf->im.listener->get_color_man)(iiimf->im.listener->self) ,
				(*iiimf->im.listener->is_vertical)(iiimf->im.listener->self) ,
				1 ,
				(*iiimf->im.listener->get_line_height)(iiimf->im.listener->self) ,
				x , y)))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " x_im_candidate_screen_new() failed.\n") ;
		#endif
			
			return ;
		}

		iiimf->im.cand_screen->listener.self = iiimf ;
		iiimf->im.cand_screen->listener.selected = candidate_selected ;
	}

	if( ! (*iiimf->im.cand_screen->init)( iiimf->im.cand_screen ,
					      num_of_candidate ,
					      num_per_window))
	{
		(*iiimf->im.cand_screen->delete)( iiimf->im.cand_screen) ;
		iiimf->im.cand_screen = NULL ;
		return ;
	}

	top = (index / num_per_window) * num_per_window ;

	for( i = 1 ; i < num_per_window + 1 ; i++)
	{
		PARSER_INIT_WITH_BOM( parser_utf16) ;
		if( im_convert_encoding( parser_utf16 , iiimf->conv ,
					 (u_char*)candidates[i] , &str ,
					 strlen_utf16( candidates[i]) + 1))
		{
			(*iiimf->im.cand_screen->set)(
						iiimf->im.cand_screen ,
						iiimf->parser_term ,
						str , i - 1 + top) ;
			free( str) ;
		}
	}

	(*iiimf->im.cand_screen->select)( iiimf->im.cand_screen , index) ;
}

#define ATOKX_LOOKUP_SHOW	0x0a02
#define ATOKX_LOOKUP_HIDE	0x0a03
#define ATOKX_LOOKUP_SELECT	0x0a04
#define ATOKX_LOOKUP_SET	0x0a05

static void
atokx_lookup(
	im_iiimf_t *  iiimf ,
	IIIMCF_event  event
	)
{
	const IIIMP_card16 *  aux_name ;
	IIIMP_card32  class_index ;
	const IIIMP_card32 *  array_val_int ;
	const IIIMP_card16 **  array_val_str ;
	int  num_of_int ;
	int  num_of_str ;
	int  i ;
	u_char *  str = NULL ;

	if( iiimcf_get_aux_event_value( event ,
					&aux_name ,
					&class_index ,
					&num_of_int ,
					&array_val_int ,
					&num_of_str ,
					&array_val_str) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " iiimcf_get_aux_event_value() failed\n");
	#endif
		return ;
	}

	if( ! num_of_int)
	{
		return ;
	}

	PARSER_INIT_WITH_BOM( parser_utf16) ;
	im_convert_encoding( parser_utf16, iiimf->conv ,
			     (u_char*)aux_name, &str,
			     strlen_utf16( aux_name) + 1) ;
	if( strcmp( str , "jp.co.justsystem.atok12.LookupAux"))
	{
		free( str) ;
		return ;
	}

	free( str) ;

	switch( array_val_int[0])
	{
	case ATOKX_LOOKUP_SHOW:
		atokx_lookup_show( iiimf) ;
		break;
	case ATOKX_LOOKUP_HIDE:
		atokx_lookup_hide( iiimf) ;
		break;
	case ATOKX_LOOKUP_SELECT:
		atokx_lookup_select( iiimf, array_val_int[1] - 1) ;
		break;
	case ATOKX_LOOKUP_SET:
		atokx_lookup_set( iiimf ,
				  array_val_str ,
				  array_val_int[2] ,
				  array_val_int[1] ,
				  array_val_int[3] - 1) ;
		break;
	}
}

#endif /* IM_IIIMF_ATOKX_LOOKUP_HACK */

static void
aux_dump(
	char *  tag ,
	im_iiimf_t *  iiimf ,
	IIIMCF_event  event
	)
{
#ifdef  IM_IIIMF_DEBUG
	const IIIMP_card16 *  aux_name ;
	IIIMP_card32  class_index ;
	const IIIMP_card32 *  array_val_int ;
	const IIIMP_card16 **  array_val_str ;
	int  num_of_int ;
	int  num_of_str ;
	int  i ;
	u_char *  str = NULL ;

	kik_debug_printf( "%s\n", tag) ;

	if( iiimcf_get_aux_event_value( event ,
					&aux_name ,
					&class_index ,
					&num_of_int ,
					&array_val_int ,
					&num_of_str ,
					&array_val_str) != IIIMF_STATUS_SUCCESS)
	{
		/* XXX */
		kik_warn_printf("iiimcf_get_aux_event_value() failed\n");
		return ;
	}

	PARSER_INIT_WITH_BOM( parser_utf16) ;
	im_convert_encoding( parser_utf16, iiimf->conv ,
			     (u_char*)aux_name, &str,
			     strlen_utf16( aux_name) + 1) ;
	kik_debug_printf( "aux_name: %s\n" , str) ;
	free( str) ;
	str = NULL ;

	kik_debug_printf( "class_index: %d, int: %d, str: %d\n",
			  class_index , num_of_int, num_of_str);

	for( i = 0 ; i < num_of_int ; i++)
	{
		kik_debug_printf( "int[%d]: %d\n", i, array_val_int[i]);
	}
	for( i = 0 ; i < num_of_str ; i++)
	{
		PARSER_INIT_WITH_BOM( parser_utf16) ;
		im_convert_encoding( parser_utf16, iiimf->conv ,
				     (u_char*)array_val_str[i], &str,
				     strlen_utf16( array_val_str[i]) + 1) ;
		kik_debug_printf( "str[%d]: %s\n" , i , str) ;
		free( str) ;
	}
#endif
}

static void
status_start(
	im_iiimf_t *  iiimf
	)
{
}

static void
status_change(
	im_iiimf_t *  iiimf
	)
{
	IIIMCF_text  iiimcf_text ;
	const IIIMP_card16  *utf16str ;
	u_char *  str ;
	int  x ;
	int  y ;
	int  on ;

#ifdef  IM_IIIMF_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n");
#endif

	if( iiimcf_get_current_conversion_mode( iiimf->context , &on) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Could not get the current mode.\n") ;
	#endif
		return ;
	}

	if( iiimcf_get_status_text( iiimf->context , &iiimcf_text) != IIIMF_STATUS_SUCCESS || ! on)
	{
		if( iiimf->im.stat_screen)
		{
			(*iiimf->im.stat_screen->delete)( iiimf->im.stat_screen) ;
			iiimf->im.stat_screen = NULL ;
		}

		return ;
	}

	(*iiimf->im.listener->get_spot)( iiimf->im.listener->self ,
					 iiimf->im.preedit.chars ,
					 iiimf->im.preedit.segment_offset ,
					 &x , &y) ;

	if( iiimf->im.stat_screen == NULL)
	{
		if( ! ( iiimf->im.stat_screen = (*syms->x_im_status_screen_new)(

				(*iiimf->im.listener->get_win_man)(iiimf->im.listener->self) ,
				(*iiimf->im.listener->get_font_man)(iiimf->im.listener->self) ,
				(*iiimf->im.listener->get_color_man)(iiimf->im.listener->self) ,
				(*iiimf->im.listener->is_vertical)(iiimf->im.listener->self) ,
				x , y)))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " x_im_satus_screen_new() failed.\n") ;
		#endif
			
			return ;
		}
	}

	if( iiimcf_get_text_utf16string( iiimcf_text , &utf16str) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Could not get utf16 string.\n") ;
	#endif
		return ;
	}

	PARSER_INIT_WITH_BOM( parser_utf16) ;
	if( im_convert_encoding( parser_utf16 , iiimf->conv ,
				 (u_char*)utf16str , &str ,
				 strlen_utf16( utf16str) + 1))
	{
		(*iiimf->im.stat_screen->set)( iiimf->im.stat_screen ,
					       iiimf->parser_term ,
					       str) ;
		free( str) ;
	}
}

static void
status_done(
	im_iiimf_t *  iiimf
	)
{
	if( iiimf->im.stat_screen)
	{
		(*iiimf->im.stat_screen->delete)( iiimf->im.stat_screen) ;
		iiimf->im.stat_screen = NULL ;
	}
}


static void
dispatch(
	im_iiimf_t *  iiimf ,
	IIIMCF_event  event ,
	IIIMCF_event_type  event_type
	)
{
	switch( event_type)
	{
	case IIIMCF_EVENT_TYPE_DESTROY:
	case IIIMCF_EVENT_TYPE_RESET:
	case IIIMCF_EVENT_TYPE_EVENTLIKE:
/*	case IIIMCF_EVENT_TYPE_KEYEVENT: */
	case IIIMCF_EVENT_TYPE_TRIGGER_NOTIFY:
	case IIIMCF_EVENT_TYPE_OPERATION:
	case IIIMCF_EVENT_TYPE_SETICFOCUS:
	case IIIMCF_EVENT_TYPE_UNSETICFOCUS:
#ifdef  HAVE_HOTKEY_NOTFY_EVENT
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
		status_start( iiimf) ;
		break ;
	case IIIMCF_EVENT_TYPE_UI_STATUS_CHANGE:
		status_change( iiimf) ;
		break ;
	case IIIMCF_EVENT_TYPE_UI_STATUS_DONE:
		status_done( iiimf) ;
		break ;
	case IIIMCF_EVENT_TYPE_UI_COMMIT:
		commit( iiimf);
		break;
	case IIIMCF_EVENT_TYPE_AUX_START:
		aux_dump( "start" , iiimf, event);
		break;
	case IIIMCF_EVENT_TYPE_AUX_DRAW:
		aux_dump( "draw" , iiimf, event);
	#ifdef  IM_IIIMF_ATOKX_LOOKUP_HACK
		atokx_lookup( iiimf , event) ;
	#endif
		break;
	case IIIMCF_EVENT_TYPE_AUX_SETVALUES:
		aux_dump( "setvalues" , iiimf, event);
		break ;
	case IIIMCF_EVENT_TYPE_AUX_DONE:
		aux_dump( "done" , iiimf, event);
		break;
#ifdef  HAVE_AUX_GETVALUES_EVENT
	case IIIMCF_EVENT_TYPE_AUX_GETVALUES:
		aux_dump( "getvalues" , iiimf, event);
		break;
#endif
	default:
		break ;
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

	if( iiimf->context)
	{
		iiimcf_destroy_context( iiimf->context) ;
	}

	free( iiimf) ;

	ref_count-- ;

#ifdef  IM_IIIMF_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " An object was deleted. ref_count is %d\n", ref_count) ;
#endif

	if( ref_count == 0 && initialized)
	{
		iiimcf_destroy_handle( handle) ;

		handle = NULL ;

		iiimcf_finalize() ;

		if( parser_utf16)
		{
			(*parser_utf16->delete)( parser_utf16) ;
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
	XKeyEvent *  xevent
	)
{
	im_iiimf_t *  iiimf ;
	IIIMCF_keyevent  key ;
	IIIMCF_event  event ;
	IIIMF_status  status ;
	int  ret ;

	int  is_shift ;
	int  is_lock ;
	int  is_ctl ;
	int  is_alt ;
	int  is_meta ;
	int  is_super ;
	int  is_hyper ;

	iiimf = (im_iiimf_t*) im ;

	key.keycode = 0 ;
	key.keychar = 0 ;
	key.modifier = 0 ;

	(*iiimf->im.listener->compare_key_state_with_modmap)(
						iiimf->im.listener->self ,
						xevent->state ,
						&is_shift , &is_lock , &is_ctl ,
						&is_alt , &is_meta , &is_super ,
						&is_hyper) ;

	if( is_shift) key.modifier |= IIIMF_SHIFT_MODIFIER ;
	if( is_ctl)   key.modifier |= IIIMF_CONTROL_MODIFIER ;
	if( is_alt)   key.modifier |= IIIMF_ALT_MODIFIER ;
	if( is_meta)  key.modifier |= IIIMF_META_MODIFIER ;

	xksym_to_iiimfkey( ksym , &key.keychar , &key.keycode) ;

	key.time_stamp = xevent->time ;

	if( iiimcf_create_keyevent( &key , &event) == IIIMF_STATUS_SUCCESS)
	{
		status = iiimcf_forward_event( iiimf->context , event) ;

		switch( status)
		{
		case IIIMF_STATUS_SUCCESS:
			goto  dispatch ;
		case IIIMF_STATUS_EVENT_NOT_FORWARDED:
			break ;
		case IIIMF_STATUS_IC_INVALID:
			kik_error_printf( "Invalid IIIMCF context\n");
			break ;
		case IIIMF_STATUS_MALLOC:
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " iiimcf internal error "
					 "[IIIMF_STATUS_MALLOC]\n");
		#endif
			break ;
		default:
			kik_error_printf( "Could not send key event to IIIMSF\n");
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
			kik_warn_printf( KIK_DEBUG_TAG " Could not get event type\n");
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
			dispatch( iiimf , received_event, type) ;
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
			kik_warn_printf( KIK_DEBUG_TAG " Could not dispatch event\n");
		#endif
		}

		if( iiimcf_ignore_event( received_event) != IIIMF_STATUS_SUCCESS)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " Could not dispatch event\n");
		#endif
		}
	}

	return  ret ;
}

static int
switch_mode(
	x_im_t *  im
	)
{
#if  0
	XKeyEvent  event ;

	event.state = ControlMask ;
	event.time = 0 ;	/* XXX */

	if( ! (key_event( im , 0x20 , XK_space , &event)))
	{
		return  1 ;
	}

	return  0 ;
#endif
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
			kik_error_printf( "Could not forward focus event to IIIMS [status = %d]\n", status) ;
		}
	}

	if( iiimf->im.stat_screen)
	{
		(*iiimf->im.stat_screen->show)( iiimf->im.stat_screen) ;
	}

	if( iiimf->im.cand_screen)
	{
		(*iiimf->im.cand_screen->show)( iiimf->im.cand_screen) ;
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
			kik_error_printf( "Could not forward unfocus event to IIIMS [status = %d]\n", status) ;
		}
	}

	if( iiimf->im.stat_screen)
	{
		(*iiimf->im.stat_screen->hide)( iiimf->im.stat_screen) ;
	}

	if( iiimf->im.cand_screen)
	{
		(*iiimf->im.cand_screen->hide)( iiimf->im.cand_screen) ;
	}
}


/* --- global functions --- */

x_im_t *
im_iiimf_new(
	u_int64_t  magic ,
	ml_char_encoding_t  term_encoding ,
	x_im_export_syms_t *  export_syms ,
	char *  lang ,	/* <language id(rfc1766)>:<language engine> */
	u_int  mod_ignore_mask
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
			kik_warn_printf( KIK_DEBUG_TAG " Could not initialize\n") ;
		#endif
			return  NULL ;
		}

		initialized = 1 ;

		if( iiimcf_create_attr( &attr) != IIIMF_STATUS_SUCCESS)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " Could not create attribute\n") ;
		#endif
			goto  error ;
		}

		if( iiimcf_attr_put_string_value( attr ,
						  IIIMCF_ATTR_CLIENT_TYPE ,
						  "IIIMF plugin for mlterm")
							!= IIIMF_STATUS_SUCCESS)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " Could not append a string to the attribute\n") ;
		#endif
			goto  error ;
		}

		if( iiimcf_create_handle( attr , &handle) != IIIMF_STATUS_SUCCESS)
		{
			kik_error_printf( "Could not create handle for IIIMF\n") ;
			goto  error ;
		}

		iiimcf_destroy_attr( attr) ;
		attr = NULL ;

		if( ! ( parser_utf16 = mkf_utf16_parser_new()))
		{
			goto  error ;
		}

		show_iiimcf_version();

		syms = export_syms ;

		show_available_gui_objects() ;
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
	iiimf->im.switch_mode = switch_mode ;
	iiimf->im.focused = focused ;
	iiimf->im.unfocused = unfocused ;

	if( ! ( iiimf->parser_term = (*syms->ml_parser_new)( term_encoding)))
	{
		goto  error ;
	}

	if( ! ( iiimf->conv = (*syms->ml_conv_new)( term_encoding)))
	{
		goto  error ;
	}

	if( iiimcf_create_attr( &attr) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Could not create attribute\n") ;
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
			kik_warn_printf( KIK_DEBUG_TAG " Could not append value to the attribute\n") ;
		#endif
		}
	}

	if( language_engine)
	{
		if( iiimcf_attr_put_ptr_value(
				attr ,
				IIIMCF_ATTR_INPUT_METHOD ,
				language_engine) != IIIMF_STATUS_SUCCESS)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " Could not append value to the attribute\n") ;
		#endif
		}
	}

	if( iiimcf_create_context( handle ,
				   attr ,
				   &iiimf->context) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Could not create context\n") ;
	#endif
		goto  error ;
	}

	iiimcf_destroy_attr( attr) ;
	attr = NULL ;

	ref_count++ ;

#ifdef  IM_IIIMF_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " New object was created. ref_count is %d.\n" , ref_count);
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


/* --- API for external tools --- */

im_info_t *
im_iiimf_get_info(
	char *  locale ,
	char *  encoding
	)
{
	im_info_t *  result = NULL ;
	IIIMCF_input_method *  input_methods ;
	IIIMCF_language *  langs ;
	mkf_conv_t *  conv = NULL ;
	int  num_of_ims ;
	int  num_of_langs ;
	int  total = 0 ;
	int  idx ;
	int  auto_idx = 0 ;
	int  i ;
	int  j ;

	if( iiimcf_initialize(IIIMCF_ATTR_NULL) != IIIMF_STATUS_SUCCESS)
	{
		return  NULL ;
	}

	if ( iiimcf_create_handle( IIIMCF_ATTR_NULL,
				   &handle) != IIIMF_STATUS_SUCCESS)
	{
		iiimcf_finalize() ;
		return  NULL ;
	}

	if( iiimcf_get_supported_input_methods(
				handle,
				&num_of_ims,
				&input_methods) != IIIMF_STATUS_SUCCESS)
	{
		goto  error ;
	}

	for( i = 0 ; i < num_of_ims ; i++)
	{
		if( iiimcf_get_input_method_languages(
					input_methods[i] ,
					&num_of_langs ,
					&langs) != IIIMF_STATUS_SUCCESS)
		{
			goto  error ;
		}

		total += num_of_langs ;
	}

	if( ! ( parser_utf16 = mkf_utf16_parser_new()))
	{
		goto  error ;
	}

	if( ! ( conv = mkf_iso8859_1_conv_new()))
	{
		goto  error ;
	}

	if( ! ( result = malloc( sizeof( im_info_t))))
	{
		goto  error ;
	}

	result->id = strdup( "iiimf") ;
	result->name = strdup( "IIIMF") ;
	result->num_of_args = total + 1;
	result->args = NULL ;
	result->readable_args = NULL ;

	if( ! ( result->args = malloc( sizeof(char*) * result->num_of_args)))
	{
		goto  error ;
	}

	if( ! ( result->readable_args = malloc( sizeof(char*) * result->num_of_args)))
	{
		goto  error ;
	}

	idx = 1 ;

	for( i = 0 ; i < num_of_ims ; i++)
	{
		const  IIIMP_card16 *  im_id ;
		const  IIIMP_card16 *  im_hrn ;
		const  IIIMP_card16 *  im_domain ;
		char *  im ;

		if( iiimcf_get_input_method_desc(
					input_methods[i],
					&im_id,
					&im_hrn,
					&im_domain) != IIIMF_STATUS_SUCCESS)
		{
			continue ;
		}

		if( iiimcf_get_input_method_languages(
					input_methods[i] ,
					&num_of_langs ,
					&langs) != IIIMF_STATUS_SUCCESS)
		{
			continue ;
		}

		PARSER_INIT_WITH_BOM( parser_utf16) ;
		im_convert_encoding( parser_utf16 , conv , (u_char*)im_id  ,
				     (u_char**)&im , strlen_utf16( im_id) + 1) ;

		for( j = 0 ; j < num_of_langs ; j++)
		{
			const char *  lang_id ;
			size_t  len ;

			iiimcf_get_language_id(langs[j], &lang_id);

			if( strncmp( lang_id , locale , 2) == 0 &&
			    auto_idx == 0)
			{
				auto_idx = idx ;
			}

			len = strlen( im) + strlen( lang_id) + 4 ;

			if( ( result->args[idx] = malloc(len)))
			{
				kik_snprintf( result->args[idx] , len ,
					      "%s:%s" , lang_id , im) ;
			}
			else
			{
				result->args[idx] = strdup( "error") ;
			}

			if( ( result->readable_args[idx] = malloc(len)))
			{
				kik_snprintf( result->readable_args[idx] , len ,
					      "%s (%s)" , lang_id , im) ;
			}
			else
			{
				result->readable_args[i] = strdup( "error") ;
			}

			idx ++ ;
		}

		free( im) ;
	}

	result->args[0] = strdup( "") ;
	if( auto_idx)
	{
		result->readable_args[0] = strdup( result->readable_args[auto_idx]) ;
	}
	else
	{
		result->readable_args[0] = strdup( "unknown") ;
	}

	if( total != idx - 1)
	{
		free( result->id) ;
		free( result->name) ;

		for( i = 0 ; i < idx - 1; i++)
		{
			free( result->args[i]) ;
			free( result->readable_args[i]) ;
		}

		free( result->args) ;
		free( result->readable_args) ;

		free( result) ;

		result = NULL ;
	}

	iiimcf_destroy_handle(handle);

	iiimcf_finalize() ;

	return  result ;

error:

	if( parser_utf16)
	{
		(*parser_utf16->delete)( parser_utf16) ;
	}

	if( conv)
	{
		(*conv->delete)( conv) ;
	}

	if( handle)
	{
		iiimcf_destroy_handle(handle);
	}

	iiimcf_finalize() ;

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

