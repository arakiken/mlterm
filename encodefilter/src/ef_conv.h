/*
 *	$Id$
 */

#ifndef __EF_CONV_H__
#define __EF_CONV_H__

#include <pobl/bl_types.h> /* size_t */

#include "ef_parser.h"

typedef struct ef_conv {
  void (*init)(struct ef_conv *);
  void (*delete)(struct ef_conv *);
  size_t (*convert)(struct ef_conv *, u_char *, size_t, ef_parser_t *);
  size_t (*illegal_char)(struct ef_conv *, u_char *, size_t, int *, ef_char_t *);

} ef_conv_t;

#endif
