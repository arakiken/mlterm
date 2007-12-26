/*
 *	$Id$
 */

#include  "mkf_sjis_env.h"


/* --- static variables --- */

static mkf_sjis_type_t  input_type = MICROSOFT_CS ;

static mkf_sjis_type_t  output_type = MICROSOFT_CS ;


/* --- global functions --- */

void
mkf_set_sjis_input_type(
	mkf_sjis_type_t  type
	)
{
	input_type = type ;
}

void
mkf_set_sjis_output_type(
	mkf_sjis_type_t  type
	)
{
	output_type = type ;
}

mkf_sjis_type_t
mkf_get_sjis_input_type(void)
{
	return  input_type ;
}

mkf_sjis_type_t
mkf_get_sjis_output_type(void)
{
	return  output_type ;
}
