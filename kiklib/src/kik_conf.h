/*
 *	update: <2001/11/22(10:51:06)>
 *	$Id$
 */

#ifndef  __KIK_CONF_H__
#define  __KIK_CONF_H__


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
	char *  key ;
	char *  value ;
	char *  default_value ;

} kik_conf_entry_t ;

KIK_MAP_TYPEDEF( kik_conf_entry , char * , kik_conf_entry_t *) ;

typedef struct  kik_conf
{
	char *  prog_name ;
	
	int  major_version ;
	int  minor_version ;
	int  revision ;
	int  patch_level ;

	kik_arg_opt_t *  arg_opts[0x5f] ;	/* 0x20 - 0x7f */
	char  end_opt ;
	
	KIK_MAP( kik_conf_entry)  conf_entries ;

} kik_conf_t ;


kik_conf_t *  kik_conf_new( char *  prog_name , int  major_version ,
	int  minor_version , int  revision , int  patch_level) ;

int  kik_conf_delete( kik_conf_t *  conf) ;

int  kik_conf_add_opt( kik_conf_t *  conf , char  opt , char *  long_opt ,
	int  is_boolean , char *  key , char *  help) ;

int  kik_conf_set_end_opt( kik_conf_t *  conf , char   opt , char *  long_opt , char *  key , char *  help) ;

int  kik_conf_parse_args( kik_conf_t *  conf , int *  argc , char ***  argv) ;

int  kik_conf_read( kik_conf_t *  conf , char *  filename) ;

char *  kik_conf_get_value( kik_conf_t *  conf , char *  key) ;

int  kik_conf_set_default_value( kik_conf_t *  conf , char *  key , char *  default_value) ;


#endif
