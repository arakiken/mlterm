/*
 *	$Id$
 */

#include  "kik_config.h"	/* PTY_XXX */


#if defined(PTY_STREAMS)

#include  "kik_pty_streams.c"

#elif defined(PTY_BSD)

#include  "kik_pty_bsd.c"

#else

#include  "kik_pty_bsd.c"

#endif
