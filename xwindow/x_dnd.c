/** @file
 * @brief X Drag and Drop protocol support
 *
 *	$Id$
 */

#include  "x_window.h"
#include  "x_dnd.h"
#include  <stdio.h>
#include  <string.h>
#include  <X11/Xatom.h>
#include  <X11/Xutil.h>
#include  <mkf/mkf_utf8_conv.h>
#include  <mkf/mkf_utf16_parser.h>

#define  SUCCESS 0
#define  FAILURE -1

/* XXX: should we cache this atom for decrease round-trip? */
#define  XA_DND_STORE(display) (XInternAtom(display, "MLTERM_DND", False))

/* following define should be consistent with the counterpart in x_window.c */
#define  XA_INCR(display) (XInternAtom(display, "INCR", False))

/* For now, we need a pointer to this structure in the x_window_t
 * to keep track of DND. It should be removed someday ...*/
typedef struct x_dnd_context {
	Window  source ;
	Atom  waiting_atom ;
	int  is_incr ;
	mkf_parser_t * parser ;
	mkf_conv_t * conv ;
} x_dnd_context_t ;

typedef struct dnd_parser {
	char *  atomname ;
	int  (*parser)(x_window_t *, unsigned char *, int) ;
} dnd_parser_t ;

/********************** parsers **********************************/
/* XXX: to properly support DnD spec v5, parsers should accept "codeset"*/
static int
parse_text_unicode(
	x_window_t *  win,
	unsigned char *  src,
	int  len)
{
	int filled_len ;
	mkf_parser_t * parser ;
	mkf_conv_t * conv ;
	unsigned char conv_buf[512] = {0};

	if( !(win->utf8_selection_notified))
		return  FAILURE ;

	if( (conv = win->dnd->conv) &&
	    (parser = win->dnd->parser) && (win->dnd->is_incr))
	{
		if( len == 0)
		{
			/* the incr session was finished */
			(conv->delete)( conv) ;
			win->dnd->conv = NULL ;
			(parser->delete)( parser) ;
			win->dnd->parser = NULL ;
#ifdef  DEBUG
			kik_debug_printf("freed parser/converter\n" ) ;
#endif
			return  SUCCESS ;
		}
#ifdef  DEBUG
		kik_debug_printf("recycling parser/converter %d, %p, %p\n",
				 win->dnd->is_incr,
				 conv,
				 parser ) ;
#endif
	}
	else
	{
		if( win->dnd->conv)
			(win->dnd->conv->delete)(win->dnd->conv) ;
		if( win->dnd->parser)
			(win->dnd->parser->delete)(win->dnd->parser) ;

		if( !(conv = mkf_utf8_conv_new()))
			return  FAILURE ;
		if( !(parser = mkf_utf16_parser_new()))
		{
			(conv->delete)(conv) ;
			return  FAILURE ;
		}

		/* initialize the parser's endian. */
		(parser->init)( parser) ;
		if ( (src[0] == 0xFF || src[0] == 0xFE)
		     && (src[1] == 0xFF || src[1] == 0xFE)
		     && (src[0] != src[1]))
		{
			/* src sequence seems to have a valid BOM and *
			 * should initialize parser correctly */
		}
		else
		{
			/* try to set parser state depending on
			   your machine's endianess by sending BOM */
			/* XXX: it's not spec comformant and someteime fails */
			u_int16_t BOM[] =  {0xFEFF};

			(parser->set_str)( parser , (unsigned char *)BOM , 2) ;
			(parser->next_char)( parser , 0) ;
		}
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
	if( win->dnd->is_incr)
	{
/* keep pointers to parser/converter to use them for next event */
		win->dnd->parser = parser ;
		win->dnd->conv = conv ;
	}
	else
	{
		(conv->delete)( conv) ;
		win->dnd->conv = NULL ;
		(parser->delete)( parser) ;
		win->dnd->parser = NULL ;
	}

	return  SUCCESS ;
}

static int
parse_text_uri_list(
	x_window_t *  win,
	unsigned char *  src,
	int  len)
{
	int pos ;
	unsigned char *  delim ;

	if( !(win->utf8_selection_notified))
		return  FAILURE ;
	pos = 0 ;
	src[len-1] = '\0'; /* force termination for malicious peers */

	delim = src ;
	while( pos < len -1){
		/* according to RFC, 0x0d is the delimiter */
		delim = (unsigned char *)strchr( (char *)(src + pos), 13) ;
		if( delim )
		{
			/* output one ' ' as a separator */
			*delim = ' ' ;
		}
		else
		{
			delim = src + len -1 ;
		}
		if( strncmp( (char *)(src + pos), "file:",5) == 0)
		{
			/* remove garbage("file:").
			   new length should be (length - "file:" + " ")*/
			(*win->utf8_selection_notified)( win ,
				(unsigned char *)(src + pos + 5),
				delim -  src- pos -5 +1 ) ;
		}
		else
		{
			/* original string + " " */
			(*win->utf8_selection_notified)( win ,
				(unsigned char *)(src + pos),
				delim - src- pos +1) ;
		}
		/* skip trailing 0x0A */
		pos = (delim - src) +2 ;
	}

	return  SUCCESS;
}

static int
parse_compound_text(
	x_window_t *  win,
	unsigned char *  src,
	int  len)
{
	if( !(win->xct_selection_notified))
		return  FAILURE ;
	(*win->xct_selection_notified)( win , src , len) ;

	return  SUCCESS ;
}

static int
parse_text(
	x_window_t *  win,
	unsigned char *  src,
	int  len)
{
	if( !(win->utf8_selection_notified))
		return  FAILURE ;
	(*win->utf8_selection_notified)( win , src , len) ;

	return  SUCCESS ;
}

static int
parse_utf8_string(
	x_window_t *  win,
	unsigned char *  src,
	int  len)
{
	if( !(win->utf8_selection_notified))
		return  FAILURE ;
	(*win->utf8_selection_notified)( win , src , len) ;

	return  SUCCESS ;
}

static int
parse_mlterm_config(
	x_window_t *  win,
	unsigned char *  src,
	int  len)
{
	char *  value ;

	src[len-1] = '\0'; /* force termination for malicious peers */

	if( !(win->set_xdnd_config))
		return  FAILURE ;
	value = strchr( (char *)src, '=') ;
	if( !value)
		return  FAILURE ;
	*value = 0 ;
#ifdef  DEBUG
	kik_debug_printf("conf key %s val %s\n",src, value) ;
#endif
	(*win->set_xdnd_config)( win ,
				 NULL, /* dev */
				 (char *)src, /* key */
				 value +1 /* value */) ;

	return  SUCCESS ;
}

static int
parse_app_color(
	x_window_t *  win,
	unsigned char *  src,
	int  len)
{
	u_int16_t *r, *g, *b;
	unsigned char buffer[25];

	r = (u_int16_t *)src ;
	g = r + 1 ;
	b = r + 2 ;
	sprintf( (char *)buffer, "bg_color=#%02x%02x%02x",
		 (*r) >> 8,
		 (*g) >> 8,
		 (*b) >> 8) ;
#ifdef  DEBUG
	kik_debug_printf( "bgcolor: %s\n" , buffer) ;
#endif
	parse_mlterm_config( win, buffer, 0) ;

	return  SUCCESS ;
}

static int
parse_prop_bgimage(
	x_window_t *  win,
	unsigned char *  src,
	int  len)
{
	char *  head ;
	char  tail;

	if( !(win->set_xdnd_config))
		return  FAILURE ;

	tail = src[len -1] ;
	src[len -1] = 0 ;

	if( (head = strstr( (char *)src, "file://")))
	{
/* format should be file://<host>/<path> */
		memmove( src, head+7, len - 6 - ((char *)src - head));
		src[strlen( (char *)src)] = tail ;
		src[strlen( (char *)src)] = 0 ;
		if( !(head = strstr( (char *)src, "/")))
			return  FAILURE ;
		/* and <host> should be localhost and safely ignored.*/
		src = (unsigned char *)head ;

		/* remove trailing garbage */
		if( (head = strstr( (char *)src, "\r")))
			*head = 0 ;
		if( (head = strstr( (char *)src, "\n")))
			*head = 0 ;
	}
	else
	{
/* other schemas (like "http" --call wget?) may be supported here */
	}
#ifdef  DEBUG
	kik_debug_printf( "bgimage: %s\n" , src) ;
#endif
	(*win->set_xdnd_config)( win ,
				 NULL, /* dev */
				 "wall_picture", /* key */
				 (char *)src /* value */) ;

	return  SUCCESS ;
}

#ifdef  DEBUG
static int
parse_debug(
	x_window_t *  win,
	unsigned char *  src,
	int  len)
{
	int i;

	kik_debug_printf( ">%s<\n", (char *)src) ;
	for( i = 0 ; i < 100 && i < len ; i++)
		kik_debug_printf( "\n%d %x" ,i, src[i]) ;

	return  SUCCESS ;
}
#endif

/* new mime type and parser pair should be added into this table */
dnd_parser_t dnd_parsers[] ={
	{"text/x-mlterm.config"  , parse_mlterm_config } ,
	{"UTF8_STRING"  , parse_utf8_string } ,
	{"COMPOUND_TEXT", parse_compound_text } ,
	{"TEXT"         , parse_text } ,
	{"application/x-color"  , parse_app_color } ,
	{"property/bgimage"  , parse_prop_bgimage } ,
	{"x-special/gnome-reset-background"  , parse_prop_bgimage },
	{"text/uri-list", parse_text_uri_list } ,
	{"text/unicode"   , parse_text_unicode } ,
	{"text/plain"   , parse_utf8_string } ,
/* nobody would use following...
	{"GIMP_PATTERN"  , parse_utf8_string } ,
	{"GIMP_BRUSH"  , parse_utf8_string } ,
	{"GIMP_GRADIENT"  , parse_utf8_string } ,
	{"GIMP_IMAGEFILE"  , parse_utf8_string } ,
*/
	{NULL, NULL}
};
/************************** static functions *************************/
static int
ignore_badwin(
	Display *  display,
	XErrorEvent *  event
	)
{
	char  buffer[1024] ;

	XGetErrorText( display , event->error_code , buffer , 1024) ;

	kik_error_printf( "%s\n" , buffer) ;

	switch( event->error_code)
	{
	case BadWindow:
		return  1;
	default:
		abort() ;
	}
}

static void
set_badwin_handler(
	int  flag
	)
{
	static XErrorHandler  old;

	if( flag)
	{
		if( !old)
			old = XSetErrorHandler( ignore_badwin) ;
	}
	else
	{
		if( old)
		{
			XSetErrorHandler( old) ;
			old = NULL ;
		}
	}
}

static int
finalize_context(
	x_window_t * win
	)
{
	if( !(win->dnd))
		return  FAILURE ;

	if( win->dnd->conv)
		(win->dnd->conv->delete)(win->dnd->conv) ;
	if( win->dnd->parser)
		(win->dnd->parser->delete)(win->dnd->parser) ;

	free( win->dnd) ;
	win->dnd = NULL ;

	return  SUCCESS ;
}

/* seek atom array and return the index */
static int
is_pref(
	Atom type,
	Atom * atom_list,
	int num
	)
{
	int i ;
	for( i = 0 ; i < num ; i++)
		if( atom_list[i] == type)
			return i ;
	return  FAILURE ;
}

/**send a accept/reject message to the dnd sender
 *\param win mlterm window
 */
static void
reply(
	x_window_t * win
	)
{
	XClientMessageEvent  msg ;

	msg.type = ClientMessage ;
	msg.display = win->display ;
	msg.format = 32 ;
	msg.window = win->dnd->source ;
	msg.message_type = XInternAtom( win->display, "XdndStatus", False) ;
	msg.data.l[0] = win->my_window ;
	if( win->dnd->waiting_atom)
	{
		msg.data.l[1] = 0x1 | 0x2 ; /* accept the drop | use [2][3] */
		msg.data.l[2] = 0 ;
		msg.data.l[3] = 0 ;
		msg.data.l[4] = XInternAtom( win->display,
					     "XdndActionCopy", False) ;
	}
	else
	{
		msg.data.l[1] = 0 ;
		msg.data.l[2] = 0 ;
		msg.data.l[3] = 0 ;
		msg.data.l[4] = 0 ;
	}

	set_badwin_handler(1) ;
	XSendEvent(win->display, msg.window, False, 0, (XEvent*)&msg) ;
	set_badwin_handler(0) ;
}

/**send finish message to dnd sender
 *\param win mlterm window
 */
static int
finish(
	x_window_t *  win
	)
{
	XClientMessageEvent msg ;

	if( !(win->dnd))
		return  FAILURE;
	if( !(win->dnd->source))
		return  FAILURE;

#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG "saying good bye\n") ;
#endif

	msg.message_type = XInternAtom( win->display, "XdndFinished", False) ;
	msg.data.l[0] = win->my_window ;
	/* setting bit 0 means success */
	msg.data.l[1] = 1 ;
	msg.data.l[2] = XInternAtom( win->display, "XdndActionCopy", False) ;
	msg.type = ClientMessage ;
	msg.format = 32 ;
	msg.window = win->dnd->source ;
	msg.display = win->display ;

	set_badwin_handler(1) ;
	XSendEvent(win->display, win->dnd->source, False, 0, (XEvent*)&msg) ;
	set_badwin_handler(0) ;

	win->dnd->source = 0 ;

	return  SUCCESS ;
}

/**parse dnd data and send them to the pty
 *\param win mlterm window
 *\param atom type of data
 *\param src data from dnd
 *\param len size of data in bytes
 */
static int
parse(
	x_window_t * win,
	unsigned char *src,
	int len)
{
	dnd_parser_t *  proc_entry ;

	if( !src)
		return  FAILURE ;
	if( !(win->dnd))
		return  FAILURE;

	if( !(win->dnd->waiting_atom))
		return  FAILURE ;

	for( proc_entry = dnd_parsers ; proc_entry->atomname ; proc_entry++)
	{
		if( (win->dnd->waiting_atom) ==
		    XInternAtom( win->display, proc_entry->atomname, False) )
			break ;
	}

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "processing as  %s\n",
			  proc_entry->atomname) ;
#endif
	if( proc_entry->parser)
		return  (proc_entry->parser)( win, src, len) ;

	return  FAILURE ;
}


/* i is used as an index for an array of atom.
 * i = -1 means "nothing"
 *
 * returned value is the atom found and NOT THE INDEX
 * and 0 means "nothing"
 */

static Atom
choose_atom(
	x_window_t *  win ,
	Atom *  atom_list,
	int  num
	)
{
	dnd_parser_t *  proc_entry ;
	int  i;

#ifdef  DEBUG
	char *  atom_name ;
	for( i = 0; i < num; i++){
		if( !atom_list[i])
			break;
		atom_name = XGetAtomName( win->display, atom_list[i]) ;
		if( atom_name)
		{
			kik_debug_printf( KIK_DEBUG_TAG "candidate #%d: %s\n",
					  i, atom_name) ;
			XFree( atom_name) ;
		}
	}
#endif

	i = -1;
	for( proc_entry = dnd_parsers; i< 0 && proc_entry->atomname ;proc_entry++)
	{
#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG "ckecking against : %s\n",
				  proc_entry->atomname) ;
#endif
		i = is_pref( XInternAtom( win->display,
					  proc_entry->atomname,
					  False),
			     atom_list, num) ;
	}
	if( i < 0)
		return (Atom)0  ;/* 0 would never be used for Atom */

#ifdef  DEBUG
	atom_name = XGetAtomName( win->display, atom_list[i]) ;
	if( atom_name)
	{
		kik_debug_printf( KIK_DEBUG_TAG "accepted: %s(%d)\n",
				  atom_name, atom_list[i]) ;
		XFree( atom_name) ;
	}
#endif

	return atom_list[i] ;
}

/* --- global functions --- */

/**set/reset the window's dnd awareness
 *\param win mlterm window
 *\param flag awareness is set when true
 */
static void
awareness(
	x_window_t * win,
	int version
	)
{
	set_badwin_handler(1) ;
	XChangeProperty( win->display, win->my_window,
			 XInternAtom( win->display, "XdndAware", False),
			 XA_ATOM, 32, PropModeReplace,
			 (unsigned char *)(&version), 1) ;
	set_badwin_handler(0) ;
}

static int
enter(
	x_window_t *  win,
	XEvent *  event
	)
{
	Atom  to_wait ;

	/* more than 3 type is available? */
	if (event->xclient.data.l[1] & 0x01)
	{
		Atom  act_type;
		int  act_format;
		unsigned long  nitems, left ;
		Atom *  dat ;
		int  result ;

		set_badwin_handler(1) ;
		result = XGetWindowProperty( win->display,
					     event->xclient.data.l[0],
					     XInternAtom( win->display,
							  "XdndTypeList",
							  False),
					     0L, 1024L, False, XA_ATOM,
					     &act_type,
					     &act_format, &nitems, &left,
					     (unsigned char **)(&dat)) ;
		set_badwin_handler(0) ;

		if( result != Success)
			return  FAILURE ;

		if( act_type != None)
		{
			to_wait = choose_atom( win , dat, nitems) ;
		}
		else
		{
			to_wait = None ;
		}
		XFree( dat) ;
	}
	else
	{
		/* less than 3 candidates */
		to_wait = choose_atom( win ,
				       (Atom *)(event->xclient.data.l+2),
				       3) ;
	}

	if( !(to_wait))
	{
		finalize_context( win) ;

		return  FAILURE ;
	}

	if( !(win->dnd))
		win->dnd = malloc( sizeof( x_dnd_context_t)) ;
	if( !(win->dnd))
		return  FAILURE ;
	win->dnd->source = event->xclient.data.l[0];
	win->dnd->waiting_atom = to_wait;
	win->dnd->is_incr = 0 ;
	win->dnd->parser = NULL ;
	win->dnd->conv = NULL ;
#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "choosed atom:%d  on %p\n",
			  to_wait, win->dnd) ;
#endif

	return  SUCCESS ;
}

static int
position(
	x_window_t *  win,
	XEvent *  event
	)
{
	if( !(win->dnd))
		return  FAILURE ;

	if( win->dnd->source != event->xclient.data.l[0])
	{
#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG "WID mismatched.\n") ;
#endif
		finalize_context( win) ;

		return  FAILURE ;
	}

	reply( win) ;

	return  SUCCESS ;
}

static int
drop(
	x_window_t *  win,
	XEvent *  event
	)
{
	if( !(win->dnd))
		return  FAILURE ;

	if( win->dnd->source != event->xclient.data.l[0])
	{
#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG "WID mismatched.\n") ;
#endif
		finalize_context( win) ;

		return  FAILURE ;
	}

	/* data request */
	set_badwin_handler(1) ;
	XConvertSelection( win->display, XInternAtom( win->display,
						      "XdndSelection", False),
			   win->dnd->waiting_atom, /* mime type */
			   XA_DND_STORE(win->display),
			   win->my_window,
			   event->xclient.data.l[2]) ;
	set_badwin_handler(0) ;

	return  SUCCESS ;
}

static int
incr(
	x_window_t *  win,
	XEvent *  event
	)
{
	u_long  bytes_after ;
	XTextProperty  ct ;
	int result ;

	if( !(win->dnd))
		return  FAILURE ;

	/* remember that it's an incremental transfer */
	win->dnd->is_incr = 1 ;

	/* dummy read to determine data length */
	set_badwin_handler(1) ;
	result = XGetWindowProperty( win->display, event->xproperty.window,
				     event->xproperty.atom, 0, 0, False,
				     AnyPropertyType, &ct.encoding, &ct.format,
				     &ct.nitems, &bytes_after, &ct.value) ;
	set_badwin_handler(0) ;
	if( result != Success)
		return  FAILURE ;

	/* ignore when ct.encoding != XA_INCR */
	if( ct.encoding != XA_INCR(win->display))
	{
#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG "ignored.\n") ;
#endif
		if( ct.value)
			XFree( ct.value) ;
		return  FAILURE ;
	}

	set_badwin_handler(1) ;
	result = XGetWindowProperty( win->display , event->xproperty.window ,
			    event->xproperty.atom , 0 , bytes_after , False ,
			    AnyPropertyType , &ct.encoding , &ct.format ,
			    &ct.nitems , &bytes_after , &ct.value) ;
	set_badwin_handler(0) ;

	if( result != Success)
		return  FAILURE ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "INCR: %d\n", ct.nitems) ;
#endif
	parse( win, ct.value, ct.nitems) ;

	if( ct.nitems == 0)
	{       /* all data have been received */
#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG "terminating.\n") ;
#endif

		finish( win) ;
		finalize_context( win) ;
	}

	if( ct.value)
		XFree( ct.value) ;

	/* This delete will trigger the next update*/
	set_badwin_handler(1) ;
	XDeleteProperty( win->display, event->xproperty.window,
			 event->xproperty.atom) ;
	set_badwin_handler(0) ;

	return  SUCCESS ;
}

static int
selection(
	x_window_t *  win,
	XEvent *  event
	)
{
	u_long  bytes_after ;
	XTextProperty  ct ;
	int  seg = 0 ;
	int  result ;

	if( !(win->dnd))
		return  FAILURE ;

	/* dummy read to determine data length */
	set_badwin_handler(1) ;
	result = XGetWindowProperty( win->display,
				     event->xselection.requestor,
				     event->xselection.property, 0, 0,
				     False,AnyPropertyType,
				     &ct.encoding, &ct.format ,
				     &ct.nitems , &bytes_after , &ct.value) ;
	set_badwin_handler(0) ;

	if( result != Success)
	{
#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG "couldn't get property. \n") ;
#endif
		finalize_context( win) ;

		return  FAILURE ;
	}
	if( ct.value)
		XFree( ct.value) ;
	if( ct.encoding == XA_INCR(win->display))
		return  SUCCESS ;

	while( bytes_after > 0)
	{
		set_badwin_handler(1) ;
		result = XGetWindowProperty( win->display,
					     event->xselection.requestor,
					     event->xselection.property,
					     seg / 4, 4096, False,
					     AnyPropertyType,
					     &ct.encoding, &ct.format,
					     &ct.nitems, &bytes_after,
					     &ct.value) ;
		set_badwin_handler(0) ;

		if(result != Success)
		{
#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG "couldn't get property. \n") ;
#endif
			finalize_context( win) ;

			return  FAILURE ;
		}
		parse( win, ct.value, ct.nitems) ;
		XFree( ct.value) ;

		seg += ct.nitems ;
	}

	finish( win) ;
	finalize_context( win) ;

	return  SUCCESS ;
}

/*****************************************************************************/

/* XFilterEvent(event, w) analogue */
/* return 0 if the event should be processed in the mlterm mail loop */
/* return 1 if nothing to be done is left for the event */
int
x_dnd_filter_event(
	XEvent *  event,
	x_window_t *  win
	)
{
	switch( event->type )
	{
/*	case CreateNotify:*/
	case MapNotify:
		/* CreateNotifyEvent seems to be lost somewhere... */
		awareness( win, 5) ;

		return  0 ;

	case SelectionNotify:
		if( event->xselection.property != XA_DND_STORE(win->display))
			return  0 ;

		selection( win, event) ;

		set_badwin_handler(1) ;
		XDeleteProperty( win->display, event->xselection.requestor,
				 event->xselection.property) ;
		set_badwin_handler(0) ;

		break;

	case ClientMessage:
		if( event->xclient.message_type ==
		    XInternAtom( win->display, "XdndEnter", False))
		{
			enter( win, event) ;
		}
		else if( event->xclient.message_type ==
			 XInternAtom( win->display, "XdndPosition", False))
		{
			position( win, event) ;
		}
		else if( event->xclient.message_type ==
			 XInternAtom( win->display, "XdndDrop", False))
		{
			drop( win, event) ;
		}
		else if ( event->xclient.data.l[0] ==
			  (XInternAtom( win->display , "WM_DELETE_WINDOW" , False)))
		{
			finalize_context( win) ;
			/* the event should also be processed in main loop */
			return  0 ;
		}
		else
		{
			return  0 ;
		}

		break ;

	case PropertyNotify:
		if( event->xproperty.atom != XA_DND_STORE( win->display))
			return  0 ;
		if( event->xproperty.state == PropertyDelete)
		{
			/* ignore delete notify */
			return  1 ;
		}

		incr( win, event) ;

		break ;

	case DestroyNotify:
		finalize_context( win) ;

		return  0 ;
	default:
		return  0 ;
	}

	/* the event was processed. mlterm main loop don't have to know about it*/
	return  1 ;
}
