/*
 * im_m17nlib.c - input method plugin using m17n library
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

#include  <X11/keysym.h>	/* XK_xxx */
#include  <kiklib/kik_mem.h>	/* malloc/alloca/free */
#include  <kiklib/kik_str.h>	/* kik_str_alloca_dup kik_snprintf kik_str_sep*/
#include  <kiklib/kik_locale.h>	/* kik_get_lang */

#include  <m17n.h>
#include  <m17n-misc.h>		/* merror_code */

#include  <x_im.h>
#include  "../im_common.h"
#include  "../im_info.h"

#if  0
#define  IM_M17NLIB_DEBUG 1
#endif

#define  MAX_BYTES_PER_CHAR	4	/* FIXME */
#define  MAX_BYTES_ESC_SEQUEACE	5	/* FIXME */

#define  MAX_BYTES(n) (((n) * MAX_BYTES_PER_CHAR) + MAX_BYTES_ESC_SEQUEACE + 1)

typedef struct im_m17nlib
{
	/* input method common object */
	x_im_t  im ;

	MInputMethod *  input_method ;
	MInputContext *  input_context ;

	MConverter *   mconverter ;	/* MText -> u_char    */
	mkf_parser_t *  parser_term ;	/* for term encoding  */
	mkf_conv_t *  conv ;

}  im_m17nlib_t ;


/* --- static variables --- */

static int  ref_count = 0 ;
static int  initialized = 0 ;
static mkf_parser_t *  parser_ascii = NULL ;
/* mlterm internal symbols */
static x_im_export_syms_t *  syms = NULL ;


/* --- static functions --- */

#ifdef  IM_M17NLIB_DEBUG
static void
show_available_ims( void)
{
	MPlist *  im_list ;
	MSymbol  sym_im ;
	int  num_of_ims ;
	int  i ;

	sym_im = msymbol( "input-method") ;

	im_list = mdatabase_list( sym_im , Mnil , Mnil , Mnil) ;

	num_of_ims = mplist_length( im_list) ;

	for( i = 0 ; i < num_of_ims ; i++, im_list = mplist_next( im_list)) {
		MDatabase *  db ;
		MSymbol *  tag ;
		
		db = mplist_value( im_list) ;
		tag = mdatabase_tag( db) ;

		printf( "%d: %s(%s)\n", i,
			msymbol_name( tag[2]) ,
			msymbol_name( tag[1])) ;
	}

	m17n_object_unref( im_list);
}
#endif

static MSymbol
xksym_to_msymbol(
	im_m17nlib_t *  m17nlib ,
	KeySym  ksym ,
	u_int  state
	)
{
	char  mod[13] = "";
	char *  key ;
	char *  str ;
	int  filled_len = 0 ;
	size_t  len ;

	int  is_shift ;
	int  is_lock ;
	int  is_ctl ;
	int  is_alt ;
	int  is_meta ;
	int  is_super ;
	int  is_hyper ;

	if( XK_Shift_L <= ksym && ksym <= XK_Hyper_R)
	{
		return  Mnil ;
	}

	(*m17nlib->im.listener->compare_key_state_with_modmap)(
						m17nlib->im.listener->self ,
						state ,
						&is_shift , &is_lock , &is_ctl ,
						&is_alt , &is_meta , &is_super ,
						&is_hyper) ;

	/* Latin 1 */
	if( XK_space <= ksym && ksym <= XK_asciitilde)
	{
		char  buf[2] = " ";
		buf[0] = ksym ;

		if( is_shift && ( 'a' <= buf[0] && buf[0] <= 'z'))
		{
			buf[0] += ( 'A' - 'a') ;
			is_shift = 0 ;
		}

		return  msymbol( buf) ;
	}

	if( is_shift)
		filled_len += kik_snprintf( &mod[filled_len] ,
				    sizeof(mod) - filled_len ,
				    "S-") ;
	if( is_ctl)
		filled_len += kik_snprintf( &mod[filled_len] ,
					    sizeof(mod) - filled_len ,
					    "C-") ;
	if( is_alt)
		filled_len += kik_snprintf( &mod[filled_len] ,
					    sizeof(mod) - filled_len ,
					    "A-") ;
	if( is_meta)
		filled_len += kik_snprintf( &mod[filled_len] ,
					    sizeof(mod) - filled_len ,
					    "M-") ;
	if( is_super)
		filled_len += kik_snprintf( &mod[filled_len] ,
					    sizeof(mod) - filled_len ,
					    "s-") ;
	if( is_hyper)
		filled_len += kik_snprintf( &mod[filled_len] ,
					    sizeof(mod) - filled_len ,
					    "H-") ;

	if( ! ( key = XKeysymToString( ksym)))
	{
		return  Mnil ;
	}

	len = strlen( mod) + strlen(key) + 1 ;

	if( ! ( str = alloca( len)))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca failed\n") ;
	#endif

		return  Mnil ;
	}

	kik_snprintf( str , len , "%s%s" , mod , key) ;

	return  msymbol( str) ;
}

static MInputMethod *
find_input_method(
	char *  param
	)
{
	char *  lang = NULL ;
	char *  im_name = NULL ;
	MPlist *  im_list ;
	MInputMethod *  result = NULL;
	int  found = 0 ;
	int  num_of_ims ;
	int  i ;

	if( param)
	{
		lang = kik_str_alloca_dup( param) ;
		if( strstr( lang , ":"))
		{
			im_name = lang ;
			lang = kik_str_sep( &im_name , ":") ;
		}

		if( lang && strcmp( lang , "") == 0)
		{
			lang = NULL ;
		}

		if( im_name && strcmp( im_name , "") == 0)
		{
			im_name = NULL ;
		}
	}

	if( lang == NULL && im_name == NULL)
	{
		lang = kik_get_lang() ;
	}

	if( ! ( im_list = mdatabase_list( msymbol( "input-method") ,
					  Mnil , Mnil , Mnil)))
	{
		kik_error_printf( "There are no available input methods.\n") ;
		return  0 ;
	}

	num_of_ims = mplist_length( im_list) ;

	for( i = 0 ; i < num_of_ims ; i++ , im_list = mplist_next( im_list))
	{
		MDatabase *  db ;
		MSymbol *  tag ;

		db = mplist_value( im_list) ;
		tag = mdatabase_tag( db) ;

		if( tag[1] == Mnil)
		{
			continue ;
		}

		if( lang && im_name)
		{
			if( strcmp( lang , msymbol_name( tag[1])) == 0 &&
			    strcmp( im_name , msymbol_name( tag[2])) == 0)
			{
				found = 1 ;
			}
		}
		else if( lang)
		{
			if( strcmp( lang , msymbol_name( tag[1])) == 0)
			{
				found = 1 ;
			}
		}
		else if( im_name)
		{
			if( strcmp( im_name , msymbol_name( tag[2])) == 0)
			{
				found = 1 ;
			}
		}

		if( found)
		{
		#ifdef  IM_M17NLIB_DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " found. language: %s, im_name: %s\n" , msymbol_name( tag[1]) , msymbol_name( tag[2])) ;
		#endif
			result = minput_open_im( tag[1] , tag[2] , NULL) ;
			break ;
		}
	}

	m17n_object_unref( im_list);

	return  result ;
}

static void
commit(
	im_m17nlib_t *  m17nlib ,
	MText *  text
	)
{
	u_char *  buf = NULL ;
	u_int  num_of_chars ;
	int  filled_len ;

	if( ( num_of_chars = mtext_len( text)))
	{
		if( ! ( buf = alloca( MAX_BYTES(num_of_chars))))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG , " alloca failed\n") ;
		#endif
		}
	}

	if( buf)
	{
		mconv_reset_converter( m17nlib->mconverter) ;
		mconv_rebind_buffer(
				m17nlib->mconverter , buf ,
				MAX_BYTES(num_of_chars)) ;
		filled_len = mconv_encode( m17nlib->mconverter , text) ;

		if( filled_len == -1)
		{
			kik_error_printf( "Could not convert the encoding of committed characters. [error code: %d]\n" , merror_code) ;
		}
		else
		{
			(*m17nlib->im.listener->write_to_term)(
					m17nlib->im.listener->self ,
					buf , filled_len) ;
		}
	}
}

static void
set_candidate(
	im_m17nlib_t *  m17nlib ,
	MText *  candidate ,
	int  idx
	)
{
	u_char *  buf ;
	u_int  num_of_chars ;
	u_int  filled_len ;

	if( ! ( num_of_chars = mtext_len( candidate)))
	{
		return ;
	}

	if( ! ( buf = alloca( MAX_BYTES(num_of_chars))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG , " alloca failed\n") ;
	#endif
		return ;
	}

	mconv_reset_converter( m17nlib->mconverter) ;
	mconv_rebind_buffer( m17nlib->mconverter , buf ,
			     MAX_BYTES(num_of_chars)) ;
	filled_len = mconv_encode( m17nlib->mconverter , candidate) ;

	if( filled_len == -1)
	{
		kik_error_printf( "Could not convert the encoding of characters in candidates. [error code: %d]\n" , merror_code) ;
		return ;
	}

	buf[filled_len] = '\0' ;
	(*m17nlib->im.cand_screen->set)( m17nlib->im.cand_screen ,
					 m17nlib->parser_term ,
					 buf , idx) ;
}

static void
preedit_changed(
	im_m17nlib_t *  m17nlib
	)
{
	int  filled_len ;
	u_char *  buf ;
	u_int  num_of_chars ;
	ml_char_t *  p ;
	mkf_char_t  ch ;
	u_int  pos = 0 ;

#ifdef  IM_M17NLIB_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	/*
	 * clear saved preedit
	 */

	if( m17nlib->im.preedit.chars)
	{
		(*syms->ml_str_delete)( m17nlib->im.preedit.chars ,
					m17nlib->im.preedit.num_of_chars) ;
		m17nlib->im.preedit.chars = NULL ;
	}

	m17nlib->im.preedit.num_of_chars = 0 ;
	m17nlib->im.preedit.filled_len = 0 ;
	m17nlib->im.preedit.segment_offset = 0 ;
	m17nlib->im.preedit.cursor_offset = X_IM_PREEDIT_NOCURSOR ;


	/*
	 * MText -> u_char
	 */

	num_of_chars = mtext_len( m17nlib->input_context->preedit) ;

	if( ! num_of_chars)
	{
		goto  draw ;
	}

	if( ! ( buf = alloca( MAX_BYTES(num_of_chars))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG , " alloca failed\n") ;
	#endif
		return ;
	}

	mconv_reset_converter( m17nlib->mconverter) ;
	mconv_rebind_buffer( m17nlib->mconverter , buf ,
			     MAX_BYTES(num_of_chars)) ;
	filled_len = mconv_encode( m17nlib->mconverter ,
				   m17nlib->input_context->preedit) ;

	if( filled_len == -1)
	{
		kik_error_printf( "Could not convert the preedit string to terminal encoding. [%d]\n" , merror_code) ;
		return ;
	}


	/*
	 * allocate im.preedit.chars
	 */

	if( ! ( m17nlib->im.preedit.chars = calloc( num_of_chars , sizeof(ml_char_t))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG , " calloc failed\n") ;
	#endif
		return ;
	}
	m17nlib->im.preedit.num_of_chars = num_of_chars ;


	/*
	 * u_char -> ml_char_t
	 */

	p = m17nlib->im.preedit.chars ;
	(*syms->ml_str_init)( p , m17nlib->im.preedit.num_of_chars);

	(*m17nlib->parser_term->init)( m17nlib->parser_term) ;
	(*m17nlib->parser_term->set_str)( m17nlib->parser_term ,
					  (u_char*) buf ,
					  filled_len) ;

	m17nlib->im.preedit.segment_offset = m17nlib->input_context->candidate_from ;
	m17nlib->im.preedit.cursor_offset = m17nlib->input_context->cursor_pos ;

	while( (*m17nlib->parser_term->next_char)( m17nlib->parser_term , &ch))
	{
		ml_color_t  fg_color = ML_FG_COLOR ;
		ml_color_t  bg_color = ML_BG_COLOR ;
		int  is_underline = 1 ;
		int  is_biwidth = 0 ;
		int  is_comb = 0 ;

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

		if( m17nlib->input_context->candidate_list &&
		    m17nlib->input_context->candidate_from <= pos &&
		    m17nlib->input_context->candidate_to > pos)
		{
			fg_color = ML_BG_COLOR ;
			bg_color = ML_FG_COLOR ;
			is_underline = 0 ;
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
						      0 , is_underline))
			{
				pos++ ;
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
				      0 , is_underline) ;

		pos++ ;
		p++ ;
		m17nlib->im.preedit.filled_len++ ;
	}

draw:

	(*m17nlib->im.listener->draw_preedit_str)( m17nlib->im.listener->self ,
						   m17nlib->im.preedit.chars ,
						   m17nlib->im.preedit.filled_len ,
						   m17nlib->im.preedit.cursor_offset) ;
}

static void
candidates_changed(
	im_m17nlib_t *  m17nlib
	)
{
	MPlist *  group;
	MPlist *  candidate ;
	u_int  num_of_candidates = 0;
	int  idx ;
	int  x ;
	int  y ;

#ifdef  IM_M17NLIB_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	if( m17nlib->input_context->candidate_list == NULL ||
	    m17nlib->input_context->candidate_show == 0)
	{
		if( m17nlib->im.cand_screen)
		{
			(*m17nlib->im.cand_screen->delete)( m17nlib->im.cand_screen) ;
			m17nlib->im.cand_screen = NULL ;
		}

		if( m17nlib->im.stat_screen)
		{
			(*m17nlib->im.stat_screen->show)( m17nlib->im.stat_screen) ;
		}

		return ;
	}

	group = m17nlib->input_context->candidate_list ;
	while( mplist_value( group) != Mnil)
	{
		if( mplist_key( group) == Mtext)
		{
			num_of_candidates += mtext_len( mplist_value( group));
		}
		else
		{
			num_of_candidates += mplist_length( mplist_value( group)) ;
		}
		group = mplist_next( group);
	}

#ifdef  IM_M17NLIB_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " number of candidates: %d\n", num_of_candidates) ;
#endif

	(*m17nlib->im.listener->get_spot)( m17nlib->im.listener->self ,
					   m17nlib->im.preedit.chars ,
					   m17nlib->im.preedit.segment_offset ,
					   &x , &y) ;

	if( m17nlib->im.cand_screen == NULL)
	{
		int  is_vertical_direction = 0 ;

		if( strcmp( msymbol_name( m17nlib->input_method->name) , "anthy") == 0)
		{
			is_vertical_direction = 1 ;
		}

		if( ! ( m17nlib->im.cand_screen = (*syms->x_im_candidate_screen_new)(
				(*m17nlib->im.listener->get_win_man)(m17nlib->im.listener->self) ,
				(*m17nlib->im.listener->get_font_man)(m17nlib->im.listener->self) ,
				(*m17nlib->im.listener->get_color_man)(m17nlib->im.listener->self) ,
				(*m17nlib->im.listener->is_vertical)(m17nlib->im.listener->self) ,
				is_vertical_direction ,
				(*m17nlib->im.listener->get_line_height)(m17nlib->im.listener->self) ,
				x , y)))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " x_im_candidate_screen_new() failed.\n") ;
		#endif
			
			return ;
		}

		m17nlib->im.cand_screen->listener.self = m17nlib ;
		m17nlib->im.cand_screen->listener.selected = NULL ; /* XXX */

	}

	if( ! (*m17nlib->im.cand_screen->init)( m17nlib->im.cand_screen ,
						num_of_candidates , 10))
	{
		(*m17nlib->im.cand_screen->delete)( m17nlib->im.cand_screen) ;
		m17nlib->im.cand_screen = NULL ;
		return ;
	}

	group = m17nlib->input_context->candidate_list ;

	if( mplist_key( group) == Mtext)
	{
		for( idx = 0 ;
		     mplist_key( group) != Mnil ;
		     group = mplist_next( group))
		{
			int  i ;
			for( i = 0 ; i < mtext_len( mplist_value( group)) ; i++)
			{
				MText *  text ;
				text = mtext() ;
				mtext_cat_char( text , mtext_ref_char( mplist_value( group) , i)) ;
				set_candidate( m17nlib , text , idx) ;
				m17n_object_unref( text) ;
				idx++ ;
			}
		}
	}
	else
	{
		for( idx = 0 ; mplist_key( group) != Mnil ; group = mplist_next( group))
		{
			for( candidate = mplist_value( group) ;
			     mplist_key( candidate) != Mnil ;
			     candidate = mplist_next( candidate) , idx++)
			{
				set_candidate( m17nlib ,
					       mplist_value( candidate) ,
					       idx) ;
			}
		}
	}

	(*m17nlib->im.cand_screen->select)( m17nlib->im.cand_screen ,
					    m17nlib->input_context->candidate_index) ;

	(*m17nlib->im.cand_screen->set_spot)( m17nlib->im.cand_screen , x , y) ;

	if( m17nlib->im.stat_screen)
	{
		(*m17nlib->im.stat_screen->hide)( m17nlib->im.stat_screen) ;
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
	im_m17nlib_t *  m17nlib ;

	m17nlib = (im_m17nlib_t*) im ;

	ref_count-- ;

#ifdef  IM_M17NLIB_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " An object was deleted. ref_count: %d\n", ref_count) ;
#endif

	if( m17nlib->input_context)
	{
		minput_destroy_ic( m17nlib->input_context) ;
	}

	if( m17nlib->input_method)
	{
		minput_close_im( m17nlib->input_method) ;
	}

	if( m17nlib->mconverter)
	{
		mconv_free_converter( m17nlib->mconverter) ;
	}

	if( m17nlib->parser_term)
	{
		(*m17nlib->parser_term->delete)( m17nlib->parser_term) ;
	}

	if( m17nlib->conv)
	{
		(*m17nlib->conv->delete)( m17nlib->conv) ;
	}

	free( m17nlib) ;

	if( ref_count == 0 && initialized)
	{
		M17N_FINI() ;

		initialized = 0 ;

		if( parser_ascii)
		{
			(*parser_ascii->delete)( parser_ascii) ;
			parser_ascii = NULL ;
		}
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
	im_m17nlib_t *  m17nlib ;
	MSymbol  mkey ;
	MText *  text ;
	int  ret = 1 ;

	m17nlib = (im_m17nlib_t*) im ;

	if( ! m17nlib->input_context->active)
	{
		return  1 ;
	}

	if( ( mkey = xksym_to_msymbol( m17nlib , ksym , event->state)) == Mnil)
	{
		return  1 ;
	}

	if( minput_filter( m17nlib->input_context , mkey , Mnil))
	{
		ret = 0 ;
	}

	if( m17nlib->input_context->preedit_changed)
	{
		preedit_changed( m17nlib) ;
	}

	if( m17nlib->input_context->candidates_changed)
	{
		candidates_changed( m17nlib) ;
	}

	text = mtext() ;

	if( minput_lookup( m17nlib->input_context , Mnil , NULL , text) == 0)
	{
		if( mtext_len( text))
		{
			commit( m17nlib , text) ;
			ret = 0 ;
		}
	}

	m17n_object_unref( text) ;

	return  ret ;
}

static int
switch_mode(
	x_im_t *  im
	)
{
	im_m17nlib_t *  m17nlib ;
	int  x ;
	int  y ;

	m17nlib = (im_m17nlib_t*) im ;

	(*m17nlib->im.listener->get_spot)( m17nlib->im.listener->self ,
					   m17nlib->im.preedit.chars ,
					   m17nlib->im.preedit.segment_offset ,
					   &x , &y) ;

	if( ! m17nlib->input_context->active)
	{
		u_char  buf[50] ;
		u_int  filled_len ;

		if( m17nlib->im.stat_screen == NULL)
		{
			if( ! ( m17nlib->im.stat_screen = (*syms->x_im_status_screen_new)(
					(*m17nlib->im.listener->get_win_man)(m17nlib->im.listener->self) ,
					(*m17nlib->im.listener->get_font_man)(m17nlib->im.listener->self) ,
					(*m17nlib->im.listener->get_color_man)(m17nlib->im.listener->self) ,
					(*m17nlib->im.listener->is_vertical)(m17nlib->im.listener->self) ,
					x , y)))
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " x_im_satus_screen_new() failed.\n") ;
			#endif

				return  1 ;
			}
		}

		mconv_reset_converter( m17nlib->mconverter) ;
		mconv_rebind_buffer( m17nlib->mconverter , buf , sizeof( buf)) ;
		filled_len = mconv_encode( m17nlib->mconverter ,
					   m17nlib->input_context->status) ;

		if( filled_len == -1)
		{
			kik_error_printf( "Could not convert the encoding of characters for status. [%d]\n" , merror_code) ;
		}
		else
		{
			buf[filled_len] = 0 ;
			(*m17nlib->im.stat_screen->set)( m17nlib->im.stat_screen ,
							 m17nlib->parser_term , buf) ;
		}
	}
	else
	{
		/*
		 * commit the last preedit before deactivating the input
		 * method.
		 */
		if( mtext_len( m17nlib->input_context->preedit))
		{
			commit( m17nlib , m17nlib->input_context->preedit) ;
		}

		/*
		 * initialize the state of MinputContext.
		 * <http://sf.net/mailarchive/message.php?msg_id=9958816>
		 */
		minput_filter( m17nlib->input_context , Mnil , Mnil);


		/*
		 * clear saved preedit
		 */

		if( m17nlib->im.preedit.chars)
		{
			(*syms->ml_str_delete)( m17nlib->im.preedit.chars ,
						m17nlib->im.preedit.num_of_chars) ;
			m17nlib->im.preedit.chars = NULL ;
		}

		m17nlib->im.preedit.num_of_chars = 0 ;
		m17nlib->im.preedit.filled_len = 0 ;
		m17nlib->im.preedit.segment_offset = 0 ;
		m17nlib->im.preedit.cursor_offset = X_IM_PREEDIT_NOCURSOR ;


		if( m17nlib->im.stat_screen)
		{
			(*m17nlib->im.stat_screen->delete)( m17nlib->im.stat_screen) ;
			m17nlib->im.stat_screen = NULL ;
		}

		if( m17nlib->im.cand_screen)
		{
			(*m17nlib->im.cand_screen->delete)( m17nlib->im.cand_screen) ;
			m17nlib->im.cand_screen = NULL ;
		}
	}

	minput_toggle( m17nlib->input_context) ;

	return  1 ;
}

static void
focused(
	x_im_t *  im
	)
{
	im_m17nlib_t *  m17nlib ;

	m17nlib = (im_m17nlib_t*) im ;

	if( m17nlib->im.cand_screen)
	{
		(*m17nlib->im.cand_screen->show)( m17nlib->im.cand_screen) ;
	}
	else if( m17nlib->im.stat_screen)
	{
		(*m17nlib->im.stat_screen->show)( m17nlib->im.stat_screen) ;
	}
}

static void
unfocused(
	x_im_t *  im
	)
{
	im_m17nlib_t *  m17nlib ;

	m17nlib = (im_m17nlib_t*) im ;

	if( m17nlib->im.stat_screen)
	{
		(*m17nlib->im.stat_screen->hide)( m17nlib->im.stat_screen) ;
	}

	if( m17nlib->im.cand_screen)
	{
		(*m17nlib->im.cand_screen->hide)( m17nlib->im.cand_screen) ;
	}
}


/* --- global functions --- */

x_im_t *
im_m17nlib_new(
	u_int64_t  magic ,
	ml_char_encoding_t  term_encoding ,
	x_im_export_syms_t *  export_syms ,
	char *  param ,	/* <language>:<input method> */
	u_int  mod_ignore_mask
	)
{
	im_m17nlib_t *  m17nlib ;
	char *  encoding_name ;
	MSymbol  encoding_sym ;

	if( magic != (u_int64_t) IM_API_COMPAT_CHECK_MAGIC)
	{
		kik_error_printf( "Incompatible input method API.\n") ;

		return  NULL ;
	}

	m17nlib = NULL ;

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

		M17N_INIT() ;

	#ifdef  RESTORE_LOCALE
		/* restoring */
		/*
		 * TODO: remove valgrind warning.
		 * The memory space pointed to by sys_locale in kik_locale.c
		 * was freed by setlocale() in m17nlib.
		 */
		kik_locale_init( cur_locale) ;
	#endif

		if( merror_code != MERROR_NONE)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG "failed to initialize m17n library\n") ;
		#endif
			goto  error ;
		}

		syms = export_syms ;

		initialized = 1 ;

		if( ! ( parser_ascii = (*syms->ml_parser_new)( ML_ISO8859_1)))
		{
			goto  error ;
		}
	}

#ifdef  IM_M17NLIB_DEBUG
	show_available_ims();
#endif

	if( ! ( m17nlib = malloc( sizeof( im_m17nlib_t))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif
		goto  error ;
	}

	m17nlib->input_method = NULL ;
	m17nlib->input_context = NULL ;
	m17nlib->mconverter = NULL ;
	m17nlib->parser_term = NULL ;
	m17nlib->conv = NULL ;

	if( ! ( m17nlib->input_method = find_input_method( param)))
	{
		kik_error_printf( "Could not find %s\n" , param) ;
		goto  error ;
	}

	if( ! ( m17nlib->input_context = minput_create_ic( m17nlib->input_method , NULL)))
	{
		kik_error_printf( "Could not crate context for %s\n", param) ;
		goto  error ;
	}

	if( term_encoding == ML_EUCJISX0213)
	{
		encoding_name = (*syms->ml_get_char_encoding_name)( ML_EUCJP) ;
	}
	else
	{
		encoding_name = (*syms->ml_get_char_encoding_name)( term_encoding) ;
	}


	if( ( encoding_sym = mconv_resolve_coding( msymbol( encoding_name))) == Mnil)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Could not resolve encoding name [%s]\n" , encoding_name) ;
	#endif
		goto  error ;
	}

	if( ! ( m17nlib->mconverter = mconv_buffer_converter( encoding_sym , NULL , 0)))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Could not create MConverter\n") ;
	#endif
		goto  error ;
	}

	if( ! ( m17nlib->conv = (*syms->ml_conv_new)( term_encoding)))
	{
		goto  error ;
	}

	if( ! ( m17nlib->parser_term = (*syms->ml_parser_new)( term_encoding)))
	{
		goto  error ;
	}

	minput_toggle( m17nlib->input_context) ;

	/*
	 * set methods of x_im_t
	 */
	m17nlib->im.delete = delete ;
	m17nlib->im.key_event = key_event ;
	m17nlib->im.switch_mode = switch_mode ;
	m17nlib->im.focused = focused ;
	m17nlib->im.unfocused = unfocused ;

	ref_count++;

#ifdef  IM_M17NLIB_DEBUG
	kik_debug_printf("New object was created. ref_count is %d.\n", ref_count);
#endif

	return  (x_im_t*) m17nlib ;

error:
	if( m17nlib)
	{
		if( m17nlib->input_context)
		{
			minput_destroy_ic( m17nlib->input_context) ;
		}

		if( m17nlib->mconverter)
		{
			mconv_free_converter( m17nlib->mconverter) ;
		}

		if( m17nlib->input_method)
		{
			minput_close_im( m17nlib->input_method) ;
		}

		if( m17nlib->parser_term)
		{
			(*m17nlib->parser_term->delete)( m17nlib->parser_term) ;
		}

		if( m17nlib->conv)
		{
			(*m17nlib->conv->delete)( m17nlib->conv) ;
		}

		free( m17nlib) ;
	}

	if( initialized && ref_count == 0)
	{
		M17N_FINI() ;

		if( parser_ascii)
		{
			(*parser_ascii->delete)( parser_ascii) ;
			parser_ascii = NULL ;
		}

		initialized = 0 ;
	}

	return  NULL ;
}


/* --- API for external tools --- */

im_info_t *
im_m17nlib_get_info(
	char *  locale ,
	char *  encoding
	)
{
	im_info_t *  result = NULL ;
	MPlist *  im_list ;
	MSymbol  sym_im ;
	int  i ;
	int  num_of_ims ;
	int  auto_idx = 0 ;

	M17N_INIT() ;

	sym_im = msymbol( "input-method") ;
	im_list = mdatabase_list( sym_im , Mnil , Mnil , Mnil) ;
	num_of_ims = mplist_length( im_list) ;

	if( num_of_ims == 0)
	{
		goto  error ;
	}

	if( ! ( result = malloc( sizeof( im_info_t))))
	{
		goto  error ;
	}

	result->id = strdup( "m17nlib") ;
	result->name = strdup( "m17n library") ;
	result->num_of_args = num_of_ims + 1;
	if( ! ( result->args = calloc( result->num_of_args , sizeof(char*))))
	{
		goto  error ;
	}

	if( ! ( result->readable_args = calloc( result->num_of_args , sizeof(char*))))
	{
		free( result->args) ;
		goto  error ;
	}

	for( i = 1 ; i < result->num_of_args; i++, im_list = mplist_next( im_list))
	{
		MDatabase *  db ;
		MSymbol *  tag ;
		size_t  len ;
		char *  lang ;
		char *  im ;

		db = mplist_value( im_list) ;
		tag = mdatabase_tag( db) ;

		lang = msymbol_name( tag[1]) ;
		im = msymbol_name( tag[2]) ;

		len = strlen( im) + strlen( lang) + 4 ;

		if( ( result->args[i] = malloc(len)))
		{
			kik_snprintf( result->args[i] , len ,
				      "%s:%s" , lang , im) ;
		}
		else
		{
			result->args[i] = strdup( "error") ;
		}

		if( ( result->readable_args[i] = malloc(len)))
		{
			kik_snprintf( result->readable_args[i] , len ,
				      "%s (%s)" , lang , im) ;
		}
		else
		{
			result->readable_args[i] = strdup( "error") ;
		}

		if( strncmp( lang , locale , 2) == 0 && auto_idx == 0)
		{
			auto_idx = i ;
		}
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

	M17N_FINI() ;

	return  result ;

error:
	M17N_FINI() ;

	if( result)
	{
		free( result) ;
	}

	if( parser_ascii)
	{
		(*parser_ascii->delete)( parser_ascii) ;
		parser_ascii = NULL ;
	}

	return  NULL ;
}

