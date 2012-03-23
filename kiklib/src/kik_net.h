/*
 *	$Id$
 */

#ifndef  __KIK_NET_H__
#define  __KIK_NET_H__


#include  "kik_types.h"		/* socklen_t */

#ifndef  USE_WIN32API

#include  <sys/types.h>
#include  <sys/socket.h>	
#include  <sys/un.h>


#ifndef  AF_LOCAL
#define  AF_LOCAL  AF_UNIX
#endif

#ifndef  PF_LOCAL
#ifdef PF_UNIX
#define  PF_LOCAL  PF_UNIX
#else
#define  PF_LOCAL  AF_LOCAL
#endif
#endif

#endif /* USE_WIN32API */


#endif
