/*
 *	$Id$
 */

#include  "ml_termcap.h"

#include  <stdio.h>		/* NULL */


typedef struct  str_field_table
{
	char *  name ;
	ml_termcap_str_field_t  field ;
	
} str_field_table_t ;

typedef struct  bool_field_table
{
	char *  name ;
	ml_termcap_bool_field_t  field ;
	
} bool_field_table_t ;


/* --- global functions --- */

char *
ml_termcap_get_str_field(
	ml_termcap_t *  termcap ,
	ml_termcap_str_field_t  field
	)
{
	if( 0 <= field && field < MAX_TERMCAP_STR_FIELDS)
	{
		return  termcap->str_fields[field] ;
	}
	else
	{
		return  NULL ;
	}
}

int
ml_termcap_get_bool_field(
	ml_termcap_t *  termcap ,
	ml_termcap_bool_field_t  field
	)
{
	if( 0 <= field && field < MAX_TERMCAP_BOOL_FIELDS)
	{
		return  termcap->bool_fields[field] ;
	}
	else
	{
		return  0 ;
	}
}
