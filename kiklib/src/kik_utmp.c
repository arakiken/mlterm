/*
 *	$Id$
 */

#include  "kik_config.h"

#ifdef  HAVE_SETUTENT

#include  "kik_utmp_sysv.c"

#else

#include  "kik_utmp_bsd.c"

#endif
