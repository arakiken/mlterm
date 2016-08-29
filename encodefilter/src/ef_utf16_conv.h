/*
 *	$Id$
 */

#ifndef __EF_UTF16_CONV_H__
#define __EF_UTF16_CONV_H__

#include "ef_conv.h"

ef_conv_t* ef_utf16_conv_new(void);

ef_conv_t* ef_utf16le_conv_new(void);

int ef_utf16_conv_use_bom(ef_conv_t* conv);

#endif
