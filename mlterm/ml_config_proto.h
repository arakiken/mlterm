/*
 *	$Id$
 */

#ifndef  __ML_CONFIG_PROTO_H__
#define  __ML_CONFIG_PROTO_H__


/*
 * These functions are exported from ml_term.h.
 */
 
int  ml_config_proto_init(void) ;

int  ml_config_proto_final(void) ;

int  ml_gen_proto_challenge(void) ;

int  ml_parse_proto( char **  dev , char **  key , char **  val , char **  str , int  do_challenge) ;

int  ml_parse_proto2( char **  file , char **  key , char **  val , char **  str) ;


#endif
