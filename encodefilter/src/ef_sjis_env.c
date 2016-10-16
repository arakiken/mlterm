/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_sjis_env.h"

/* --- static variables --- */

static ef_sjis_type_t input_type = MICROSOFT_CS;

static ef_sjis_type_t output_type = MICROSOFT_CS;

/* --- global functions --- */

void ef_set_sjis_input_type(ef_sjis_type_t type) { input_type = type; }

void ef_set_sjis_output_type(ef_sjis_type_t type) { output_type = type; }

ef_sjis_type_t ef_get_sjis_input_type(void) { return input_type; }

ef_sjis_type_t ef_get_sjis_output_type(void) { return output_type; }
