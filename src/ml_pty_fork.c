/*
 *	update: <2001/11/7(18:14:20)>
 *	$Id$
 */

#if defined(PTY_STREAMS)

#include  "ml_pty_fork_streams.c"

#elif defined(PTY_BSD)

#include  "ml_pty_fork_bsd.c"

#else

#include  "ml_pty_fork_bsd.c"

#endif
