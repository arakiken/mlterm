/*
 *	$Id$
 */

#ifndef  __ML_TERMCAP_H__
#define  __ML_TERMCAP_H__


#include  <kiklib/kik_types.h>


typedef enum  ml_termcap_str_field
{
	MLT_DELETE ,
	MLT_BACKSPACE ,
	
	MAX_TERMCAP_STR_FIELDS ,

} ml_termcap_str_field_t ;

typedef enum  ml_termcap_bool_field
{
	MLT_BCE ,

	MAX_TERMCAP_BOOL_FIELDS ,

} ml_termcap_bool_field_t ;

typedef struct  ml_termcap_entry
{
	char *  name ;
	
	char *  str_fields[MAX_TERMCAP_STR_FIELDS] ;
	int8_t  bool_fields[MAX_TERMCAP_BOOL_FIELDS] ;

} ml_termcap_entry_t ;

typedef struct  ml_termcap
{
	ml_termcap_entry_t *  entries ;
	u_int  num_of_entries ;

} ml_termcap_t ;


int  ml_termcap_init( ml_termcap_t *  termcap) ;

int  ml_termcap_final( ml_termcap_t *  termcap) ;

int  ml_termcap_read_conf( ml_termcap_t *  termcap , char *  filename) ;

ml_termcap_entry_t *  ml_termcap_get_entry( ml_termcap_t *  termcap , char *  name) ;

char *   ml_termcap_get_str_field( ml_termcap_entry_t *  entry , ml_termcap_str_field_t  field) ;

int   ml_termcap_get_bool_field( ml_termcap_entry_t *  entry , ml_termcap_bool_field_t  field) ;


#endif
