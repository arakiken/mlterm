/*
 * aux.c - iiimf plugin for mlterm (part for handling X aux object)
 *
 * Copyright (c) 2005 Seiichi SATO <ssato@sh.rim.or.jp>
 *
 * $Id$
 */

/*
 * This file is heavily based on iiimpAux.c of im-sdk-r2195.
 */

/*
 * Copyright 1990-2001 Sun Microsystems, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions: The above copyright notice and this
 * permission notice shall be included in all copies or substantial
 * portions of the Software.
 *
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE OPEN GROUP OR SUN MICROSYSTEMS, INC. BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE EVEN IF
 * ADVISED IN ADVANCE OF THE POSSIBILITY OF SUCH DAMAGES.
 *
 *
 * Except as contained in this notice, the names of The Open Group and/or
 * Sun Microsystems, Inc. shall not be used in advertising or otherwise to
 * promote the sale, use or other dealings in this Software without prior
 * written authorization from The Open Group and/or Sun Microsystems,
 * Inc., as applicable.
 *
 *
 * X Window System is a trademark of The Open Group
 *
 *
 * OSF/1, OSF/Motif and Motif are registered trademarks, and OSF, the OSF
 * logo, LBX, X Window System, and Xinerama are trademarks of the Open
 * Group. All other trademarks and registered trademarks mentioned herein
 * are the property of their respective owners. No right, title or
 * interest in or to any trademark, service mark, logo or trade name of
 * Sun Microsystems, Inc. or its licensors is granted.
 *
 */


#include  <string.h>		/* strncmp */
#include  <sys/types.h>
#include  <sys/stat.h>
#include  <limits.h>
#include  <fcntl.h>
#include  <kiklib/kik_unistd.h>
#include  <kiklib/kik_mem.h>	/* malloc/alloca/free */
#include  <kiklib/kik_str.h>	/* kik_snprintf/kik_str_alloca_dup/kik_str_sep*/
#include  <kiklib/kik_dlfcn.h>	/* kik_dl_open() */
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_list.h>
#include  <kiklib/kik_debug.h>
#include  <mkf/mkf_iso8859_conv.h>

#include  "im_iiimf.h"
#include  "../im_common.h"

#if  0
#define  AUX_DEBUG  1
#endif

#if  1
#define  ATOK12_HACK  1
#endif

#ifdef  __sparcv9
#  define  AUX_DIR_SPARCV9	"/usr/lib/im/sparcv9/" /* FIXME */
#else
#  define  AUX_BASE_DIR		"/usr/lib/im/"
#endif

#define  AUX_DIR_SYMBOL		"aux_dir"
#define  AUX_INFO_SYMBOL	"aux_info"
#define  AUX_CONF_MAGIC		"# IIIM X auxiliary"
#define  AUX_IF_VERSION_2	0x00020000

/* faked "composed" structure */
typedef struct  aux_composed
{
	int len ;
	aux_t *  aux ;
	IIIMCF_event  event ;
	aux_data_t *  data ;

} aux_composed_t ;

typedef struct aux_module
{
	char *  file_name ;

	kik_dl_handle_t  handle ;

	int  num_of_entries ;
	aux_entry_t *  entries ;

	struct aux_module *  next ;

} aux_module_t ;

typedef struct aux_module_info
{
	char *  aux_name ;
	char *  file_name ;

} aux_module_info_t ;

typedef struct  aux_id_info
{
	int  im_id ;
	int  ic_id ;
	aux_t *  aux ;

} aux_id_info_t ;

typedef struct  filter_info
{
	Display *  display ;
	Window  window ;
	Bool (* filter)(Display * , Window , XEvent * , XPointer) ;
	XPointer  client_data ;

	struct  filter_info *  next ;

} filter_info_t ;

KIK_LIST_TYPEDEF( aux_module_info_t) ;
KIK_LIST_TYPEDEF( aux_id_info_t) ;

static aux_service_t  aux_service ;

/* --- static variables --- */

static int  initialized = 0 ;

static IIIMCF_handle  handle = NULL ;
static x_im_export_syms_t *  syms = NULL ;

static mkf_parser_t *  parser_utf16 = NULL ;
static mkf_conv_t *  conv_iso88591 = NULL ;

#if 0
static mkf_parser_t *  parser_locale = NULL ;
static mkf_conv_t *  conv_locale = NULL ;
static mkf_conv_t *  conv_utf16 = NULL ;
#endif

static aux_module_t *  module_list = NULL ;
static KIK_LIST( aux_module_info_t)  module_info_list = NULL ;
static KIK_LIST( aux_id_info_t)  id_info_list = NULL ;
static filter_info_t *  filter_info_list = NULL ;

/* --- static functions --- */

static int
is_valid_path(
	char *  file_name
	)
{
	size_t  len ;

	/*
	 * may not start with "/"
	 * may not start with "../"
	 * may not contain "/../"
	 * may not end with "/"
	 * may not end with "/."
	 * may not end with "/.."
	 * may not be ".."
	 */

	if( file_name == NULL)
	{
		return  0 ;
	}

	if( ( len = strlen( file_name)) < 5)
	{
		return  0 ;
	}

	if( file_name[0] == '/' ||
	    strncmp( file_name , "../", 3) == 0 ||
	    strstr( file_name , "/../") ||
	    file_name[len - 1] == '/' ||
	    strcmp( &file_name[len - 2] , "/.") == 0 ||
	    strcmp( &file_name[len - 3] , "/..") == 0 ||
	    strcmp( file_name , "..") == 0)
	{
		return  0 ;
	}

	return  1 ;
}

static int
is_conf_file(
	char *  file_name
	)
{
	int  fd ;
	int  len ;
	int  magic_len ;
	char  buf[64] ;

	if( file_name == NULL)
	{
		return  0 ;
	}


	/*
	 * must have ".conf" suffix
	 */

	if( (len = strlen( file_name)) < 6)
	{
		return  0 ;
	}

	if( strcmp( &file_name[len - 5] , ".conf"))
	{
		return  0 ;
	}

	if( ( fd = open( file_name , O_RDONLY, 0)) == -1)
	{
		return  0 ;
	}

	magic_len = strlen( AUX_CONF_MAGIC) ;

	len = read( fd , buf , magic_len) ;

	close( fd) ;

	if( len == -1)
	{
		return  0 ;
	}

	if( len == magic_len && strncmp( buf , AUX_CONF_MAGIC , magic_len))
	{
		return  1 ;
	}

	return  0 ;
}


/*
 * module related functions
 */

static aux_module_t *
load_module(
	char *  file_name
	)
{
	aux_module_t *  module = NULL ;
	aux_info_t *  aux_info = NULL ;
	aux_dir_t *  aux_dir = NULL ;
	int  num_of_aux_dir ;
	unsigned int ifversion ;
	kik_dl_handle_t  dl_handle = NULL ;
	aux_dir_t *  dir ;
	aux_entry_t *  entry ;
	int  i ;

	char *  dirname = NULL ;
	char *  basename = NULL ;
	char *  dirpath ;
	size_t  len ;

	if( ! is_valid_path( file_name))
	{
		return  NULL ;
	}

	/* eliminate leading "./" */
	if( strncmp( file_name , "./" , 2) == 0)
	{
		file_name += 2 ;
	}

	/*
	 * TODO: kik_dirname()
	 */
	if( ( dirname = kik_str_alloca_dup( file_name)))
	{
		if( ( basename = strrchr( dirname , '/')))
		{
			size_t  len ;
			basename[0] = '\0' ;
			basename ++ ;
			len = strlen( basename) ;
			if( strncmp( &basename[len - 3] , ".so" , 3))
			{
				return  NULL ;
			}
			/* eliminate suffix ".so" */
			basename[strlen( basename) - 3] = '\0' ;
		}
	}

	len = strlen(AUX_BASE_DIR) + strlen( dirname) + 2 ;
	if( ! ( dirpath = alloca( len)))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG "alloca failed\n") ;
	#endif
		return  NULL ;
	}

	kik_snprintf( dirpath , len , "%s%s/" , AUX_BASE_DIR , dirname) ;

	if( ! ( dl_handle = kik_dl_open( dirpath , basename)))
	{
		return  NULL ;
	}

	aux_info = (aux_info_t*) kik_dl_func_symbol( dl_handle ,
						     AUX_INFO_SYMBOL) ;
	if( aux_info &&
	    aux_info->if_version >= AUX_IF_VERSION_2 &&
	    aux_info->register_service)
	{
		(*aux_info->register_service)( AUX_IF_VERSION_2 ,
					       &aux_service) ;
		ifversion = aux_info->if_version ;
		aux_dir = aux_info->dir ;
	}
	else
	{
		ifversion = 0 ;
		aux_dir = (aux_dir_t *) kik_dl_func_symbol( dl_handle ,
							    AUX_DIR_SYMBOL) ;
	}

	if( aux_dir == NULL)
	{
		goto  error ;
	}

	/* count available aux_dir_t in the module */
	for( num_of_aux_dir = 0 , dir = aux_dir ;
	     dir->name.len > 0 ;
	     dir ++ , num_of_aux_dir ++) ;

	if( ! ( module = malloc( sizeof( aux_module_t))))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif
		goto  error ;
	}

	module->file_name = strdup( file_name) ;
	module->handle = dl_handle ;
	module->entries = NULL ;
	module->num_of_entries = 0 ;

	module->next = NULL ;

	if( ! ( module->entries = calloc( num_of_aux_dir , sizeof( aux_entry_t))))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " calloc failed. (%d, %d)\n" ,
				  num_of_aux_dir , sizeof( aux_entry_t)) ;
	#endif
		goto  error ;
	}

	module->num_of_entries = num_of_aux_dir ;

	for( i = 0 , entry = module->entries , dir = aux_dir ;
	     i < num_of_aux_dir ;
	     i ++ , entry ++ , dir ++)
	{
		entry->created = 0 ;
		memcpy( &entry->dir , dir , sizeof( aux_dir_t));
		entry->if_version = ifversion ;
	}

	return  module ;

error:

	if( dl_handle)
	{
		kik_dl_close( dl_handle) ;
	}

	if( module)
	{
		if( module->entries)
		{
			free( module->entries) ;
		}

		free( module) ;
	}

	return  NULL ;
}

static void
unload_module(
	aux_module_t *  module
	)
{
	if( module == NULL)
	{
		return ;
	}

	if( module->handle)
	{
		kik_dl_close( module->handle) ;
	}

	if( module->num_of_entries)
	{
		free( module->entries) ;
	}

	if( module->file_name)
	{
		free( module->file_name) ;
	}

	free( module) ;
}

static void
register_module_info(
	char *  aux_name ,
	char *  file_name
	)
{
	aux_module_info_t *  info ;

#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "register_aux: %s %s\n", aux_name, file_name) ;
#endif

	if( ! ( info = malloc( sizeof( aux_module_info_t))))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG "malloc failed\n") ;
	#endif
		return ;
	}

	info->aux_name = strdup( aux_name) ;
	info->file_name = strdup( file_name) ;

	kik_list_insert_head( aux_module_info_t , module_info_list , info) ;
}

static char *
find_module_name(
	char *  aux_name
	)
{
	KIK_ITERATOR( aux_module_info_t)  iterator = NULL ;

	iterator = kik_list_first( module_info_list) ;

	while( iterator)
	{
		if( kik_iterator_indirect( iterator) == NULL)
		{
			kik_error_printf(
				"iterator found , but it has no logs."
				"don't you cross over memory boundaries anywhere?\n") ;
		}
		else if ( strcmp( kik_iterator_indirect( iterator)->aux_name , aux_name) == 0)
		{
			return  kik_iterator_indirect( iterator)->file_name ;
		}

		iterator = kik_iterator_next( iterator) ;
	}

	return  NULL ;
}

static aux_im_data_t *
create_im_data(
	IIIMCF_context  context ,
	const IIIMP_card16 *  aux_name_utf16
	)
{
	u_char *  aux_name = NULL ;
	aux_im_data_t *  im_data = NULL ;
	aux_entry_t *  entry = NULL ;
	aux_entry_t *  e ;
	aux_module_t *  module ;
	int  i ;

	for( module = module_list ; module ; module = module->next)
	{
		for( i = 0 , e = module->entries ;
		     i < module->num_of_entries ;
		     i ++ , e ++)
		{
			if( memcmp( aux_name_utf16 , e->dir.name.utf16str , e->dir.name.len) == 0)
			{
				entry = e ;
			}
		}
	}

	if( entry == NULL)
	{
		PARSER_INIT_WITH_BOM( parser_utf16) ;
		im_convert_encoding( parser_utf16 , conv_iso88591 ,
				     (u_char*)aux_name_utf16 , &aux_name,
				     strlen_utf16( aux_name_utf16) + 1) ;

		if( ! ( module = load_module( find_module_name( aux_name))))
		{
			goto  error ;
		}

		module->next = module_list ;
		module_list = module ;

		for( i = 0 , e = module->entries ;
		     i < module->num_of_entries ;
		     i ++ , e ++)
		{
			if( memcmp( aux_name_utf16 , e->dir.name.utf16str , e->dir.name.len) == 0)
			{
				entry = e ;
			}
		}

		free( aux_name) ;
		aux_name = NULL ;
	}

	if( entry == NULL)
	{
		goto  error ;
	}

	if( ! ( im_data = malloc( sizeof( aux_im_data_t))))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG "malloc failed.\n") ;
	#endif
		goto  error ;
	}

	im_data->im_id = 0 ;
	im_data->ic_id = 0 ;
	im_data->entry = entry ;
	im_data->data = NULL ;
	im_data->next = NULL ;

	if( iiimcf_get_im_id( handle , &im_data->im_id) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG "iiimcf_get_im_id failed.\n") ;
	#endif
		goto  error ;
	}
	if( iiimcf_get_ic_id( context, &im_data->ic_id) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG "iiimcf_get_ic_id failed.\n") ;
	#endif
		goto  error ;
	}

	return  im_data ;

error:
	if( aux_name)
	{
		free( aux_name) ;
	}

	if( im_data)
	{
		free( im_data) ;
	}

	return  NULL ;
}

#ifdef  ATOK12_HACK
#define  ATOK12_SO "./locale/ja/atokserver/atok12aux.so"
static void
register_atok12_module( void)
{
	aux_module_t *  module ;
	aux_entry_t *  e ;
	int  i ;

	if( ! ( module = load_module( ATOK12_SO)))
	{
		return ;
	}

	for( i = 0 , e = module->entries ; i < module->num_of_entries ;
	     i ++ , e ++)
	{
		u_char *  str = NULL;

		PARSER_INIT_WITH_BOM( parser_utf16) ;
		im_convert_encoding( parser_utf16 , conv_iso88591 ,
				     (u_char*)e->dir.name.utf16str ,
				     &str , e->dir.name.len + 1) ;
		register_module_info( str , ATOK12_SO) ;
		free( str) ;
	}

	unload_module( module) ;
}
#endif


/*
 * X aux service
 */

#define ROUNDUP( n) ( ( (n) + sizeof( int) - 1) / sizeof( int) * sizeof( int))

static aux_composed_t *
create_composed_from_event(
	aux_t *  aux ,
	IIIMCF_event  event
	)
{
	unsigned char *  p ;
	aux_composed_t *  ac ;
	const IIIMP_card16 *  aux_name ;
	IIIMP_card32  class_index ;
	const IIIMP_card32 *  int_array ;
	const IIIMP_card16 **  str_array ;
	int  num_of_int ;
	int  num_of_str ;
	aux_data_t *  data ;

	int  i ;
	int  n ;
	int  aux_data_t_n ;
	int  aux_name_len ;
	int  aux_name_n ;
	int  integer_list_n = 0 ;
	int  string_list_n = 0 ;
	int  string_n = 0 ;
	int  *pstring_len ;

	if( iiimcf_get_aux_event_value( event , &aux_name , &class_index ,
					&num_of_int , &int_array ,
					&num_of_str, &str_array) != IIIMF_STATUS_SUCCESS)
	{
		return  NULL ;
	}

	/* first of all, caliculate size. */
	n = ROUNDUP( sizeof( aux_composed_t)) ;
	aux_data_t_n = n ;
	n += sizeof( aux_data_t) ;
	aux_name_n = n = ROUNDUP( n) ;
	aux_name_len = strlen_utf16( aux_name) / 2  ;
	n += ( aux_name_len + 1) * sizeof( IIIMP_card16) ;
	if(num_of_int > 0)
	{
		integer_list_n = n = ROUNDUP( n) ;
		n += num_of_int * sizeof( int) ;
	}
	pstring_len = NULL ;
	if( num_of_str > 0)
	{
		if( ! ( pstring_len = calloc( num_of_str , sizeof( int))))
		{
			return NULL ;
		}
		string_list_n = n = ROUNDUP(n) ;
		n += num_of_str * sizeof( aux_string_t) ;
		string_n = n = ROUNDUP(n) ;
		for( i = 0 ; i < num_of_str ; i ++ )
		{
			pstring_len[i] = strlen_utf16( str_array[i]) / 2  ;
			n += (pstring_len[i] + 1) * sizeof( IIIMP_card16) ;
		}
	}
	if( ! ( p = calloc( 1 , n)))
	{
		if( pstring_len)
		{
			free( pstring_len) ;
		}
		return  NULL ;
	}

	ac = (aux_composed_t *) p ;
	ac->len = n ;
	ac->event = event ;
	data = (aux_data_t*)( p + aux_data_t_n) ;
	ac->data = data ;

	if( aux)
	{
		ac->aux = aux ;
		data->im = aux->im_data->im_id ;
		data->ic = aux->im_data->ic_id ;
	}

	data->aux_index = class_index ;
	data->aux_name = p + aux_name_n ;
	memcpy( data->aux_name , aux_name , (aux_name_len + 1) * sizeof( IIIMP_card16)) ;
	data->aux_name_length = aux_name_len * sizeof( IIIMP_card16) ;

	data->integer_count = num_of_int ;
	if( num_of_int > 0)
	{
		data->integer_list = (int*)(p + integer_list_n) ;
		for (i = 0 ; i < num_of_int ; i ++)
		{
			data->integer_list[i] = int_array[i] ;
		}
	}
	data->string_count = num_of_str ;
	data->string_ptr = p ;
	if (num_of_str > 0) {
		aux_string_t *pas ;

		data->string_list = pas = (aux_string_t*)(p + string_list_n) ;
		p += string_n ;
		for( i = 0 ; i < num_of_str ; i ++ , pas ++)
		{
			pas->len = pstring_len[i] * sizeof( IIIMP_card16) ;
			pas->ptr = p ;
			n = (pstring_len[i] + 1) * sizeof( IIIMP_card16) ;
			memcpy( p , str_array[i] , n) ;
			p += n ;
		}
	}

	if( pstring_len)
	{
		free( pstring_len) ;
	}


	return  ac ;
}

static aux_composed_t *
create_composed_from_aux_data(
	const aux_data_t *  data
	)
{
	u_char *  p ;
	aux_composed_t *  ac ;
	aux_data_t *  data2 ;

	int  i ;
	int  n ;
	int  aux_data_t_n ;
	int  aux_name_n ;
	int  integer_list_n ;
	int  string_list_n ;
	int  string_n ;


	/* first of all, caliculate size. */
	n = ROUNDUP( sizeof( aux_composed_t)) ;
	aux_data_t_n = n ;
	n += sizeof( aux_data_t) ;
	aux_name_n = n = ROUNDUP( n) ;
	n += data->aux_name_length + sizeof( IIIMP_card16) ;
	integer_list_n = n = ROUNDUP( n) ;
	n += data->integer_count * sizeof( int) ;
	string_list_n = n = ROUNDUP( n) ;
	n += data->string_count * sizeof( aux_string_t) ;
	string_n = n = ROUNDUP( n) ;
	for( i = 0 ; i < data->string_count ; i ++)
	{
		n += data->string_list[i].len + sizeof( IIIMP_card16) ;
	}

	if( ! ( p = calloc( 1 , n)))
	{
		return  NULL ;
	}

	ac = (aux_composed_t *) p ;
	ac->len = n ;
	data2 = (aux_data_t*)( p + aux_data_t_n) ;
	ac->data = data2 ;

	*data2 = *data ;
	data2->aux_name = p + aux_name_n ;
	memcpy( data2->aux_name , data->aux_name , data->aux_name_length) ;

	if( data->integer_count > 0)
	{
		data2->integer_list = (int*)(p + integer_list_n) ;
		memcpy(data2->integer_list , data->integer_list,
		       sizeof( int) * data->integer_count) ;
	}
	else
	{
		data2->integer_list = NULL ;
	}

	data2->string_ptr = p ;
	if( data->string_count > 0)
	{
		aux_string_t *  pas1 ;
		aux_string_t *  pas2 ;

		pas1= data->string_list ;
		data2->string_list = pas2 = (aux_string_t*)(p + string_list_n) ;
		p += string_n ;
		for( i = 0 ; i < data->string_count ; i ++ , pas1 ++ , pas2 ++)
		{
			pas2->len = pas1->len ;
			pas2->ptr = p ;
			memcpy( p , pas1->ptr , pas2->len) ;
			p += pas2->len + sizeof( IIIMP_card16) ;
		}
	}
	else
	{
		data2->string_list = NULL ;
	}

	return  ac ;
}

#undef ROUNDUP

static void
service_setvalue(
	aux_t *  aux ,
	const u_char *  p ,
	int  len
	)
{
	aux_composed_t *  ac ;

#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	ac = (aux_composed_t *) p ;

	if( ac == NULL)
	{
		return ;
	}

	if( ac->event)
	{
		if( iiimcf_forward_event( aux->iiimf->context , ac->event) == IIIMF_STATUS_SUCCESS)
		{
			im_iiimf_process_event( aux->iiimf) ;
		}
	}
	else if( ac->data)
	{
		int  i ;
		aux_data_t *  data = ac->data ;
		IIIMCF_event  event ;
		IIIMP_card32 *  int_array = NULL ;
		const IIIMP_card16 **  str_array = NULL ;

		if( data->integer_count > 0)
		{
			if( ! ( int_array = calloc( data->integer_count , sizeof( IIIMP_card32))))
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
			#endif
				return ;
			}
			for( i = 0 ; i < data->integer_count ; i ++)
			{
				int_array[i] = data->integer_list[i] ;
			}
		}

		if( data->string_count > 0)
		{
			if( ! ( str_array = calloc( data->string_count , sizeof( IIIMP_card16 *))))
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
			#endif
				if( int_array)
				{
					free( int_array) ;
				}
				return ;
			}
			for( i = 0 ; i < data->string_count ; i ++)
			{
				str_array[i] = (const IIIMP_card16*)data->string_list[i].ptr ;
			}
		}

		if( iiimcf_create_aux_setvalues_event( (IIIMP_card16*)data->aux_name ,
						       data->aux_index ,
						       data->integer_count ,
						       int_array ,
						       data->string_count ,
						       str_array ,
						       &event) == IIIMF_STATUS_SUCCESS)
		{
			if( iiimcf_forward_event( aux->iiimf->context ,
						  event) == IIIMF_STATUS_SUCCESS)
			{
				im_iiimf_process_event( aux->iiimf) ;
			}
		}

		if( int_array)
		{
			free( int_array) ;
		}
		if( str_array)
		{
			free( str_array) ;
		}
	}
}

static void
service_getvalue(
	aux_t *  aux ,
	const u_char *  p ,
	int  len
	)
{
	aux_composed_t *  ac ;

#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	ac = (aux_composed_t *) p ;

	if( ac == NULL)
	{
		return ;
	}

	if( ac->event)
	{
		if( iiimcf_forward_event( aux->iiimf->context , ac->event) == IIIMF_STATUS_SUCCESS)
		{
			im_iiimf_process_event( aux->iiimf) ;
		}
	}
	else if( ac->data)
	{
		int  i ;
		aux_data_t *  data = ac->data ;
		IIIMCF_event  event ;
		IIIMP_card32 *  int_array = NULL ;
		const IIIMP_card16 **  str_array = NULL ;

		if( data->integer_count > 0)
		{
			if( ! ( int_array = calloc( data->integer_count , sizeof( IIIMP_card32))))
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " calloc failed.\n") ;
			#endif
				return ;
			}
			for( i = 0 ; i < data->integer_count ; i ++)
			{
				int_array[i] = data->integer_list[i] ;
			}
		}

		if( data->string_count > 0)
		{
			if( ! ( str_array = calloc( data->string_count , sizeof( IIIMP_card16 *))))
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " calloc failed.\n") ;
			#endif
				if( int_array)
				{
					free( int_array) ;
				}
				return ;
			}
			for( i = 0 ; i < data->string_count ; i ++)
			{
				str_array[i] = (const IIIMP_card16*)data->string_list[i].ptr ;
			}
		}

		if( iiimcf_create_aux_getvalues_event( (IIIMP_card16*)data->aux_name ,
						       data->aux_index ,
						       data->integer_count ,
						       int_array ,
						       data->string_count ,
						       str_array ,
						       &event) == IIIMF_STATUS_SUCCESS)
		{
			if( iiimcf_forward_event( aux->iiimf->context ,
						  event) == IIIMF_STATUS_SUCCESS)
			{
				im_iiimf_process_event( aux->iiimf) ;
			}
		}

		if( int_array)
		{
			free( int_array) ;
		}
		if( str_array)
		{
			free( str_array) ;
		}
	}
}

static int
service_im_id(
	aux_t *  aux
	)
{
#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif
	return  aux->im_data->im_id ;
}

static int
service_ic_id(
	aux_t *  aux
	)
{
#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif
	return  aux->im_data->ic_id ;
}

static void
service_data_set(
	aux_t *  aux ,
	int  im_id ,
	void *  data
	)
{
	aux_im_data_t *  im_data ;

#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

#if  0 /* XXX */
	for( im_data = aux->im_data ; im_data ; im_data = im_data->next)
#else
	for( im_data = aux->im_data_list ; im_data ; im_data = im_data->next)
#endif
	{
		if( im_data->im_id == im_id)
		{
			im_data->data = data ;
		}
	}

	return  ;
}

static void *
service_data_get(
	aux_t *  aux ,
	int  im_id
	)
{
	aux_im_data_t *  im_data ;

#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

#if  0 /* XXX */
	for( im_data = aux->im_data ; im_data ; im_data = im_data->next)
#else
	for( im_data = aux->im_data_list ; im_data ; im_data = im_data->next)
#endif
	{
		if( im_data->im_id == im_id)
		{
			return  im_data->data ;
		}
	}

	return  NULL ;
}

static Display *
service_display(
	aux_t *  aux
	)
{
#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	if( aux->iiimf == NULL)
	{
		return  NULL ;
	}

	return  ((x_window_t *)aux->iiimf->im.listener->self)->display ;
}

static Window
service_window(
	aux_t *  aux
	)
{
#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif
	if( aux->iiimf == NULL)
	{
		return  None ;
	}

	return  ((x_window_t *)aux->iiimf->im.listener->self)->my_window ;
}

static XPoint *
service_point(
	aux_t *  aux ,
	XPoint *  point
	)
{
	int  x ;
	int  y ;

#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	if( aux->iiimf == NULL)
	{
		point->x = -1 ;
		point->y = -1 ;

		return  point ;
	}

	(*aux->iiimf->im.listener->get_spot)( aux->iiimf->im.listener->self ,
					      aux->iiimf->im.preedit.chars ,
					      aux->iiimf->im.preedit.segment_offset ,
					      &x , &y) ;

	x -= ((x_window_t *)aux->iiimf->im.listener->self)->x ;
	y -= ((x_window_t *)aux->iiimf->im.listener->self)->y ;

	point->x = (x > SHRT_MAX) ? SHRT_MAX : x ;
	point->y = (y > SHRT_MAX) ? SHRT_MAX : y ;

	return  point ;
}

static XPoint *
service_point_caret(
	aux_t *  aux ,
	XPoint *  point
	)
{
#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	return  service_point( aux , point) ;
}

static size_t
service_utf16_mb(
	const char **  in_buf ,
	size_t *  in_bytes_left ,
	char **  out_buf ,
	size_t *  out_bytes_left
	)
{
#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif
	/* not implemented yet */
	return  0 ;
}

static size_t
service_mb_utf16(
	const char **  in_buf ,
	size_t *  in_bytes_left ,
	char **  out_buf ,
	size_t *  out_bytes_left
	)
{
#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif
	/* not implemented yet */
	return  0 ;
}

static u_char *
service_compose(
	const aux_data_t *  data ,
	int *  size
	)
{
	aux_composed_t *  ac ;

#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	ac = create_composed_from_aux_data( data) ;

	return  (u_char *) ac ;
}

static int
service_compose_size(
	aux_data_type_t  type ,
	const u_char *  p
	)
{
#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	return  0 ;
}

static aux_data_t *
service_decompose(
	aux_data_type_t  type ,
	const u_char *  p
	)
{
	aux_composed_t *  ac ;

#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	ac = (aux_composed_t *) p ;

	if( ac->data)
	{
		if( ! ( ac = create_composed_from_aux_data( ac->data)))
		{
			return  NULL ;
		}
		ac->data->type = type ;
		return  ac->data ;
	}

	if( ac->event)
	{
		if( ! ( ac = create_composed_from_event( ac->aux , ac->event)))
		{
			return  NULL ;
		}
		ac->data->type = type ;

		return  ac->data ;
	}

	return  NULL ;
}

static void
service_decompose_free(
	aux_data_t *  data
	)
{
#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	if( data && data->string_ptr)
	{
		free( data->string_ptr) ;
	}
}

/* XXX: taken from xc/lib/X11/Xlcint.h */
void _XRegisterFilterByType( Display *  display , Window  window ,
			     int  start_type , int  end_type ,
			     Bool (* filter)( Display * , Window ,
					      XEvent * , XPointer) ,
			     XPointer  client_data) ;
void _XUnregisterFilter( Display *  display , Window  window ,
			 Bool (* filter)( Display * , Window ,
					  XEvent * , XPointer) ,
			 XPointer  client_data) ;

static void
service_register_X_filter(
	Display *  display ,
	Window  window ,
	int  start_type ,
	int  end_type ,
	Bool (* filter)( Display * , Window , XEvent * , XPointer) ,
	XPointer  client_data
	)
{
	filter_info_t *  filter_info ;

#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	for( filter_info = filter_info_list ; filter_info ; filter_info = filter_info->next)
	{
		if( filter_info->display == display &&
		    filter_info->window == window &&
		    filter_info->filter == filter &&
		    filter_info->client_data == client_data)
		{
			return ;
		}
	}

	if( ! ( filter_info = malloc( sizeof( filter_info_t))))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif
		return ;
	}

	filter_info->display = display ;
	filter_info->window = window ;
	filter_info->filter = filter ;
	filter_info->client_data = client_data ;

	filter_info->next = filter_info_list ;
	filter_info_list = filter_info ;

	_XRegisterFilterByType( display , window , start_type , end_type ,
				filter , client_data) ;
}

static void
service_unregister_X_filter(
	Display *  display ,
	Window  window ,
	Bool (* filter)(Display * , Window , XEvent * , XPointer),
	XPointer  client_data
	)
{
	filter_info_t *  filter_info ;

#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	for( filter_info = filter_info_list ; filter_info ; filter_info = filter_info->next)
	{
		if( filter_info->display == display &&
		    filter_info->window == window &&
		    filter_info->filter == filter &&
		    filter_info->client_data == client_data)
		{
			filter_info->display = NULL ;
		}
	}

	_XUnregisterFilter( display , window , filter , client_data) ;
}

static Bool
service_server(
	aux_t *  aux
	)
{
#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif
	return  False ;
	/* http://www.openi18n.org/subgroups/im/iiimf/aux/XAuxPrimitive.html#server_client */
}

static Window
service_client_window(
	aux_t *  aux
	)
{
#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif
	if( aux->iiimf == NULL)
	{
		return  None ;
	}

	return  ((x_window_t *)aux->iiimf->im.listener->self)->my_window ;
}

static Window
service_focus_window(
	aux_t *  aux
	)
{
#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	if( aux->iiimf == NULL)
	{
		return  None ;
	}

	return  ((x_window_t *)aux->iiimf->im.listener->self)->my_window ;
}

static int
service_screen_number(
	aux_t *  aux
	)
{
#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif
	if( aux->iiimf == NULL)
	{
		return  -1 ;
	}

	return  ((x_window_t *)aux->iiimf->im.listener->self)->screen ;
}

static int
service_point_screen(
	aux_t *  aux ,
	XPoint *  point
	)
{
	int  x ;
	int  y ;

#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	if( aux->iiimf == NULL)
	{
		point->x = -1 ;
		point->y = -1 ;

		return  -1 ;
	}

	(*aux->iiimf->im.listener->get_spot)(
					aux->iiimf->im.listener->self ,
					aux->iiimf->im.preedit.chars ,
					aux->iiimf->im.preedit.segment_offset ,
					&x , &y) ;

	point->x = (x > SHRT_MAX) ? SHRT_MAX : x ;
	point->y = (y > SHRT_MAX) ? SHRT_MAX : y ;

	return  ((x_window_t *)aux->iiimf->im.listener->self)->screen ;
}

static int
service_point_caret_screen(
	aux_t *  aux ,
	XPoint *  point
	)
{
#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	return  service_point_screen( aux , point) ;
}

static Bool
service_get_conversion_mode(
	aux_t *  aux
	)
{
#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	return  aux->iiimf->on ? True : False ;
}

static void
service_set_conversion_mode(
	aux_t *  aux ,
	int  on
	)
{
	IIIMCF_event  event ;

#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	if( iiimcf_create_trigger_notify_event( on == 1 ? 1 : 0 , &event) != IIIMF_STATUS_SUCCESS)
	{
		return ;
	}

	if( iiimcf_forward_event( aux->iiimf->context , event) != IIIMF_STATUS_SUCCESS)
	{
		return ;
	}

	im_iiimf_process_event( aux->iiimf) ;
}

static aux_t *
service_aux_get_from_id(
	int  im_id ,
	int  ic_id ,
	IIIMP_card16 *  aux_name ,
	int aux_name_length
	)
{
	aux_t *  aux = NULL ;
	KIK_ITERATOR( aux_id_info_t)  iterator = NULL ;
	aux_im_data_t *  im_data ;

#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	if( id_info_list == NULL)
	{
		return  NULL ;
	}

	iterator = kik_list_first( id_info_list) ;

	while( iterator)
	{
		if( kik_iterator_indirect( iterator) == NULL)
		{
			kik_error_printf(
				"iterator found , but it has no logs."
				"don't you cross over memory boundaries anywhere?\n") ;
		}
		else if ( kik_iterator_indirect( iterator)->im_id == im_id &&
			  kik_iterator_indirect( iterator)->ic_id == ic_id)
		{
			aux = kik_iterator_indirect( iterator)->aux ;
			break ;
		}

		iterator = kik_iterator_next( iterator) ;
	}

	if( aux == NULL)
	{
		return  NULL ;
	}

	for( im_data = aux->im_data_list ; im_data ; im_data = im_data->next)
	{
		if( memcmp( aux_name ,
			    im_data->entry->dir.name.utf16str ,
			    im_data->entry->dir.name.len) == 0)
		{
			aux->im_data = im_data ;
			return  aux ;
		}
	}

	if( ! ( im_data = create_im_data( aux->iiimf->context , (const IIIMP_card16 *)aux_name)))
	{
		return  NULL ;
	}

	im_data->next = aux->im_data_list ;
	aux->im_data_list = im_data ;
	aux->im_data = im_data ;

	if( ! im_data->entry->created)
	{
		if( ! (*im_data->entry->dir.method->create)( aux))
		{
			return  NULL ;
		}
		im_data->entry->created = 1 ;
	}

	return  aux ;
}

static aux_service_t  aux_service =
{
	service_setvalue ,
	service_im_id ,
	service_ic_id ,
	service_data_set ,
	service_data_get ,
	service_display ,
	service_window ,
	service_point ,
	service_point_caret ,
	service_utf16_mb ,
	service_mb_utf16 ,
	service_compose ,
	service_compose_size ,
	service_decompose ,
	service_decompose_free ,
	service_register_X_filter ,
	service_unregister_X_filter ,
	service_server ,
	service_client_window ,
	service_focus_window ,
	service_screen_number ,
	service_point_screen ,
	service_point_caret_screen ,
	service_get_conversion_mode ,
	service_set_conversion_mode ,
	service_getvalue ,
	service_aux_get_from_id

} ;


/* --- global functions --- */

void
aux_init(
	IIIMCF_handle  iiimcf_handle ,
	x_im_export_syms_t *  export_syms ,
	mkf_parser_t *  _parser_utf16
	)
{
	const IIIMCF_object_descriptor *  objdesc_list ;
	const IIIMCF_object_descriptor **  bin_objdesc_list = NULL ;
	IIIMCF_downloaded_object * objs = NULL ;
	int  num_of_objs ;
	int  num_of_bin_objs ;
	int  i ;

	if( initialized)
	{
		return ;
	}

#ifdef  AUX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	handle = iiimcf_handle ;
	syms = export_syms ;
	parser_utf16 = _parser_utf16 ;

	if( ! ( conv_iso88591 = (*syms->ml_conv_new)( ML_ISO8859_1)))
	{
		return ;
	}

	kik_list_new( aux_module_info_t , module_info_list) ;

	initialized = 1 ;


	if( iiimcf_get_object_descriptor_list( handle ,
					       &num_of_objs ,
					       &objdesc_list) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " iiimcf_get_object_descriptor_list failed.\n");
	#endif
		return ;
	}

	bin_objdesc_list = alloca( sizeof( IIIMCF_object_descriptor*) * num_of_objs) ;
	objs = alloca( sizeof( IIIMCF_downloaded_object) * num_of_objs) ;

	if( ! bin_objdesc_list || ! objs)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " alloca failed.\n") ;
	#endif
		return ;
	}

	for( i = 0 , num_of_bin_objs = 0 ; i < num_of_objs; i ++)
	{
		if( objdesc_list[i].predefined_id == IIIMP_IMATTRIBUTE_BINARY_GUI_OBJECT)
		{
			bin_objdesc_list[num_of_bin_objs] = &objdesc_list[i] ;
			num_of_bin_objs ++ ;
		}
	}

	if( iiimcf_get_downloaded_objects( handle ,
					   num_of_bin_objs ,
					   bin_objdesc_list ,
					   objs) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG "iiimcf_get_downloaded_objects failed\n");
	#endif
	#ifdef  ATOK12_HACK
		register_atok12_module() ;
	#endif
		return ;
	}

	for( i = 0 ; i < num_of_bin_objs ; i ++)
	{
		const IIIMP_card16 *  obj_name ;
		u_char *  file_name = NULL ;

		if( iiimcf_get_downloaded_object_filename( objs[i] ,
							   &obj_name) != IIIMF_STATUS_SUCCESS)
		{
			continue ;
		}

		PARSER_INIT_WITH_BOM( parser_utf16) ;
		im_convert_encoding( parser_utf16 , conv_iso88591 ,
				     (u_char*)obj_name , &file_name,
				     strlen_utf16( obj_name) + 1) ;
		if( is_conf_file( file_name))
		{
			kik_file_t *  from ;

			/*
			 * [format]
			 * ---------------------------------------------
			 * # IIIM X auxiliary
			 * <name of X aux object1> <path to the object1>
			 * <name of X aux object2> <path to the object2>
			 *            :
			 *            :
			 * <name of X aux objectN> <path to the objectN>
			 * ---------------------------------------------
			 */
			if( ( from = kik_file_open( file_name , "r")))
			{
				char *  line ;
				size_t  len ;

				while( ( line = kik_file_get_line( from , &len)))
				{
					char *  aux_name ;
					char *  mod_name ;

					if( *line == '#' || *line == '\n')
					{
						continue ;
					}

					line[ len - 1] = '\0' ;

					while( *line == ' ' || *line == '\t')
					{
						line ++ ;
					}
					aux_name = kik_str_sep( &line , " ") ;
					if( ! aux_name || ! line)
					{
						continue ;
					}

					while( *line == ' ' || *line == '\t')
					{
						line ++ ;
					}
					mod_name = kik_str_chop_spaces( line) ;

					register_module_info( aux_name , mod_name) ;
				}
				kik_file_close( from) ;
			}
		}
		else /* ! is_conf_file( file_name) */
		{
			aux_module_t *  module ;

			if( ( module = load_module( file_name)))
			{
				aux_entry_t *  entry ;
				int  j ;

				for( j = 0 , entry = module->entries ;
				     j < module->num_of_entries ;
				     j ++ , entry ++)
				{
					u_char *  aux_name = NULL ;

					PARSER_INIT_WITH_BOM( parser_utf16) ;
					im_convert_encoding( parser_utf16 ,
							     conv_iso88591 ,
							     (u_char*)entry->dir.name.utf16str ,
							     &aux_name ,
							     entry->dir.name.len + 1) ;

					register_module_info( aux_name , file_name) ;
					free( aux_name) ;
				}

				unload_module( module) ;
			}
		}

		free( file_name) ;
	}

	return ;
}

void
aux_quit( void)
{
	KIK_ITERATOR( aux_module_info_t)  iterator ;
	aux_module_t *  module ;
	filter_info_t *  filter_info ;

	if( ! initialized)
	{
		return ;
	}


	iterator = kik_list_first( module_info_list) ;

	while( iterator)
	{
		aux_module_info_t *  module_info ;
		if( kik_iterator_indirect( iterator) == NULL)
		{
			kik_error_printf(
				"iterator found , but it has no logs."
				"don't you cross over memory boundaries anywhere?\n") ;
		}

		module_info = kik_iterator_indirect( iterator) ;

		if( module_info->aux_name)
		{
			free( module_info->aux_name) ;
		}

		if( module_info->file_name)
		{
			free( module_info->file_name) ;
		}

		free( module_info) ;

		iterator = kik_iterator_next( iterator) ;
	}

	kik_list_delete( aux_module_info_t , module_info_list) ;
	module_info_list = NULL ;


	if( id_info_list)
	{
		kik_list_delete( aux_id_info_t , id_info_list) ;
		id_info_list = NULL ;
	}


	for( module = module_list ; module ; )
	{
		aux_module_t *  unused ;

		unused = module ;
		module = module->next ;

		unload_module( unused) ;
	}
	module_list = NULL ;


	for( filter_info = filter_info_list ; filter_info ; )
	{
		filter_info_t *  unused ;

		if( filter_info->display)
		{
			/* hack */
			_XUnregisterFilter( filter_info->display ,
					    filter_info->window ,
					    filter_info->filter ,
					    filter_info->client_data) ;
		}

		unused = filter_info ;
		filter_info = filter_info->next ;

		free( unused) ;
	}
	filter_info_list = NULL ;


	if( conv_iso88591)
	{
		(*conv_iso88591->delete)( conv_iso88591) ;
		conv_iso88591 = NULL ;
	}

	initialized = 0 ;
}

aux_t *
aux_new(
	im_iiimf_t *  iiimf
	)
{
	aux_id_info_t *  id_info = NULL ;
	aux_t *  aux = NULL ;

	if( ! ( id_info = malloc( sizeof( aux_id_info_t))))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif
		return  NULL ;
	}

	if( ! ( aux = malloc( sizeof( aux_t))))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif
		goto  error ;
	}

	if( iiimcf_get_im_id( handle , &id_info->im_id) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG "iiimcf_get_im_id failed.\n") ;
	#endif
		goto  error ;
	}
	if( iiimcf_get_ic_id( iiimf->context, &id_info->ic_id) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG "iiimcf_get_ic_id failed.\n") ;
	#endif
		goto  error ;
	}

	id_info->aux = aux ;

	aux->iiimf = iiimf ;
	aux->service = &aux_service ;
	aux->im_data = NULL ;
	aux->im_data_list = NULL ;

	if( id_info_list == NULL)
	{
		kik_list_new( aux_id_info_t , id_info_list) ;
	}

	kik_list_insert_head( aux_id_info_t , id_info_list , id_info) ;

	return  aux ;

error:
	if( aux)
	{
		free( aux) ;
	}

	if( id_info)
	{
		free( id_info) ;
	}

	return  NULL ;
}

void
aux_event(
	aux_t *  aux ,
	IIIMCF_event  event ,
	IIIMCF_event_type  type
	)
{
	aux_im_data_t *  im_data ;
	aux_composed_t  ac ;
	const IIIMP_card16 *  aux_name ;

	if( iiimcf_get_aux_event_value( event , &aux_name , NULL , NULL , NULL ,
					NULL , NULL) != IIIMF_STATUS_SUCCESS)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " iiimcf_get_aux_event_value() failed\n");
	#endif
		return ;
	}

	for( im_data = aux->im_data_list ; im_data ; im_data = im_data->next)
	{
		if( memcmp( aux_name ,
			    im_data->entry->dir.name.utf16str ,
			    im_data->entry->dir.name.len) == 0)
		{
			aux->im_data = im_data ;
		}
	}

	if( im_data == NULL)
	{
		if( ! ( im_data = create_im_data( aux->iiimf->context , aux_name)))
		{
			return ;
		}

		im_data->next = aux->im_data_list ;
		aux->im_data_list = im_data ;
		aux->im_data = im_data ;
	}

	if( ! im_data->entry->created)
	{
		if( (*im_data->entry->dir.method->create)( aux) == False)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG "aux object internal error. [create]\n") ;
		#endif
			return ;
		}
		im_data->entry->created = 1 ;
	}

	ac.len = 0 ;
	ac.aux = aux ;
	ac.event = event ;
	ac.data = NULL ;

	switch( type)
	{
	case IIIMCF_EVENT_TYPE_AUX_START:
		if( im_data->entry->dir.method->start)
		{
			if( (*im_data->entry->dir.method->start)( aux ,
								  (XPointer)&ac,
								  0) == False)
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG "aux object internal error. [start]\n") ;
			#endif
			}
		}
		break ;
	case IIIMCF_EVENT_TYPE_AUX_DRAW:
		if( im_data->entry->dir.method->draw)
		{
			if( (*im_data->entry->dir.method->draw)( aux ,
								 (XPointer)&ac,
								 0) == False)
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG "aux object internal error. [draw]\n") ;
			#endif
			}
		}
		break ;
#if 0
	case IIIMCF_EVENT_TYPE_AUX_SETVALUES:
		/* XXX */
		break ;
#endif
	case IIIMCF_EVENT_TYPE_AUX_DONE:
		if( im_data->entry->dir.method->done)
		{
			if( (*im_data->entry->dir.method->done)( aux ,
								 (XPointer)&ac,
								 0) == False)
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG "aux object internal error. [done]\n") ;
			#endif
			}
		}
		break ;
#ifdef  HAVE_AUX_GETVALUES_EVENT
	case IIIMCF_EVENT_TYPE_AUX_GETVALUES:
		if( im_data->entry->dir.method->getvalues_reply)
		{
			if( (*im_data->entry->dir.method->getvalues_reply)(
								aux ,
								(XPointer) &ac ,
								0) == False)
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG "aux object internal error. [getvalues_reply]\n") ;
			#endif
			}
		}
		break ;
#endif
	default:
		break ;
	}

	return;
}

void
aux_set_focus(
	aux_t *  aux
	)
{
	aux_im_data_t *  im_data ;

	for( im_data = aux->im_data_list ; im_data ; im_data = im_data->next)
	{
		if( im_data->entry->if_version >= AUX_IF_VERSION_2 &&
		    im_data->entry->dir.method->set_icfocus)
		{
			aux->im_data = im_data ;
			(*im_data->entry->dir.method->set_icfocus)( aux) ;
		}
	}
}

void
aux_unset_focus(
	aux_t *  aux
	)
{
	aux_im_data_t *  im_data ;

	for( im_data = aux->im_data_list ; im_data ; im_data = im_data->next)
	{
		if( im_data->entry->if_version >= AUX_IF_VERSION_2 &&
		    im_data->entry->dir.method->unset_icfocus)
		{
			aux->im_data = im_data ;
			(*im_data->entry->dir.method->unset_icfocus)( aux) ;
		}
	}
}

void
aux_delete(
	aux_t *  aux
	)
{
	KIK_ITERATOR( aux_id_info_t)  iterator = NULL ;
	aux_im_data_t *  im_data ;

	aux->iiimf = NULL ;

	for( im_data = aux->im_data_list ; im_data ;)
	{
		aux_im_data_t *  unused ;

		if( im_data->entry->if_version >= AUX_IF_VERSION_2 &&
		    im_data->entry->dir.method->destroy_ic &&
		    im_data->entry->created)
		{
			aux->im_data = im_data ;
			(*im_data->entry->dir.method->destroy_ic)( aux) ;
		}

		unused = im_data ;
		im_data = im_data->next ;

		free( unused) ;
	}

	iterator = kik_list_first( id_info_list) ;
	while( iterator)
	{
		if( kik_iterator_indirect( iterator) == NULL)
		{
			kik_error_printf(
				"iterator found , but it has no logs."
				"don't you cross over memory boundaries anywhere?\n") ;
		}
		else if ( kik_iterator_indirect( iterator)->aux == aux)
		{
			free( kik_iterator_indirect( iterator)) ;
			kik_list_remove( aux_id_info_t , id_info_list , iterator) ;

			break ;
		}

		iterator = kik_iterator_next( iterator) ;
	}

	free( aux) ;
}

