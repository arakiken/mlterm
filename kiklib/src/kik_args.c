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
 *  --x(=xxx)
 *  -xxx(=xxx)
 *  --xxx(=xxx)
 *
 *  "--" cancels parsing options.
 *
 * !! NOTICE !!
 * after kik_parse_options() , argv points to an argument next to a successfully parsed one.
 */

int
kik_parse_options(
	char **  opt ,
	char **  opt_val ,
	int *  argc ,
	char ***  argv
	)
{
	char *  arg_p ;

	if( *argc == 0 || ( arg_p = (*argv)[0]) == NULL)
	{
		/* end of argv */
		
		return  0 ;
	}

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

	(*argv) ++ ;
	(*argc) -- ;

	return  1 ;
}
