/*
 *	$Id$
 */

/*
 * Drag and Drop stuff.
 */
#define  XA_DND_DROP(display) (XInternAtom(display, "XdndDrop", False))
#define  XA_DND_ENTER(display) (XInternAtom(display, "XdndEnter", False))
#define  XA_DND_POSITION(display) (XInternAtom(display, "XdndPosition", False))
#define  XA_DND_STORE(display) (XInternAtom(display, "MLTERM_DND", False))

typedef struct x_dnd_context {
	Window  source ;
	Atom  waiting_atom ;
} x_dnd_context_t ;

void x_dnd_set_awareness( x_window_t * win, int version );

Atom x_process_selection( x_window_t *  win , XEvent * event);

Atom x_process_incr( x_window_t *  win , XEvent * event);

Atom x_process_position( x_window_t *  win , XEvent * event);

Atom x_process_drop( x_window_t *  win , XEvent * event);

Atom x_process_enter( x_window_t *  win , XEvent * event);
