/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_GB18030_2000_INTERN_H__
#define __EF_GB18030_2000_INTERN_H__

#include <pobl/bl_types.h> /* u_char */

int ef_decode_gb18030_2000_to_ucs4(u_char* ucs4, u_char* gb18030);

int ef_encode_ucs4_to_gb18030_2000(u_char* gb18030, u_char* ucs4);

#endif
