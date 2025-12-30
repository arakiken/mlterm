/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_PROPERTY_H__
#define __EF_PROPERTY_H__

/* Do not forget to change the size of ef_char_t::property according to this. */
typedef enum ef_property {
  EF_COMBINING = 0x1u,

  /* only for UCS */
  EF_FULLWIDTH = 0x2u,
  EF_AWIDTH = 0x4u

} ef_property_t;

#endif
