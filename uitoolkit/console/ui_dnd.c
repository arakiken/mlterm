/*
 * $Id$
 */

#ifndef DISABLE_XDND

#include "../ui_window.h"
#include "../ui_dnd.h"

/* --- global functions --- */

/*
 * XFilterEvent(event, w) analogue.
 * return 0 if the event should be processed in the mlterm mail loop.
 * return 1 if nothing to be done is left for the event.
 */
int ui_dnd_filter_event(XEvent* event, ui_window_t* win) { return 0; }

#endif /* DISABLE_XDND */
