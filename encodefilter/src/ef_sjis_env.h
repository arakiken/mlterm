/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_SJIS_ENV_H__
#define __EF_SJIS_ENV_H__

typedef enum ef_sjis_type_t {
  APPLE_CS,
  MICROSOFT_CS,
  MS_CS_WITH_NECDOS_9_12 /* chars(9 - 12 section) not included in MS charset. */

} ef_sjis_type_t;

void ef_set_sjis_input_type(ef_sjis_type_t type);

void ef_set_sjis_output_type(ef_sjis_type_t type);

ef_sjis_type_t ef_get_sjis_input_type(void);

ef_sjis_type_t ef_get_sjis_output_type(void);

#endif
