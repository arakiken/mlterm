/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __C_INTL_H__
#define __C_INTL_H__

#include "c_config.h"

#define _(arg) gettext(arg)
#define N_(arg) arg

#include "gettext.h"

#endif
