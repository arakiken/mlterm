/*
 *	$Id$
 */

#if defined(PTY_STREAMS)

#include  "ml_pty_fork_streams.c"

#elif defined(PTY_BSD)

#include  "ml_pty_fork_bsd.c"

#else

#include  "ml_pty_fork_bsd.c"

#endif
