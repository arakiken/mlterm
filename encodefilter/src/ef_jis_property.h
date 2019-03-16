/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_JIS_PROPERTY_H__
#define __EF_JIS_PROPERTY_H__

#include <pobl/bl_types.h> /* u_char */

#include "ef_property.h"

ef_property_t ef_get_jisx0208_1983_property(u_char *ch);

ef_property_t ef_get_jisx0213_2000_1_property(u_char *ch);

#endif
