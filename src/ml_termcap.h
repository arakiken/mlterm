/*
 *	update: <2001/11/26(17:13:03)>
 *	$Id$
 */

#ifndef  __ML_TERMCAP_H__
#define  __ML_TERMCAP_H__



typedef enum  ml_termcap_field
{
	MLT_DELETE ,
	MLT_BACKSPACE ,
	
	MAX_TERMCAP_FIELDS ,

} ml_termcap_field_t ;

typedef struct  ml_termcap
{
	char *  fields[MAX_TERMCAP_FIELDS] ;

} ml_termcap_t ;


int  ml_termcap_init( ml_termcap_t *  termcap) ;

int  ml_termcap_final( ml_termcap_t *  termcap) ;

int  ml_termcap_read_conf( ml_termcap_t *  termcap , char *  filename) ;

char *   ml_termcap_get_sequence( ml_termcap_t *  termcap , ml_termcap_field_t  field) ;


#endif
