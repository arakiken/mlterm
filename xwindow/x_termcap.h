/*
 *	$Id$
 */

#ifndef  __X_TERMCAP_H__
#define  __X_TERMCAP_H__


#include  <kiklib/kik_types.h>


typedef enum  x_termcap_str_field
{
	ML_DELETE ,
	ML_BACKSPACE ,
	ML_HOME ,
	ML_END ,
	
	MAX_TERMCAP_STR_FIELDS

} x_termcap_str_field_t ;

typedef enum  x_termcap_bool_field
{
	ML_BCE ,

	MAX_TERMCAP_BOOL_FIELDS

} x_termcap_bool_field_t ;

typedef struct  x_termcap_entry
{
	char *  name ;
	
	char *  str_fields[MAX_TERMCAP_STR_FIELDS] ;
	int8_t  bool_fields[MAX_TERMCAP_BOOL_FIELDS] ;

} x_termcap_entry_t ;

typedef struct  x_termcap
{
	x_termcap_entry_t *  entries ;
	u_int  num_of_entries ;

} x_termcap_t ;


int  x_termcap_init( x_termcap_t *  termcap) ;

int  x_termcap_final( x_termcap_t *  termcap) ;

int  x_read_termcap_config( x_termcap_t *  termcap , char *  filename) ;

x_termcap_entry_t *  x_termcap_get_entry( x_termcap_t *  termcap , char *  name) ;

char *   x_termcap_get_str_field( x_termcap_entry_t *  entry , x_termcap_str_field_t  field) ;

int   x_termcap_get_bool_field( x_termcap_entry_t *  entry , x_termcap_bool_field_t  field) ;


#endif
