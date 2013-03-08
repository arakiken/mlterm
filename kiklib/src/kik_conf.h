/*
 *	$Id$
 */

#ifndef  __KIK_CONF_H__
#define  __KIK_CONF_H__


#include  "kik_def.h"		/* REMOVE_FUNCS_MLTERM_UNUSE */
#include  "kik_types.h"
#include  "kik_map.h"


/*
 * all members should be allocated on the caller side.
 */
typedef struct  kik_arg_opt
{
	char  opt ;
	char *  long_opt ;
	int  is_boolean ;
	char *  key ;
	char *  help ;
	
} kik_arg_opt_t ;

/*
 * all members are allocated internally.
 */
typedef struct  kik_conf_entry
{
	char *  value ;
#ifndef  REMOVE_FUNCS_MLTERM_UNUSE
	char *  default_value ;
#endif

} kik_conf_entry_t ;

KIK_MAP_TYPEDEF( kik_conf_entry , char * , kik_conf_entry_t *) ;

typedef struct  kik_conf
{
	kik_arg_opt_t **  arg_opts ;	/* 0x20 - 0x7f */
	int  num_of_opts ;
	char  end_opt ;
	
	KIK_MAP( kik_conf_entry)  conf_entries ;

} kik_conf_t ;


int  kik_init_prog( char *  path , char *  version) ;

char *  kik_get_prog_path(void) ;

kik_conf_t *  kik_conf_new(void) ;

int  kik_conf_delete( kik_conf_t *  conf) ;

int  kik_conf_add_opt( kik_conf_t *  conf , char  short_opt , char *  long_opt ,
	int  is_boolean , char *  key , char *  help) ;

int  kik_conf_set_end_opt( kik_conf_t *  conf , char  opt , char *  long_opt , char *  key , char *  help) ;

int  kik_conf_parse_args( kik_conf_t *  conf , int *  argc , char ***  argv) ;

int  kik_conf_write( kik_conf_t *  conf , char *  filename) ;

int  kik_conf_read( kik_conf_t *  conf , char *  filename) ;

char *  kik_conf_get_value( kik_conf_t *  conf , char *  key) ;

#ifndef  REMOVE_FUNCS_MLTERM_UNUSE
int  kik_conf_set_default_value( kik_conf_t *  conf , char *  key , char *  default_value) ;
#endif

char *  kik_conf_get_version( kik_conf_t *  conf) ;


#endif
