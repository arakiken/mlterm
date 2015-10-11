/*
 *	$Id$
 */

#include  "ml_termcap.h"

#include  <string.h>		/* strchr */
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_mem.h>	/* free */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_conf_io.h>


typedef struct  str_field_table
{
	char *  name ;
	ml_termcap_str_field_t  field ;

} str_field_table_t ;

typedef struct  bool_field_table
{
	char *  name ;
	ml_termcap_bool_field_t  field ;

} bool_field_table_t ;


/* --- static variables --- */

static ml_termcap_entry_t *  entries ;
static u_int  num_of_entries ;

static str_field_table_t  str_field_table[] =
{
	{ "kD" , TC_DELETE , } ,
	{ "kb" , TC_BACKSPACE , } ,
	{ "kh" , TC_HOME , } ,
	{ "@7" , TC_END , } ,
	/* "\x1bOP" in xterm(279), but doc/term/mlterm.ti defined "\x1b[11~" from before. */
	{ "k1" , TC_F1 , } ,
	/* "\x1bOQ" in xterm(279), but doc/term/mlterm.ti defined "\x1b[12~" from before. */
	{ "k2" , TC_F2 , } ,
	/* "\x1bOR" in xterm(279), but doc/term/mlterm.ti defined "\x1b[13~" from before. */
	{ "k3" , TC_F3 , } ,
	/* "\x1bOS" in xterm(279), but doc/term/mlterm.ti defined "\x1b[14~" from before. */
	{ "k4" , TC_F4 , } ,
	/* Requested by Andi Cristian Serbanescu (1 Nov 2012) */
	{ "k5" , TC_F5 , } ,
} ;

static bool_field_table_t  bool_field_table[] =
{
	{ "ut" , TC_BCE , } ,
} ;

static char *  tc_file = "mlterm/termcap" ;


/* --- static functions --- */

static int
entry_init(
	ml_termcap_entry_t *  entry ,
	const char *  name
	)
{
	memset( entry , 0 , sizeof(ml_termcap_entry_t)) ;
	entry->name = strdup( name) ;

	return  1 ;
}

static int
entry_final(
	ml_termcap_entry_t *  entry
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
	ml_termcap_entry_t *  entry ,
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
					if( ( value = kik_str_unescape( value)))
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

static ml_termcap_entry_t *
search_entry(
	const char *  name
	)
{
	int  count ;

	for( count = 0 ; count < num_of_entries ; count ++)
	{
		const char *  p1 ;
		const char *  p2 ;

		p1 = entries[count].name ;

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
				return  &entries[count] ;
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
			ml_termcap_entry_t *  entry ;
			char *  field ;
			char *  db_p ;

			entry_db[db_len] = '\0' ;
			db_p = entry_db ;

			if( ( field = kik_str_sep( &db_p , ":")))
			{
				if( ( entry = search_entry( field)))
				{
				#if  0
					entry_final( entry) ;
					entry_init( entry , field) ;
				#endif
					parse_entry_db( entry , db_p) ;
				}
				else if( ( p = realloc( entries ,
					sizeof( ml_termcap_entry_t) * (num_of_entries + 1))))
				{
					entries = p ;
					entry = &entries[num_of_entries] ;

					if( entry_init( entry , field) &&
						parse_entry_db( entry , db_p))
					{
						num_of_entries ++ ;
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

static int
termcap_init(void)
{
	char *  rcpath ;

	if( ( entries = malloc( sizeof( ml_termcap_entry_t))) == NULL)
	{
		return  0 ;
	}

	if( ! entry_init( entries , "*"))
	{
		return  0 ;
	}

	entries[0].bool_fields[TC_BCE] = 1 ;
	num_of_entries = 1 ;

	if( ( rcpath = kik_get_sys_rc_path( tc_file)))
	{
		if( ! read_conf( rcpath))
		{
		#if  defined(__APPLE__) || defined(__ANDROID__)
		#define  MAX_DB_LEN_IDX  2
			const char *  db[] =
			{
				"k1=\E[11~:k2=\E[12~:k3=\E[13~:k4=\E[14~" ,
				"ut" ,
				"kh=\E[7~:@7=\E[8~:k1=\E[11~:k2=\E[12~:k3=\E[13~:k4=\E[14~:ut" ,
				"kb=^H:kD=^?:k1=\E[11~:k2=\E[12~:k3=\E[13~:k4=\E[14~" ,
			} ;

			const char *  names[] =
			{
				"mlterm" ,
				"xterm" ,
				"rxvt" ,
				"kterm" ,
			} ;

			void *  p ;
			char *  buf ;

			if( ( p = realloc( entries ,
				sizeof(ml_termcap_entry_t) * sizeof(db) / sizeof(db[0]))) &&
			    ( buf = alloca( strlen( db[MAX_DB_LEN_IDX]) + 1)))
			{
				size_t  count ;

				entries = p ;
				num_of_entries = sizeof(db) / sizeof(db[0]) + 1 ;

				for( count = 0 ; count < sizeof(db) / sizeof(db[0]) ; count++)
				{
					entry_init( entries + count + 1 , names[count]) ;
					strcpy( buf , db[count]) ;
					parse_entry_db( entries + count + 1 , buf) ;
				}
			}
		#endif
		}

		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( tc_file)))
	{
		read_conf( rcpath) ;
		free( rcpath) ;
	}

	return  1 ;
}


/* --- global functions --- */

ml_termcap_entry_t *
ml_termcap_get(
	const char *  name
	)
{
	ml_termcap_entry_t *  entry ;

	if( entries == NULL)
	{
		if( ! termcap_init())
		{
			return  NULL ;
		}
	}

	if( ( entry = search_entry( name)))
	{
		return  entry ;
	}

	/* '*' */
	return  entries ;
}

void
ml_termcap_final(void)
{
	int  count ;

	for( count = 0 ; count < num_of_entries ; count ++)
	{
		entry_final( &entries[count]) ;
	}

	free( entries) ;
}

char *
ml_termcap_get_str_field(
	ml_termcap_entry_t *  entry ,
	ml_termcap_str_field_t  field
	)
{
	if( (u_int)field < MAX_TERMCAP_STR_FIELDS)
	{
		return  entry->str_fields[field] ;
	}
	else
	{
		return  NULL ;
	}
}

int
ml_termcap_get_bool_field(
	ml_termcap_entry_t *  entry ,
	ml_termcap_bool_field_t  field
	)
{
	if( (u_int)field < MAX_TERMCAP_BOOL_FIELDS)
	{
		return  entry->bool_fields[field] ;
	}
	else
	{
		return  0 ;
	}
}

char *
ml_termcap_special_key_to_seq(
	ml_termcap_entry_t *  entry ,
	ml_special_key_t  key ,
	int  modcode ,
	int  is_app_keypad ,
	int  is_app_cursor_keys ,
	int  is_app_escape
	)
{
	static char  escseq[10] ;
	char *  seq ;
	char  intermed_ch ;
	char  final_ch ;
	int  param ;

	switch( key)
	{
	case  SPKEY_DELETE:
		if( modcode ||
		    ! (seq = ml_termcap_get_str_field( entry , TC_DELETE)))
		{
			intermed_ch = '[' ;
			param = 3 ;
			final_ch = '~' ;
			break ;
		}
		else
		{
			return  seq ;
		}

	case  SPKEY_BACKSPACE:
		if( ! (seq = ml_termcap_get_str_field( entry , TC_BACKSPACE)))
		{
			seq = "\x7f" ;
		}

		return  seq ;

	case  SPKEY_ESCAPE:
		if( is_app_escape)
		{
			return  "\x1bO[" ;
		}
		else
		{
			return  NULL ;
		}

	case  SPKEY_END:
		if( modcode ||
		    ! is_app_cursor_keys ||
		    ! (seq = ml_termcap_get_str_field( entry , TC_END)))
		{
			intermed_ch = (is_app_cursor_keys && ! modcode) ? 'O' : '[' ;
			param = modcode ? 1 : 0 ;
			final_ch = 'F' ;
			break ;
		}
		else
		{
			return  seq ;
		}

	case  SPKEY_HOME:
		if( modcode ||
		    ! is_app_cursor_keys ||
		    ! (seq = ml_termcap_get_str_field( entry , TC_HOME)))
		{
			intermed_ch = (is_app_cursor_keys && ! modcode) ? 'O' : '[' ;
			param = modcode ? 1 : 0 ;
			final_ch = 'H' ;
			break ;
		}
		else
		{
			return  seq ;
		}

	case  SPKEY_BEGIN:
		intermed_ch = '[' ;
		param = modcode ? 1 : 0 ;
		final_ch = 'E' ;
		break ;

	case  SPKEY_ISO_LEFT_TAB:
		intermed_ch = '[' ;
		param = 0 ;
		final_ch = 'Z' ;
		modcode = 0 ;
		break ;

	default:
		if( key <= SPKEY_KP_F4)
		{
			if( is_app_keypad)
			{
				char  final_chs[] =
				{
					'j' ,	/* MULTIPLY */
					'k' ,	/* ADD */
					'l' ,	/* SEPARATOR */
					'm' ,	/* SUBTRACT */
					'n' ,	/* DELETE */
					'o' ,	/* DIVIDE */
					'q' ,	/* END */
					'w' ,	/* HOME */
					'u' ,	/* BEGIN */
					'x' ,	/* UP */
					'r' ,	/* DOWN */
					'v' ,	/* RIGHT */
					't' ,	/* LEFT */
					'p' ,	/* INSERT */
					'y' ,	/* PRIOR */
					's' ,	/* NEXT */
					'P' ,	/* F1 */
					'Q' ,	/* F2 */
					'R' ,	/* F3 */
					'S' ,	/* F4 */
				} ;

				intermed_ch = 'O' ;
				param = 0 ;
				final_ch = final_chs[key - SPKEY_KP_MULTIPLY] ;
			}
			else
			{
				if( key <= SPKEY_KP_DIVIDE)
				{
					return  NULL ;
				}
				else if( key <= SPKEY_KP_BEGIN)
				{
					key += (SPKEY_END - SPKEY_KP_END) ;
				}
				else if( key <= SPKEY_KP_LEFT)
				{
					key += (SPKEY_UP - SPKEY_KP_UP) ;
				}
				else if( key == SPKEY_KP_INSERT)
				{
					key = SPKEY_INSERT ;
				}
				else if( key <= SPKEY_KP_F4)
				{
					key += (SPKEY_PRIOR - SPKEY_KP_PRIOR) ;
				}
				else
				{
					return  NULL ;
				}

				return  ml_termcap_special_key_to_seq( entry ,
						key , modcode , is_app_keypad ,
						is_app_cursor_keys , is_app_escape) ;
			}
		}
		else if( key <= SPKEY_LEFT)
		{
			intermed_ch = (is_app_cursor_keys && ! modcode) ? 'O' : '[' ;
			param = modcode ? 1 : 0 ;
			final_ch = (key - SPKEY_UP) + 'A' ;
		}
		else if( key <= SPKEY_NEXT)
		{
			intermed_ch = '[' ;
			param = (key - SPKEY_FIND) + 1 ;
			final_ch = '~' ;
		}
		else if( key <= SPKEY_F5)
		{
			if( modcode ||
			    ! (seq = ml_termcap_get_str_field( entry , TC_F1 + key - SPKEY_F1)))
			{
				if( key == SPKEY_F5)
				{
					intermed_ch = '[' ;
					param = 15 ;
					final_ch = '~' ;
				}
				else
				{
					intermed_ch = 'O' ;
					param = modcode ;
					/* PQRS */
					final_ch = (key - SPKEY_F1) + 'P' ;

					/*
					 * Shift+F1 is not ^[O1;2P but ^[O2P.
					 * So 'modcode' is copied to 'param' varaiable
					 * above and then cleared to 0 here.
					 */
					modcode = 0 ;
				}
			}
			else
			{
				return  seq ;
			}
		}
		else /* if( key <= SPKEY_F37) */
		{
			char  params[] =
			{
				/* F6 - F15 */
				17 , 18 , 19 , 20 , 21 , 23 , 24 , 25 , 26 , 28 ,
				/* F16 - F25 */
				29 , 31 , 32 , 33 , 34 , 42 , 43 , 44 , 45 , 46 ,
				/* F26 - F35 */
				47 , 48 , 49 , 50 , 51 , 52 , 53 , 54 , 55 , 56 ,
				/* F36 - F37 */
				57 , 58 ,

			} ;
			intermed_ch = '[' ;
			final_ch = '~' ;

			param = params[key -  SPKEY_F6] ;
		}
	}

	if( modcode) /* ESC <intermed> Ps ; Ps <final> */
	{
		kik_snprintf( escseq , sizeof(escseq) ,
			      "\x1b%c%d;%d%c" ,
			      intermed_ch , param ,
			      modcode , final_ch) ;
	}
	else if( param) /* ESC <intermed> Ps <final> */
	{
		kik_snprintf( escseq , sizeof(escseq) ,
			      "\x1b%c%d%c" ,
			      intermed_ch , param ,
			      final_ch) ;
	}
	else /* ESC <intermed> <final> */
	{
		kik_snprintf( escseq , sizeof(escseq) ,
			      "\x1b%c%c" ,
			      intermed_ch , final_ch) ;
	}

	return  escseq ;
}
