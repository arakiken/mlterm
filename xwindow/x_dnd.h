/*
 *	$Id$
 */

/*
 * Drag and Drop stuff.
 */
#define  XA_DND_DROP(display) (XInternAtom(display, "XdndDrop", False))
#define  XA_DND_AWARE(display) (XInternAtom(display, "XdndAware", False))
#define  XA_DND_ENTER(display) (XInternAtom(display, "XdndEnter", False))
#define  XA_DND_TYPE_LIST(display) (XInternAtom(display, "XdndTypeList", False))
#define  XA_DND_STATUS(display) (XInternAtom(display, "XdndStatus", False))
#define  XA_DND_POSITION(display) (XInternAtom(display, "XdndPosition", False))
#define  XA_DND_STORE(display) (XInternAtom(display, "MLTERM_DND", False))
#define  XA_DND_ACTION_COPY(display) (XInternAtom(display, "XdndActionCopy", False))
#define  XA_DND_SELECTION(display) (XInternAtom(display, "XdndSelection", False))
#define  XA_DND_FINISH(display) (XInternAtom(display, "XdndFinished", False))
#define  XA_DND_MIME_TEXT_PLAIN(display) (XInternAtom(display, "text/plain", False))
#define  XA_DND_MIME_TEXT_UNICODE(display) (XInternAtom(display, "text/unicode", False))
#define  XA_DND_MIME_TEXT_URL_LIST(display) (XInternAtom(display, "text/uri-list", False))

void x_dnd_set_awareness( x_window_t * win, int flag );

int x_dnd_parse( x_window_t * win, Atom atom, unsigned char *src, int len);

void x_dnd_finish( x_window_t *  win) ;

void x_dnd_reply( x_window_t *  win) ;

Atom x_dnd_preferable_atom( x_window_t *  win , Atom *atom, int num);

