/*
 * im_kbd.c - keyboard mapping input method for mlterm
 *
 *    based on x_kbd.c written by Araki Ken.
 *
 * Copyright (C) 2001, 2002, 2003 Araki Ken <arakiken@users.sf.net>
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
#include  <kiklib/kik_str.h>	/* kik_snprintf */
#include  <mkf/mkf_utf16_parser.h>
#include  <ml_iscii.h>

#include  <x_im.h>
#include  "../im_common.h"
#include  "../im_info.h"

#if  0
#define IM_KBD_DEBUG 1
#endif

typedef enum  kbd_type
{
	KBD_TYPE_ARABIC ,
	KBD_TYPE_ISCII ,

}  kbd_type_t ;

typedef enum  kbd_mode
{
	KBD_MODE_ASCII = 0 ,
	KBD_MODE_ARABIC ,
	KBD_MODE_ISCII_INSCRIPT ,
	KBD_MODE_ISCII_PHONETIC ,

}  kbd_mode_t ;

typedef struct im_kbd
{
	x_im_t  im ;

	kbd_type_t  type ;
	kbd_mode_t  mode ;

	ml_iscii_keymap_t  keymap ;

	mkf_parser_t *  parser ;
	mkf_conv_t *  conv ;

}  im_kbd_t ;


/* --- static variables --- */

static int  ref_count = 0 ;
static int  initialized = 0 ;
static mkf_parser_t *  parser_ascii = NULL ;

/* mlterm internal symbols */
static x_im_export_syms_t *  mlterm_syms = NULL ;

static u_char *  arabic_conv_tbl[] =
{
	"\x06\x37" ,	/* ' */
	NULL ,		/* ( */
	NULL ,		/* ) */
	NULL ,		/* * */
	NULL ,		/* + */
	"\x06\x48" ,	/* , */
	NULL ,		/* - */
	"\x06\x32" ,	/* . */
	"\x06\x38" ,	/* / */
	"\x06\x60" ,	/* 0 */
	"\x06\x61" ,	/* 1 */
	"\x06\x62" ,	/* 2 */
	"\x06\x63" ,	/* 3 */
	"\x06\x64" ,	/* 4 */
	"\x06\x65" ,	/* 5 */
	"\x06\x66" ,	/* 6 */
	"\x06\x67" ,	/* 7 */
	"\x06\x68" ,	/* 8 */
	"\x06\x69" ,	/* 9 */
	NULL ,		/* : */
	"\x06\x43" ,	/* ; */
	"\x00\x2c" ,	/* < */
	NULL ,		/* = */
	"\x00\x2e" ,	/* > */
	"\x06\x1f" ,	/* ? */
	NULL ,		/* @ */
	"\x06\x50" ,	/* A */
	"\x06\x44\x06\x22" ,	/* B */
	"\x00\x7b" ,	/* C */
	"\x00\x5b" ,	/* D */
	"\x06\x4f" ,	/* E */
	"\x00\x5d" ,	/* F */
	"\x06\x44\x06\x23" ,	/* G */
	"\x06\x23" ,	/* H */
	"\x00\xf7" ,	/* I */
	"\x06\x40" ,	/* J */
	"\x06\x0c" ,	/* K */
	"\x00\x2f" ,	/* L */
	"\x00\x27" ,	/* M */
	"\x06\x22" ,	/* N */
	"\x00\xd7" ,	/* O */
	"\x06\x1b" ,	/* P */
	"\x06\x4e" ,	/* Q */
	"\x06\x4c" ,	/* R */
	"\x06\x4d" ,	/* S */
	"\x06\x44\x06\x25" ,	/* T */
	"\x00\x60" ,	/* U */
	"\x00\x7d" ,	/* V */
	"\x06\x4b" ,	/* W */
	"\x06\x52" ,	/* X */
	"\x06\x25" ,	/* Y */
	"\x00\x7e" ,	/* Z */
	"\x06\x2c" ,	/* [ */
	NULL ,		/* \ */
	"\x06\x2f" ,	/* ] */
	NULL ,		/* ^ */
	NULL ,		/* _ */
	"\x06\x30" ,	/* ` */
	"\x06\x34" ,	/* a */
	"\x06\x44\x06\x27" ,	/* b */
	"\x06\x24" ,	/* c */
	"\x06\x4a" ,	/* d */
	"\x06\x2b" ,	/* e */
	"\x06\x28" ,	/* f */
	"\x06\x44" ,	/* g */
	"\x06\x27" ,	/* h */
	"\x06\x47" ,	/* i */
	"\x06\x2a" ,	/* j */
	"\x06\x46" ,	/* k */
	"\x06\x45" ,	/* l */
	"\x06\x29" ,	/* m */
	"\x06\x49" ,	/* n */
	"\x06\x2e" ,	/* o */
	"\x06\x2d" ,	/* p */
	"\x06\x36" ,	/* q */
	"\x06\x42" ,	/* r */
	"\x06\x33" ,	/* s */
	"\x06\x41" ,	/* t */
	"\x06\x39" ,	/* u */
	"\x06\x31" ,	/* v */
	"\x06\x35" ,	/* w */
	"\x06\x21" ,	/* x */
	"\x06\x3a" ,	/* y */
	"\x06\x26" ,	/* z */
	"\x00\x3c" ,	/* { */
	NULL ,		/* | */
	"\x00\x3e" ,	/* } */
	"\x06\x51" ,	/* ~ */

} ;


/* --- static functions --- */

/*
 * methods of x_im_t
 */

static int
delete(
	x_im_t *  im
	)
{
	im_kbd_t *  kbd ;

	kbd = (im_kbd_t*) im ;

	if( kbd->keymap)
	{
		(*mlterm_syms->ml_iscii_keymap_delete)( kbd->keymap) ;
	}

	if( kbd->parser)
	{
		(*kbd->parser->delete)( kbd->parser) ;
	}

	if( kbd->conv)
	{
		(*kbd->conv->delete)( kbd->conv) ;
	}

	if( kbd->im.stat_screen)
	{
		(*kbd->im.stat_screen->delete)( kbd->im.stat_screen) ;
	}

	ref_count-- ;

#ifdef  IM_KBD_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " An object was deleted. ref_count: %d\n", ref_count) ;
#endif

	free( kbd) ;

	if( initialized && ref_count == 0)
	{
		(*parser_ascii->delete)( parser_ascii) ;
		parser_ascii = NULL ;

		initialized = 0 ;
	}

	return  ref_count ;
}

static int
key_event_arabic(
	x_im_t *  im ,
	u_char  key_char ,
	KeySym  ksym ,
	XKeyEvent *  event
	)
{
	im_kbd_t *  kbd ;
	size_t  len ;
	u_char *  c ;
	u_char  conv_buf[10] ;

	kbd = (im_kbd_t*) im ;

	if( kbd->mode != KBD_MODE_ARABIC)
	{
		return  1 ;
	}

	if( event->state & ~ShiftMask)
	{
		return  1 ;
	}

	if( key_char < 0x27 || key_char > 0x7e)
	{
		return  1 ;
	}

	if( ! ( c = arabic_conv_tbl[key_char - 0x27]))
	{
		return  1 ;
	}

	if( *c == 0x0)
	{
		/* "\x00\xNN" */
		len = 1 + strlen( c + 1) ;
	}
	else
	{
		len = strlen( c) ;
	}

	(*kbd->parser->init)( kbd->parser) ;
	(*kbd->parser->set_str)( kbd->parser , c , len) ;

	(*kbd->conv->init)( kbd->conv) ;

	while( ! kbd->parser->is_eos)
	{
		len = (*kbd->conv->convert)( kbd->conv , conv_buf ,
					     sizeof( conv_buf) , kbd->parser) ;

		if( len == 0)
		{
			/* finished converting */
			break ;
		}

		(*kbd->im.listener->write_to_term)( kbd->im.listener->self ,
						    conv_buf , len) ;
	}

	return  0 ;
}

static int
key_event_iscii(
	x_im_t *  im ,
	u_char  key_char ,
	KeySym  ksym ,
	XKeyEvent *  event
	)
{
	im_kbd_t *  kbd ;
	u_char  buf[512] ;
	size_t  len ;

	kbd = (im_kbd_t*) im ;

	if( kbd->mode == KBD_MODE_ASCII)
	{
		return  1 ;
	}

	if( event->state & ~ShiftMask)
	{
		return  1 ;
	}

	if( key_char < 0x21 || key_char > 0x7e)
	{
		return  1 ;
	}

	len = (*mlterm_syms->ml_convert_ascii_to_iscii)( kbd->keymap ,
							 buf , sizeof( buf) ,
							 &key_char , 1) ;

	(*kbd->im.listener->write_to_term)( kbd->im.listener->self ,
					    buf , len) ;

	return  0 ;
}

static int
switch_mode(
	x_im_t *  im
	)
{
	im_kbd_t *  kbd ;

	kbd =  (im_kbd_t*)  im ;

	if( kbd->type == KBD_TYPE_ARABIC)
	{
		if( kbd->mode == KBD_MODE_ASCII)
		{
			kbd->mode = KBD_MODE_ARABIC ;
		}
		else
		{
			kbd->mode = KBD_MODE_ASCII ;
		}
	}
	else /* kbd->type == KBD_TYPE_ISCII */
	{
		if( kbd->keymap)
		{
			(*mlterm_syms->ml_iscii_keymap_delete)( kbd->keymap) ;
			kbd->keymap = NULL ;
		}


		/* Inscript => Phonetic => US ASCII => Inscript ... */

		if( kbd->mode == KBD_MODE_ASCII)
		{
			kbd->keymap = (*mlterm_syms->ml_iscii_keymap_new)( 1) ;
			kbd->mode = KBD_MODE_ISCII_INSCRIPT ;

		#ifdef  IM_KBD_DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " switched to inscript.\n");
		#endif

		}
		else if( kbd->mode == KBD_MODE_ISCII_INSCRIPT)
		{
			kbd->keymap = (*mlterm_syms->ml_iscii_keymap_new)( 0) ;
			kbd->mode = KBD_MODE_ISCII_PHONETIC ;

		#ifdef  IM_KBD_DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " switched to phonetic.\n");
		#endif
		}
		else
		{
			kbd->mode = KBD_MODE_ASCII ;

		#ifdef  IM_KBD_DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " switched to ascii.\n");
		#endif
		}

		if( ( kbd->type == KBD_MODE_ISCII_INSCRIPT ||
		      kbd->type == KBD_MODE_ISCII_PHONETIC) &&
		    ( kbd->keymap == NULL))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_iscii_keymap_new() failed.\n");
		#endif
			kbd->mode = KBD_MODE_ASCII ;
		}
	}

	if( kbd->mode == KBD_MODE_ASCII)
	{
		if( kbd->im.stat_screen)
		{
			(*kbd->im.stat_screen->delete)( kbd->im.stat_screen) ;
			kbd->im.stat_screen = NULL ;
		}
	}
	else
	{
		int  x ;
		int  y ;

		(*kbd->im.listener->get_spot)( kbd->im.listener->self ,
					       NULL , 0 , &x , &y) ;

		if( kbd->im.stat_screen == NULL)
		{
			if( ! ( kbd->im.stat_screen = (*mlterm_syms->x_im_status_screen_new)(
						(*kbd->im.listener->get_win_man)(kbd->im.listener->self) ,
						(*kbd->im.listener->get_font_man)(kbd->im.listener->self) ,
						(*kbd->im.listener->get_color_man)(kbd->im.listener->self) ,
						(*kbd->im.listener->is_vertical)(kbd->im.listener->self) ,
						x , y)))
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " x_im_satus_screen_new() failed.\n") ;
			#endif

				return  0 ;
			}
		}

		switch( kbd->mode)
		{
		case KBD_MODE_ARABIC:
			(*kbd->im.stat_screen->set)( kbd->im.stat_screen ,
						     parser_ascii ,
						     "Arabic") ;
			break ;
		case KBD_MODE_ISCII_INSCRIPT:
			(*kbd->im.stat_screen->set)( kbd->im.stat_screen ,
						     parser_ascii ,
						     "ISCII:inscript") ;
			break ;
		case KBD_MODE_ISCII_PHONETIC:
			(*kbd->im.stat_screen->set)( kbd->im.stat_screen ,
						     parser_ascii ,
						     "ISCII:phonetic") ;
			break ;
		default:
			break ;
		}
	}

	return  1 ;
}

static void
focused(
	x_im_t *  im
	)
{
	im_kbd_t *  kbd ;

	kbd =  (im_kbd_t*)  im ;

	if( kbd->im.stat_screen)
	{
		(*kbd->im.stat_screen->show)( kbd->im.stat_screen) ;
	}
}

static void
unfocused(
	x_im_t *  im
	)
{
	im_kbd_t *  kbd ;

	kbd = (im_kbd_t*)  im ;

	if( kbd->im.stat_screen)
	{
		(*kbd->im.stat_screen->hide)( kbd->im.stat_screen) ;
	}
}


/* --- global functions --- */

x_im_t *
im_new(
	u_int64_t  magic ,
	ml_char_encoding_t  term_encoding ,
	x_im_export_syms_t *  export_syms ,
	char *  opts
	)
{
	im_kbd_t *  kbd ;
	void * hold_handler;

	if( magic != (u_int64_t) IM_API_COMPAT_CHECK_MAGIC)
	{
		kik_error_printf( "Incompatible input method API.\n") ;

		return  NULL ;
	}

	if( ! initialized)
	{
		mlterm_syms = export_syms ;

		if( ! ( parser_ascii = (*mlterm_syms->ml_parser_new)( ML_ISO8859_1)))
		{
			return  NULL ;
		}

		initialized = 1 ;
	}

	kbd = NULL ;

	if( ! ( kbd = malloc( sizeof( im_kbd_t))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif

		goto  error ;
	}

	if( term_encoding == ML_ISCII)
	{
		kbd->type = KBD_TYPE_ISCII ;
	}
	else
	{
		kbd->type = KBD_TYPE_ARABIC ;
	}

	kbd->mode = KBD_MODE_ASCII ;
	kbd->keymap = NULL ;
	kbd->parser = NULL ;
	kbd->conv = NULL ;

	if( kbd->type == KBD_TYPE_ARABIC)
	{
		if( ! ( kbd->parser = mkf_utf16_parser_new()))
		{
			goto  error ;
		}
	}

	if( ! ( kbd->conv = (*mlterm_syms->ml_conv_new)( term_encoding)))
	{
		goto  error ;
	}

	/*
	 * set methods of x_im_t
	 */
	kbd->im.delete = delete ;
	kbd->im.key_event = (kbd->type == KBD_TYPE_ARABIC) ?
			    key_event_arabic : key_event_iscii ;
	kbd->im.switch_mode = switch_mode ;
	kbd->im.focused = focused ;
	kbd->im.unfocused = unfocused ;

	ref_count++;

#ifdef  IM_KBD_DEBUG
	kik_debug_printf("New object was created. ref_count is %d.\n", ref_count) ;
#endif

	return  (x_im_t*) kbd ;

error:
	if( kbd)
	{
		if( kbd->parser)
		{
			(*kbd->parser->delete)( kbd->parser) ;
		}

		free( kbd) ;
	}

	if( initialized && ref_count)
	{
		(*parser_ascii->delete)( parser_ascii) ;
		parser_ascii = NULL ;

		initialized = 0 ;
	}

	return  NULL ;
}


/* --- API for external tools --- */

im_info_t *
im_get_info(
	char *  locale ,
	char *  encoding
)
{
	im_info_t *  result ;
	int  i ;

	if( ! ( result = malloc( sizeof( im_info_t))))
	{
		return  NULL ;
	}

	result->id = strdup( "kbd") ;
	result->name = strdup( "keyboard") ;
	result->num_of_args = 1 ;

	if( ! ( result->args = malloc( sizeof(char*) * result->num_of_args)))
	{
		free( result) ;
		return  NULL ;
	}

	if( ! ( result->readable_args = malloc( sizeof(char*) * result->num_of_args)))
	{
		free( result->args) ;
		free( result) ;
		return  NULL ;
	}

	result->args[0] = strdup( "") ;
	if( strcmp( encoding , "ISCII") == 0)
	{
		result->readable_args[0] = strdup( "Indic") ;
	}
	else
	{
		result->readable_args[0] = strdup( "Arabic") ;
	}

	return  result ;
}

