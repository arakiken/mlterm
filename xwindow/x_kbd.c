/*
 *	$Id$
 */

#include  "x_kbd.h"

#include  <kiklib/kik_util.h>
#include  <mkf/mkf_utf16_parser.h>


#if  0

typedef struct canna_kbd
{
	x_kbd_t  kbd ;
	Window  tiny_win ;	/* conversion window */

} canna_kbd_t ;

typedef struct skk_kbd
{
	x_kbd_t  kbd ;
	
} skk_kbd_t ;

#endif

typedef struct iscii_kbd
{
	x_kbd_t  kbd ;

	ml_iscii_keymap_t  keymap ;
	
} iscii_kbd_t ;


/* --- static variables --- */

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

static size_t
arabic_get_str(
	x_kbd_t *  kbd ,
	u_char *  seq ,
	size_t  seq_len ,
	mkf_parser_t **  parser ,
	KeySym *  ksym ,
	XKeyEvent *  event
	)
{
	size_t  len ;

	len = x_window_get_str( kbd->window , seq , seq_len , parser , ksym , event) ;

	if( (event->state & ~ShiftMask) == 0 && len == 1 && 0x27 <= *seq && *seq <= 0x7e)
	{
		char *  c ;

		if( ( c = arabic_conv_tbl[*seq - 0x27]))
		{
			if( *c == 0x0)
			{
				/* "\x00\xNN" */
				len = 1 + strlen( c + 1) ;
			}
			else
			{
				len = strlen( c) ;
			}
			
			/* Don't use strcpy because c may include NULL character. */
			memcpy( seq , c , K_MIN(len,seq_len - 1)) ;
			*parser = kbd->parser ;
			*ksym = 0 ;
		}
	}

	return  len ;
}

static int
arabic_delete(
	x_kbd_t *  kbd
	)
{
	(*kbd->parser->delete)( kbd->parser) ;
	free( kbd) ;

	return  1 ;
}

static size_t
iscii_get_str(
	x_kbd_t *  kbd ,
	u_char *  seq ,
	size_t  seq_len ,
	mkf_parser_t **  parser ,
	KeySym *  ksym ,
	XKeyEvent *  event
	)
{
	iscii_kbd_t *  iscii_kbd ;
	size_t  len ;

	iscii_kbd = (iscii_kbd_t*) kbd ;
	
	len = x_window_get_str( kbd->window , seq , seq_len , parser , ksym , event) ;
	
	if( (event->state & ~ShiftMask) == 0 && len == 1 && 0x21 <= *seq && *seq <= 0x7e)
	{
		len = ml_convert_ascii_to_iscii( iscii_kbd->keymap , seq , seq_len , seq , len) ;
		*parser = NULL ;
		*ksym = 0 ;
	}

	return  len ;
}

static int
iscii_delete(
	x_kbd_t *  kbd
	)
{
	iscii_kbd_t *  iscii_kbd ;

	iscii_kbd = (iscii_kbd_t*) kbd ;

	ml_iscii_keymap_delete( iscii_kbd->keymap) ;
	
	free( kbd) ;

	return  1 ;
}



/* --- global functions --- */

x_kbd_t *
x_arabic_kbd_new(
	x_window_t *  win
	)
{
	x_kbd_t *  kbd ;

	if( ( kbd = malloc( sizeof( x_kbd_t))) == NULL)
	{
		return  NULL ;
	}

	if( ( kbd->parser = mkf_utf16_parser_new()) == NULL)
	{
		free( kbd) ;
		
		return  NULL ;
	}

	kbd->type = KBD_ARABIC ;
	kbd->window = win ;
	kbd->delete = arabic_delete ;
	kbd->get_str = arabic_get_str ;

	return  kbd ;
}

x_kbd_t *
x_iscii_phonetic_kbd_new(
	x_window_t *  win
	)
{
	iscii_kbd_t *  iscii_kbd ;

	if( ( iscii_kbd = malloc( sizeof( iscii_kbd_t))) == NULL)
	{
		return  NULL ;
	}

	if( ( iscii_kbd->keymap = ml_iscii_keymap_new( 0)) == NULL)
	{
		free( iscii_kbd) ;

		return  NULL ;
	}

	iscii_kbd->kbd.type = KBD_ISCII_PHONETIC ;	
	iscii_kbd->kbd.window = win ;
	iscii_kbd->kbd.parser = NULL ;		/* XXX */
	iscii_kbd->kbd.delete = iscii_delete ;
	iscii_kbd->kbd.get_str = iscii_get_str ;

	return  &iscii_kbd->kbd ;	
}

x_kbd_t *
x_iscii_inscript_kbd_new(
	x_window_t *  win
	)
{
	iscii_kbd_t *  iscii_kbd ;

	if( ( iscii_kbd = malloc( sizeof( iscii_kbd_t))) == NULL)
	{
		return  NULL ;
	}

	if( ( iscii_kbd->keymap = ml_iscii_keymap_new( 1)) == NULL)
	{
		free( iscii_kbd) ;

		return  NULL ;
	}
	
	iscii_kbd->kbd.type = KBD_ISCII_INSCRIPT ;
	iscii_kbd->kbd.window = win ;
	iscii_kbd->kbd.parser = NULL ;		/* XXX */
	iscii_kbd->kbd.delete = iscii_delete ;
	iscii_kbd->kbd.get_str = iscii_get_str ;

	return  &iscii_kbd->kbd ;	
}
