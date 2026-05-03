/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __DAEMON_H__
#define __DAEMON_H__

#include <pobl/bl_def.h> /* USE_WIN32API */

#ifndef NO_DAEMON
#if defined(USE_WIN32API) || defined(USE_QUARTZ) || defined(USE_BEOS) || defined(USE_FRAMEBUFFER)
#define NO_DAEMON
#endif
#endif

#ifndef NO_DAEMON

int daemon_init(void);

int daemon_final(void);

#else

#define daemon_init() (0)
#define daemon_final() (0)
#define daemon_get_fd() (-1)

#endif

#endif
