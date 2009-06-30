/*
 *	$Id$
 */

#include  "x_termcap.h"

#include  <string.h>		/* strchr */
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_mem.h>	/* free */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_conf_io.h>


typedef struct  str_field_table
{
	char *  name ;
	x_termcap_str_field_t  field ;
	
} str_field_table_t ;

typedef struct  bool_field_table
{
	char *  name ;
	x_termcap_bool_field_t  field ;
	
} bool_field_table_t ;


/* --- static variables --- */

static str_field_table_t  str_field_table[] =
{
	{ "kD" , ML_DELETE , } ,
	{ "kb" , ML_BACKSPACE , } ,
	{ "kh" , ML_HOME , } ,
	{ "@7" , ML_END , } ,
} ;

static bool_field_table_t  bool_field_table[] =
{
	{ "ut" , ML_BCE , } ,
} ;

static char *  tc_file = "mlterm/termcap" ;


/* --- static functions --- */

static char *
parse_str_field_value(
	char *  seq
	)
{
	char *  new_seq ;
	char *  p ;

	if( ( new_seq = malloc( strlen( seq) + 1)) == NULL)
	{
		return  NULL ;
	}

	p = new_seq ;

	while( *seq)
	{
		if( *seq == '\\')
		{
			if( *(++ seq) == '\0')
			{
				break ;
			}
			
			if( *seq == 'E')
			{
				*p = '\x1b' ;
			}
			else
			{
				*p = *seq ;
			}
		}
		else if( *seq == '^')
		{
			if( *(++ seq) == '\0')
			{
				break ;
			}

			if( '@' <= *seq && *seq <= '_')
			{
				*p = *seq - 'A' + 1 ;
			}
			else if( *seq == '?')
			{
				*p = '\x7f' ;
			}
		}
		else
		{
			*p = *seq ;
		}

		p ++ ;
		seq ++ ;
	}

	*p = '\0' ;

	return  new_seq ;
}

static int
entry_init(
	x_termcap_entry_t *  entry ,
	char *  name
	)
{
	int  count ;

	entry->name = strdup( name) ;
	entry->str_fields[ML_DELETE] = strdup( "\x1b[3~") ;
	entry->str_fields[ML_BACKSPACE] = strdup( "\x7f") ;
	entry->str_fields[ML_HOME] = strdup( "\x1bOH") ;
	entry->str_fields[ML_END] = strdup( "\x1bOF") ;

	for( count = 0 ; count < MAX_TERMCAP_BOOL_FIELDS ; count ++)
	{
		entry->bool_fields[count] = 0 ;
	}

	return  1 ;
}

static int
entry_final(
	x_termcap_entry_t *  entry
	)
{
	int  count ;

	free( entry->name) ;
	
	for( count = 0 ; count < MAX_TERMCAP_STR_FIELDS ; count ++)
	{
		free( entry->str_fields[count]) ;
	}
	
	return  1 ;
}

static int
parse_entry_db(
	x_termcap_entry_t *  entry ,
	char *  entry_db
	)
{
	char *  field ;
	int  count ;
	
	while( ( field = kik_str_sep( &entry_db , ":")))
	{
		char *  key ;
		char *  value ;

		key = kik_str_sep( &field , "=") ;

		if( ( value = field) == NULL)
		{
			for( count = 0 ; count < MAX_TERMCAP_BOOL_FIELDS ; count ++)
			{
				if( strcmp( key , bool_field_table[count].name) == 0)
				{
					entry->bool_fields[ bool_field_table[count].field] = 1 ;

					break ;
				}
			}
		}
		else
		{
			for( count = 0 ; count < MAX_TERMCAP_STR_FIELDS ; count ++)
			{
				if( strcmp( key , str_field_table[count].name) == 0)
				{
					if( ( value = parse_str_field_value( value)))
					{
						free( entry->str_fields[ str_field_table[count].field]) ;
						entry->str_fields[ str_field_table[count].field] = value ;
					}

					break ;
				}
			}
		}
	}
	
	return  1 ;
}

static x_termcap_entry_t *
search_entry(
	x_termcap_t *  termcap ,
	char *  name
	)
{
	int  count ;

	for( count = 0 ; count < termcap->num_of_entries ; count ++)
	{
		char *  p1 ;
		char *  p2 ;
		
		p1 = termcap->entries[count].name ;

		while( *p1)
		{
			p2 = name ;

			while( *p1 && *p2 && *p1 != '|' && *p1 == *p2)
			{
				p1 ++ ;
				p2 ++ ;
			}

			if( *p1 == '|' || *p1 == '\0')
			{
				return  &termcap->entries[count] ;
			}
			else
			{
				if( ( p1 = strchr( p1 , '|')) == NULL)
				{
					break ;
				}

				p1 ++ ;
			}
		}
	}

	return  NULL ;
}

static int
read_conf(
	x_termcap_t *  termcap ,
	char *  filename
	)
{
	kik_file_t *  from ;
	char *  line ;
	size_t  len ;
	char *  entry_db ;
	size_t  db_len ;

	if( ! ( from = kik_file_open( filename , "r")))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s couldn't be opened.\n" , filename) ;
	#endif
	
		return  0 ;
	}

	entry_db = NULL ;
	db_len = 0 ;
	
	while( ( line = kik_file_get_line( from , &len)))
	{
		void *  p ;
		
		if( len < 2) /* skip empty(LF-only) line */
		{
			continue ;
		}
		if( *line == '#')
		{
			continue ;
		}

		line[len - 1] = '\0' ;

		while( *line == ' ' || *line == '\t')
		{
			line ++ ;
		}

		len = strlen( line) ;

		/* + 1 is for NULL terminator */
		if( ( p = realloc( entry_db , db_len + len + 1)) == NULL)
		{
			free( entry_db) ;
			kik_file_close( from) ;

			return  0 ;
		}

		entry_db = p ;

		strncpy( &entry_db[db_len] , line , len) ;
		db_len += len ;
		
		if( entry_db[db_len - 1] == '\\')
		{
			db_len -- ;
		}
		else
		{
			x_termcap_entry_t *  entry ;
			char *  field ;
			char *  db_p ;
			
			entry_db[db_len] = '\0' ;
			db_p = entry_db ;
			
			if( ( field = kik_str_sep( &db_p , ":")))
			{
				if( ( entry = search_entry( termcap , field)))
				{
					entry_final( entry) ;
					entry_init( entry , field) ;
					parse_entry_db( entry , db_p) ;
				}
				else if( ( p = realloc( termcap->entries ,
					sizeof( x_termcap_entry_t) * (termcap->num_of_entries + 1))))
				{
					termcap->entries = p ;
					entry = &termcap->entries[termcap->num_of_entries] ;

					if( entry_init( entry , field) &&
						parse_entry_db( entry , db_p))
					{
						termcap->num_of_entries ++ ;
					}
				}
			}

			db_len = 0 ;
		}
	}

	free( entry_db) ;

	kik_file_close( from) ;
	
	return  1 ;
}


/* --- global functions --- */

int
x_termcap_init(
	x_termcap_t *  termcap
	)
{
	char *  rcpath ;
	
	if( ( termcap->entries = malloc( sizeof( x_termcap_entry_t))) == NULL)
	{
		return  0 ;
	}
	
	if( ! entry_init( termcap->entries , "*"))
	{
		return  0 ;
	}
	
	termcap->num_of_entries = 1 ;

	if( ( rcpath = kik_get_sys_rc_path( tc_file)))
	{
		read_conf( termcap , rcpath) ;
		free( rcpath) ;
	}
	
	if( ( rcpath = kik_get_user_rc_path( tc_file)))
	{
		read_conf( termcap , rcpath) ;
		free( rcpath) ;
	}

	return  1 ;
}

int
x_termcap_final(
	x_termcap_t *  termcap
	)
{
	int  count ;

	for( count = 0 ; count < termcap->num_of_entries ; count ++)
	{
		entry_final( &termcap->entries[count]) ;
	}

	free( termcap->entries) ;

	return  1 ;
}

x_termcap_entry_t *
x_termcap_get_entry(
	x_termcap_t *  termcap ,
	char *  name
	)
{
	x_termcap_entry_t *  entry ;
	
	if( ( entry = search_entry( termcap , name)))
	{
		return  entry ;
	}

	/* '*' */
	return  termcap->entries ;
}

char *
x_termcap_get_str_field(
	x_termcap_entry_t *  entry ,
	x_termcap_str_field_t  field
	)
{
	if( 0 <= field && field < MAX_TERMCAP_STR_FIELDS)
	{
		return  entry->str_fields[field] ;
	}
	else
	{
		return  NULL ;
	}
}

int
x_termcap_get_bool_field(
	x_termcap_entry_t *  entry ,
	x_termcap_bool_field_t  field
	)
{
	if( 0 <= field && field < MAX_TERMCAP_BOOL_FIELDS)
	{
		return  entry->bool_fields[field] ;
	}
	else
	{
		return  0 ;
	}
}
