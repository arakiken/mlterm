/*
 *	$Id$
 */

#include  "kik_conf.h"

#include  <stdio.h>
#include  <string.h>		/* memset */

#include  "kik_str.h"		/* kik_str_sep/strdup */
#include  "kik_mem.h"		/* malloc/alloca */
#include  "kik_file.h"
#include  "kik_conf_io.h"
#include  "kik_args.h"


#define  CH2IDX(ch)  ((ch) - 0x20)


/* --- static functions --- */

static void
__exit(
	kik_conf_t *  conf ,
	int  status
	)
{
#ifdef  DEBUG
	kik_conf_delete( conf) ;
	kik_mem_free_all() ;
#endif

	exit(status) ;
}

static void
version(
	kik_conf_t *  conf
	)
{
	if( conf->patch_level > 0)
	{
		printf( "%s version %d.%d.%d patch level %d\n" ,
			conf->prog_name , conf->major_version , conf->minor_version ,
				conf->revision , conf->patch_level) ;
	}
	else
	{
		printf( "%s version %d.%d.%d\n" ,
			conf->prog_name , conf->major_version , conf->minor_version ,
				conf->revision) ;
	}
}

static void
usage(
	kik_conf_t *  conf
	)
{
	int  counter ;
	kik_arg_opt_t *  end_opt ;

	printf( "usage: %s" , conf->prog_name) ;
	
	for( counter = 0 ; counter < 0x5f ; counter ++)
	{
		if( conf->arg_opts[counter] != NULL && conf->arg_opts[counter]->opt != conf->end_opt)
		{
			printf( " [options]") ;
			
			break ;
		}
	}

	if( conf->end_opt > 0)
	{
		printf( " -%c ..." , conf->end_opt) ;
	}

	printf( "\n\noptions:\n") ;

	end_opt = NULL ;
	for( counter = 0 ; counter < 0x5f ; counter ++)
	{
		if( conf->arg_opts[counter] != NULL)
		{
			if( conf->arg_opts[counter]->opt == conf->end_opt)
			{
				end_opt = conf->arg_opts[counter] ; 
			}
			else
			{
				char *  str ;
				size_t  len ;

				len = 3 + 8 + 1 ;
				if( conf->arg_opts[counter]->long_opt)
				{
					len += (3 + strlen( conf->arg_opts[counter]->long_opt) + 1) ;
				}

				if( ( str = alloca( len)) == NULL)
				{
				#ifdef  DEBUG
					kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
				#endif

					return  ;
				}

				/* 3 bytes */
				sprintf( str , " -%c" , conf->arg_opts[counter]->opt) ;

				if( conf->arg_opts[counter]->long_opt)
				{
					/* 3 bytes */
					strcat( str , "/--") ;
					
					strcat( str , conf->arg_opts[counter]->long_opt) ;
				}

				if( conf->arg_opts[counter]->is_boolean)
				{
					/* 8 bytes or ... */
					strcat( str , "(=bool) ") ;
				}
				else
				{
					/* 7 bytes */
					strcat( str , "=value ") ;
				}

				printf( "%-20s: %s\n" , str , conf->arg_opts[counter]->help) ;
			}
		}
	}

	if( end_opt)
	{
		printf( "\nend option:\n -%c" , end_opt->opt) ;

		if( end_opt->long_opt)
		{
			printf( " --%s" , end_opt->long_opt) ;
		}

		printf( " ... : %s\n" , end_opt->help) ;
	}

	printf( "\nnotice:\n") ;
	printf( "(=bool) is \"=true\" or \"=false\".\n") ;
}

static kik_conf_entry_t *
create_new_conf_entry(
	kik_conf_t *  conf ,
	char *  key
	)
{
	kik_conf_entry_t *  entry ;
	int  result ;
	
	if( ( entry = malloc( sizeof( kik_conf_entry_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  NULL ;
	}
	memset( entry , 0 , sizeof( kik_conf_entry_t)) ;

	kik_map_set( result , conf->conf_entries , key , entry) ;
	if( ! result)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " kik_map_set() failed.\n") ;
	#endif

		free( entry) ;
	
		return  NULL ;
	}

	return  entry ;
}


/* --- global functions --- */

kik_conf_t *
kik_conf_new(
	char *  prog_name ,
	int  major_version ,
	int  minor_version ,
	int  revision ,
	int  patch_level
	)
{
	kik_conf_t *  conf ;

	if( ( conf = malloc( sizeof( kik_conf_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  NULL ;
	}
	
	memset( conf , 0 , sizeof( kik_conf_t)) ;
	
	conf->prog_name = prog_name ;

	conf->major_version = major_version ;
	conf->minor_version = minor_version ;
	conf->revision = revision ;
	conf->patch_level = patch_level ;
	
	kik_map_new( char * , kik_conf_entry_t * , conf->conf_entries ,
		kik_map_hash_str , kik_map_compare_str) ;

	/* default options */
	kik_conf_add_opt( conf , 'h' , "help" , 1 , "help" , "help message") ;
	kik_conf_add_opt( conf , 'v' , "version" , 1 , "version" , "version message") ;
	
	return  conf ;
}

int
kik_conf_delete(
	kik_conf_t *  conf
	)
{
	int  counter ;
	KIK_PAIR( kik_conf_entry) *  pairs ;
	u_int  size ;

	for( counter = 0 ; counter < 0x5f ; counter ++)
	{
		if( conf->arg_opts[counter])
		{
			free( conf->arg_opts[counter]) ;
		}
	}

	kik_map_get_pairs_array( conf->conf_entries , pairs , size) ;
	
	for( counter = 0 ; counter < size ; counter ++)
	{
		free( pairs[counter]->value->key) ;
		free( pairs[counter]->value->value) ;
		free( pairs[counter]->value->default_value) ;
		free( pairs[counter]->value) ;
	}
	
	kik_map_delete( conf->conf_entries) ;

	free( conf) ;

	return  1 ;
}

int
kik_conf_add_opt(
	kik_conf_t *  conf ,
	char   opt ,
	char *  long_opt ,	/* optional(NULL is accepted) */
	int  is_boolean ,
	char *  key ,
	char *  help
	)
{
	if( conf->arg_opts[CH2IDX(opt)] == NULL)
	{
		if( ( conf->arg_opts[CH2IDX(opt)] = malloc( sizeof( kik_arg_opt_t))) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
		#endif

			return  0 ;
		}
	}

	conf->arg_opts[CH2IDX(opt)]->opt = opt ;
	conf->arg_opts[CH2IDX(opt)]->long_opt = long_opt ;
	conf->arg_opts[CH2IDX(opt)]->key = key ;
	conf->arg_opts[CH2IDX(opt)]->is_boolean = is_boolean ;
	conf->arg_opts[CH2IDX(opt)]->help = help ;

	return  1 ;
}

int
kik_conf_set_end_opt(
	kik_conf_t *  conf ,
	char   opt ,
	char *  long_opt ,
	char *  key ,
	char *  help
	)
{
	conf->end_opt = opt ;

	/* is_boolean is always true */
	return  kik_conf_add_opt( conf , opt , long_opt , 1 , key , help) ;
}

int
kik_conf_parse_args(
	kik_conf_t *  conf ,
	int *  argc ,
	char ***  argv
	)
{
	char *  opt ;
	char *  opt_val ;
	KIK_PAIR( kik_conf_entry)  pair ;
	kik_conf_entry_t *  entry ;
	int  ret ;

	while( kik_parse_options( &opt , &opt_val , argv))
	{
		char  short_opt ;

		if( strlen( opt) == 1)
		{
			short_opt = *opt ;
		}
		else if( strlen( opt) > 1)
		{
			/* long opt -> short opt */
			
			int  counter ;
			
			for( counter = 0 ; counter < 0x5f ; counter ++)
			{
				if( conf->arg_opts[counter] && conf->arg_opts[counter]->long_opt &&
					strcmp( opt , conf->arg_opts[counter]->long_opt) == 0)
				{
					short_opt = conf->arg_opts[counter]->opt ;
					
					break ;
				}
			}

			if( counter == 0x5f)
			{
				kik_msg_printf( "%s is unknown option.\n\n" , opt) ;

				goto error ;
			}
		}
		else
		{
			kik_msg_printf( "%s is unknown option.\n\n" , opt) ;
			
			goto error ;
		}

		if( conf->arg_opts[CH2IDX(short_opt)] == NULL)
		{
			kik_msg_printf( "%s is unknown option.\n\n" , opt) ;
			
			goto error ;
		}
	
		kik_map_get( ret , conf->conf_entries , conf->arg_opts[CH2IDX(short_opt)]->key , pair) ;
		if( ! ret)
		{
			if( ( entry = create_new_conf_entry( conf , conf->arg_opts[CH2IDX(short_opt)]->key))
				== NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " create_new_conf_entry() failed.\n") ;
			#endif
			
				return  0 ;
			}
		}
		else
		{
			entry = pair->value ;
			
			if( entry->value)
			{
				free( entry->value) ;
			}
		}

		if( short_opt == 'h')
		{
			usage( conf) ;

			__exit( conf , 1) ;
		}
		else if( short_opt == 'v')
		{
			version( conf) ;

			__exit( conf , 1) ;
		}

		if( conf->arg_opts[CH2IDX(short_opt)]->is_boolean)
		{
			if( opt_val)
			{
				/* "-[opt]=true" format */
				
				entry->value = strdup( opt_val) ;
			}
			else if((*argv)[1] != NULL &&
				(strcmp((*argv)[1] , "true") == 0 || strcmp((*argv)[1] , "false") == 0))
			{
				/* "-[opt] true" format */

				entry->value = strdup( *(++ (*argv))) ;
			}
			else
			{
				/* "-[opt]" format */

				entry->value = strdup( "true") ;
			}
		}
		else
		{
			if( opt_val == NULL)
			{
				/* "-[opt] [opt_val]" format */
				
				if( *(++ (*argv)) == NULL)
				{
					kik_msg_printf( "%s option requires value.\n\n" , opt) ;
					
					goto  error ;
				}

				entry->value = strdup( (*argv)[0]) ;
			}
			else
			{
				/* "-[opt]=[opt_val]" format */
				
				entry->value = strdup( opt_val) ;
			}
		}
		
		if( short_opt == conf->end_opt)
		{
			/* the value of conf->end_opt should be "true" */
			
			break ;
		}
	}

	(*argv) ++ ;

	return  1 ;
	
error:
	usage( conf) ;

	__exit( conf , 1) ;

	return  0 ;
}

int
kik_conf_read(
	kik_conf_t *  conf ,
	char *  filename
	)
{
	kik_file_t *  from ;
	char *  key ;
	char *  value ;
	kik_conf_entry_t *  entry ;
	KIK_PAIR( kik_conf_entry)  pair ;
	int  ret ;

	if( ! ( from = kik_file_open( filename , "r")))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s couldn't be opened.\n" , filename) ;
	#endif
		
		return  0 ;
	}

	while( kik_conf_io_read( from , &key , &value))
	{
		value = strdup( value) ;

		kik_map_get( ret , conf->conf_entries , key , pair) ;
		if( ! ret)
		{
			key = strdup( key) ;

			if( ( entry = create_new_conf_entry( conf , key)) == NULL)
			{
				return  0 ;
			}

			entry->key = key ;
		}
		else
		{
			entry = pair->value ;
			
			if( entry->value)
			{
				free( entry->value) ;
			}
		}
		
		entry->value = value ;
	}

	kik_file_delete( from) ;

	return  1 ;
}

char *
kik_conf_get_value(
	kik_conf_t *  conf ,
	char *  key
	)
{
	KIK_PAIR( kik_conf_entry)  pair ;
	int  ret ;

	kik_map_get( ret , conf->conf_entries , key , pair) ;

	if( ! ret)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " no such key[%s] in conf map.\n" , key) ;
	#endif
	
		return  NULL ;
	}
	else
	{
		return  pair->value->value ? pair->value->value : pair->value->default_value ;
	}
}

int
kik_conf_set_default_value(
	kik_conf_t *  conf ,
	char *  key ,
	char *  default_value
	)
{
	kik_conf_entry_t *  entry ;
	KIK_PAIR( kik_conf_entry)  pair ;
	int  ret ;

	key = strdup( key) ;

	kik_map_get( ret , conf->conf_entries , key , pair) ;
	if( ! ret)
	{
		if( ( entry = create_new_conf_entry( conf , key)) == NULL)
		{
			return  0 ;
		}
	}
	else
	{
		entry = pair->value ;

		free( entry->default_value) ;
	}

	entry->default_value = default_value ;

	return  1 ;
}
