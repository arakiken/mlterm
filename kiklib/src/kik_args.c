/*
 *	$Id$
 */

#include  "kik_args.h"

#include  <string.h>	/* strchr */

#include  "kik_debug.h"


/* --- global functions --- */

/*
 * supported option syntax.
 *
 *  -x(=xxx)
 *  -xxx(=xxx)
 *  --xxx(=xxx)
 *
 *  "--" cancels parsing options.
 *
 * !! NOTICE !!
 * after kik_parse_options() , argv points to the option argument that was parsed that time.
 */

int
kik_parse_options(
	char **  opt ,
	char **  opt_val ,
	char ***  argv
	)
{
	char *  arg_p ;

	/* passing argv[0] 'cause it may be the program name. */
	arg_p = (*argv)[1] ;

	if( arg_p == NULL)
	{
		/* end of argv */
		
		return  0 ;
	}

	(*argv) ++ ;

	if( *arg_p != '-')
	{
		/* not option */

		return  0 ;
	}
	arg_p ++ ;

	if( *arg_p == '-')
	{
		arg_p ++ ;

		if( *arg_p == '\0')
		{
			/* "--" */

			return  0 ;
		}
	}

	*opt = arg_p ;

	if( ( arg_p = strchr( arg_p , '=')) == NULL)
	{
		*opt_val = NULL ;
	}
	else
	{
		*arg_p = '\0' ;
		*opt_val = arg_p + 1 ;
	}

	return  1 ;
}
