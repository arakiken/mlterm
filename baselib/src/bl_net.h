/*
 *	$Id$
 */

#ifndef __BL_NET_H__
#define __BL_NET_H__

#ifdef USE_WIN32API

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501 /* for getaddrinfo */
#include <windows.h>
#include <ws2tcpip.h> /* addrinfo */

#else /* USE_WIN32API */

#include "bl_types.h" /* socklen_t */
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>

#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif

#ifndef PF_LOCAL
#ifdef PF_UNIX
#define PF_LOCAL PF_UNIX
#else
#define PF_LOCAL AF_LOCAL
#endif
#endif

#endif /* USE_WIN32API */

#endif
