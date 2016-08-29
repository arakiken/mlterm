/*
 *	$Id$
 */

#ifndef __UI_GDIOBJ_POOL_H__
#define __UI_GDIOBJ_POOL_H__

#include <pobl/bl_types.h>

#include "ui.h"

int ui_gdiobj_pool_init(void);

int ui_gdiobj_pool_final(void);

HPEN ui_acquire_pen(u_long rgb);

int ui_release_pen(HPEN pen);

HBRUSH ui_acquire_brush(u_long rgb);

int ui_release_brush(HBRUSH brush);

#endif
