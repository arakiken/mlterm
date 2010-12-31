/*
 *	$Id$
 */

#ifndef  __KIK_ARGS_H__
#define  __KIK_ARGS_H__


#include  "kik_mem.h"	/* alloca */
#include  "kik_str.h"	/* kik_count_char_in_str */


#define  kik_arg_str_to_array( argc , args)  \
	_kik_arg_str_to_array( \
		/* \
		 * '\t' is not recognized as separator. \
		 * If you try to recognize '\t', don't forget to add checking "-e\t" in \
		 * set_config("mlclient") in x_screen.c. \
		 */ \
		alloca( sizeof( char*) * ( kik_count_char_in_str( args , ' ') + 2)) ,\
		argc , args)


int  kik_parse_options( char **  opt , char **  opt_val , int *  argc , char ***  argv) ;

char **  _kik_arg_str_to_array( char **  argv , int *  argc , char *  args) ;


#endif
