/** @file
 * @brief X Drag and Drop protocol support
 * 
 *	$Id$
 */

#include  "x_window.h"

/*
#include  <ctype.h>
*/
#include  <X11/Xatom.h>
#include  <mkf/mkf_utf8_conv.h>
#include  <mkf/mkf_utf16_parser.h>

#include  "x_dnd.h"

/* XXX Should we create x_atom.h ?? */
#define  XA_COMPOUND_TEXT(display)  (XInternAtom(display , "COMPOUND_TEXT" , False))
#define  XA_TEXT(display)  (XInternAtom( display , "TEXT" , False))
#define  XA_UTF8_STRING(display)  (XInternAtom(display , "UTF8_STRING" , False))
#define  XA_INCR(display) (XInternAtom(display, "INCR", False))

static u_char  DND_VERSION = 5 ;

/* --- static functions --- */
static int
charset_name2code(
	char *charset
	)
{
	/*  Not used for now.
	    Someday we can use codeset as defined in XDND spec.

	    int i;
	for( i = strlen(charset) -1 ; i > 0 ; i--)
		charset[i] = tolower(charset[i]);
	if( strcmp(charset, "utf-16le") ==0 )
		return 0 ;
	*/

	return -1 ;
}

static int
is_pref(
	Atom type,
	Atom * atom,
	int num
	)
{
	int i ;
	for( i = 0 ; i < num ; i++)
		if( atom[i] == type)
			return i ;
	return 0 ;
}


/* --- global functions --- */

/**set/reset the window's dnd awareness
 *\param win mlterm window
 *\param flag awareness is set when true
 */
void
x_dnd_set_awareness(
	x_window_t * win,
	int flag
	)
{
	if( flag)
	{
		XChangeProperty(win->display, win->my_window,
				XA_DND_AWARE(win->display),
				XA_ATOM, 32, PropModeReplace, 
				&DND_VERSION, 1) ;
	}
	else
	{
		XDeleteProperty(win->display, win->my_window,
				XA_DND_AWARE(win->display)) ;
	}
}
/**send a accept/reject message to the dnd sender
 *\param win mlterm window
 */
void
x_dnd_reply(
	x_window_t * win
	)
{
	XClientMessageEvent reply_msg;
	
	reply_msg.type = ClientMessage;
	reply_msg.display = win->display;
	reply_msg.format = 32;
	reply_msg.window = win->dnd_source;
	reply_msg.message_type = XA_DND_STATUS(win->display);
	reply_msg.data.l[0] = win->my_window;
	if (win->is_dnd_accepting)
	{
		reply_msg.data.l[1] = 0x1 | 0x2; /* accept the drop | use [2][3] */
		reply_msg.data.l[2] = 0;
		reply_msg.data.l[3] = 0;
		reply_msg.data.l[4] = XA_DND_ACTION_COPY(win->display);
	}
	else
	{
		reply_msg.data.l[1] = 0;
		reply_msg.data.l[2] = 0;
		reply_msg.data.l[3] = 0;
		reply_msg.data.l[4] = 0;
	}
	
	XSendEvent(win->display, reply_msg.window, False, 0, (XEvent*)&reply_msg);
}

/**send finish message to dnd sender
 *\param win mlterm window 
 */
void
x_dnd_finish(
	x_window_t *  win 
	)
{
	XClientMessageEvent reply_msg ;
	
	if( win->dnd_source)
	{
		reply_msg.message_type = XA_DND_FINISH(win->display) ;
		reply_msg.data.l[0] = win->my_window ;
		/* setting bit 0 means success */
		reply_msg.data.l[1] = 1 ; 
		reply_msg.data.l[2] = XA_DND_ACTION_COPY(win->display) ;
		reply_msg.type = ClientMessage ;
		reply_msg.format = 32 ;
		reply_msg.window = win->dnd_source ;
		reply_msg.display = win->display ;
		
		XSendEvent(win->display, win->dnd_source, False, 0,
			  (XEvent*)&reply_msg) ;
		win->dnd_source = 0 ;
	}
}

/**parse dnd data and send them to the pty
 *\param win mlterm window
 *\param atom type of data
 *\param src data from dnd
 *\param len size of data in bytes
 */
int
x_dnd_parse(
	x_window_t * win,
	Atom atom,
	
	unsigned char *src,
	int len)
{
	char * atom_name;
	char * charset;
	int charset_code;

	if(!src)
		return 1 ;
	if(!atom)
		return 1;

	atom_name = XGetAtomName( win->display, atom);

	/* process charset directive */
	if( (charset = strchr( atom_name, ';')) )
	{
		if(strncmp(charset+1, "charset", 7)
		    || strncmp(charset+1, "CHARSET", 7))
		{
			/* remove charset=... and re-read atom */
			*charset = 0; 
			charset = strchr( charset +1, '=') +1 ;
			charset_code = charset_name2code( charset) ;
			/* atom_name is now terminated before ";charset=xxx" */
			atom = XInternAtom( win->display, atom_name, False) ;
			XFree( atom_name) ;
			if( !atom)
				return 1;
		}
		else
		{
			XFree(atom_name);
		}
	}
	else
	{
		XFree(atom_name);
	}
	    
	/* COMPOUND_TEXT */
	if( atom == XA_COMPOUND_TEXT(win->display))
	{
		if( !(win->xct_selection_notified))
			return 0 ;
		(*win->xct_selection_notified)( win , src , len) ;
		return 1 ;
	}
	/* UTF8_STRING */
	if( atom == XA_UTF8_STRING(win->display) )
	{
		if( !(win->utf8_selection_notified))
			return 0 ;
		(*win->utf8_selection_notified)( win , src , len) ;
		return 1 ;
	}
	/* text/unicode */
	if( atom == XA_DND_MIME_TEXT_UNICODE(win->display))
	{
		int filled_len ;
		mkf_parser_t * parser ;
		mkf_conv_t * conv ;
		char conv_buf[512] = {0};

		if( !(win->utf8_selection_notified))
			return 0 ;

		if( !(conv = mkf_utf8_conv_new()))
		{
			return 0 ;
		}
		if( !(parser = mkf_utf16_parser_new()))
		{
			(conv->delete)(conv) ;
			return 0 ;
		}

		(parser->init)( parser) ;
		if ( (src[0] == 0xFF || src[0] == 0xFE)
		     && (src[1] == 0xFF || src[1] == 0xFE)
		     && (src[0] != src[1]))
		{			
			/* src sequence seems to have a valid BOM and *
			 * should be processed correctly */
		}
		else
		{
			/* try to set parser state by sending BOM */
			u_int16_t BOM[] =  {0xFEFF};
			
			(parser->set_str)( parser , (char *)BOM , 2) ;
			(parser->next_char)( parser , 0) ;	     
		}

		(parser->set_str)( parser , src , len) ;
		/* conversion from utf16 -> utf8. */
		while( ! parser->is_eos)
		{
			filled_len = (conv->convert)( conv,
						      conv_buf,
						      sizeof(conv_buf),
						      parser) ;
			if(filled_len ==0) 
				break ;
			(*win->utf8_selection_notified)( win,
							 conv_buf, 
							 filled_len) ;
                }
		(conv->delete)( conv) ;
		(parser->delete)( parser) ;

		return 1 ;
	}
	/* text/plain */
	if( atom == XA_DND_MIME_TEXT_PLAIN(win->display))
	{
		if( !(win->utf8_selection_notified))
			return 0 ; /* needs ASCII capable parser*/
		(*win->utf8_selection_notified)( win , src , len) ;
		return 1 ;
	}
	/* TEXT */
	if( atom == XA_TEXT(win->display) )
	{
		if( !(win->utf8_selection_notified))
			return 0 ;
		(*win->utf8_selection_notified)( win , src , len) ;
		return 1 ;
	}
	/* text/url-list */
	if( atom == XA_DND_MIME_TEXT_URI_LIST(win->display))
	{
		int pos ;
		unsigned char *delim ;

		if( !(win->utf8_selection_notified))
			return 0 ;
		pos = 0 ;
		delim = src ;
		while( pos < len){
			delim = strchr( &(src[pos]), 13) ;
			if( delim )
			{
				/* output one ' ' as a separator */
				*delim = ' ' ; 
			}
			else
			{
				delim = &(src[len -1]) ;
			}
			if( strncmp( &(src[pos]), "file:",5) == 0)
			{
				/* remove garbage("file:").
				   new length should be (length - "file:" + " ")*/
				(*win->utf8_selection_notified)( win ,
								 &(src[pos+5]),
								 (delim - &(src[pos])) -5 +1 ) ;
			}
			else
			{
				/* original string + " " */
				(*win->utf8_selection_notified)( win ,
								 &(src[pos]),
								 (delim - &(src[pos])) +1) ;
			}
			/* skip trailing 0x0A */
			pos = (delim - src) +2 ; 
		}
	}
	return 0 ;
}

Atom
x_dnd_preferable_atom(
	x_window_t *  win ,
	Atom *  atom,
	int  num
	)
{
	int  i = 0 ;

	i = is_pref( XA_COMPOUND_TEXT( win->display), atom, num) ;
	if(!i)
		i = is_pref( XA_UTF8_STRING( win->display), atom, num) ;
	if(!i)
		i = is_pref( XA_TEXT( win->display), atom, num) ;
	if(!i)
		i = is_pref( XA_DND_MIME_TEXT_PLAIN( win->display), atom, num) ;
	if(!i)
		i = is_pref( XA_DND_MIME_TEXT_UNICODE( win->display), atom, num) ;
	if(!i)
		i = is_pref( XA_DND_MIME_TEXT_URI_LIST( win->display), atom, num) ;
		
#ifdef  DEBUG
	if( i)
	{
		char *  p ;
		p = XGetAtomName( win->display, atom[i]);
		if( p)
		{
			kik_debug_printf( "accepted as atom: %s(%d)\n",
					  p, atom[i]) ;
			XFree( p) ;
		}
	}
	else
	{
		char *  p ;
		for( i = 0 ; i < num ; i++)
			if( atom[i])
			{
				p = XGetAtomName( win->display, atom[i]);
				kik_debug_printf("dropped atoms: %d\n",
						 XGetAtomName( win->display,
							       atom[i])) ;
				XFree( p) ;
			}
	}
#endif
	if( i)
		return atom[i] ;
	else
		return (Atom)0  ;/* 0 would never be used for Atom */
}

