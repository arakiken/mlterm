/*
 *	$Id$
 */

#ifndef  __ML_TERMCAP_H__
#define  __ML_TERMCAP_H__


#include  <kiklib/kik_types.h>


typedef enum  ml_termcap_str_field
{
	ML_DELETE ,
	ML_BACKSPACE ,
	
	MAX_TERMCAP_STR_FIELDS ,

} ml_termcap_str_field_t ;

typedef enum  ml_termcap_bool_field
{
	ML_BCE ,

	MAX_TERMCAP_BOOL_FIELDS ,

} ml_termcap_bool_field_t ;

typedef struct  ml_termcap
{
	char *  name ;
	
	char *  str_fields[MAX_TERMCAP_STR_FIELDS] ;
	int8_t  bool_fields[MAX_TERMCAP_BOOL_FIELDS] ;

} ml_termcap_t ;


char *   ml_termcap_get_str_field( ml_termcap_t *  termcap , ml_termcap_str_field_t  field) ;

int   ml_termcap_get_bool_field( ml_termcap_t *  termcap , ml_termcap_bool_field_t  field) ;


#endif
