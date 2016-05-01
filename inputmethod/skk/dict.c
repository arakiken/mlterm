/*
 *	$Id$
 */

#include  "dict.h"

#include  <fcntl.h>
#include  <unistd.h>
#include  <sys/stat.h>
#include  <string.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>
#include  <kiklib/kik_path.h>
#include  <mkf/mkf_eucjp_parser.h>
#include  <mkf/mkf_eucjp_conv.h>
#include  <mkf/mkf_utf8_parser.h>
#include  <mkf/mkf_utf8_conv.h>

/* for serv_search() */
#include  <kiklib/kik_def.h>	/* USE_WIN32API */
#ifdef  USE_WIN32API
#include  <winsock2.h>
#endif
#include  <kiklib/kik_net.h>	/* getaddrinfo/socket/connect */
#include  <errno.h>

#include  "mkf_str_parser.h"
#include  "../im_common.h"


#ifndef  USE_WIN32API
#define  closesocket( sock)  close( sock)
#endif

#define  MAX_TABLES  256	/* MUST BE 2^N (see calc_index()) */
#define  UTF_MAX_SIZE  6	/* see ml_char.h */


typedef struct table
{
	char **  entries ;
	u_int  num ;

} table_t ;


/* --- static variables --- */

static char *  global_dict ;
static int  sock = -1 ;


/* --- static functions --- */

static int
calc_index(
	char *  p
	)
{
	char *  end ;
	u_int  idx ;

	if( ! ( end = strchr( p , ' ')))
	{
		return  -1 ;
	}

	if( p + 6 < end)
	{
		end = p + 6 ;
	}

	idx = 0 ;
	while( p < end)
	{
		idx += *(p++) ;
	}

	idx &= (MAX_TABLES - 1) ;

	return  idx ;
}


static void
invalidate_entry(
	char *  entry ,
	char *  data ,
	size_t  data_size
	)
{
	char *  p ;

	if( data <= entry && entry < data + data_size)
	{
		if( ( p = strchr( entry , ' ')) && p[1] == '/')
		{
			p[1] = 'X' ;
		}
	}
	else
	{
		free( entry) ;
	}
}

static int
is_invalid_entry(
	char *  entry
	)
{
	char *  p ;

	if( ! ( p = strchr( entry , ' ')) || p[1] == 'X')
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}


static char *
file_load(
	size_t *  size ,
	table_t *  tables ,
	char *  path ,
	int  rw
	)
{
	int  fd ;
	struct stat  st ;
	char *  data ;
	char *  p ;
	int  count ;
	u_int  filled_nums[MAX_TABLES] ;

	if( rw && stat( path , &st) != 0)
	{
		creat( path , 0600) ;
		free( path) ;

		return  NULL ;
	}

	fd = open( path , O_RDONLY , 0) ;
	free( path) ;

	if( fd < 0 || fstat( fd , &st) != 0 || st.st_size == 0 ||
	    ! ( data = malloc( st.st_size + 1)))
	{
		return  NULL ;
	}

	if( read( fd , data , st.st_size) != st.st_size)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Failed to read skk-jisyo.\n") ;
	#endif

		free( data) ;
		data = NULL ;

		return  NULL ;
	}
	data[st.st_size] = '\0' ;

	p = data ;
	do
	{
		if( p[0] != ';' || p[1] != ';')
		{
			if( ( count = calc_index(p)) != -1)
			{
				tables[count].num ++ ;
			}
		}

		if( ! ( p = strchr( p , '\n')))
		{
			break ;
		}

		p ++ ;
	}
	while( 1) ;

	for( count = 0 ; count < MAX_TABLES ; count++)
	{
		if( ! ( tables[count].entries = malloc( sizeof(*tables[0].entries) *
							tables[count].num)))
		{
			tables[count].num = 0 ;
		}
	}

	memset( filled_nums , 0 , sizeof(filled_nums)) ;
	p = data ;
	do
	{
		if( p[0] != ';' || p[1] != ';')
		{
			if( ( count = calc_index(p)) != -1)
			{
				tables[count].entries[filled_nums[count]++] = p ;
			}
		}

		if( ! ( p = strchr( p , '\n')))
		{
			break ;
		}

		*(p++) = '\0' ;
	}
	while( 1) ;

#if  0
	for( count = 0 ; count < MAX_TABLES ; count++)
	{
		kik_debug_printf( "%d %d\n" , count , filled_nums[count]) ;
	}
#endif

	*size = st.st_size ;

	return  data ;
}

static void
file_unload(
	table_t *  tables ,
	char *  data ,
	size_t  data_size ,
	char *  path
	)
{
	u_int  tbl_idx ;
	u_int  ent_idx ;
	FILE *  fp ;
	char *  p ;

	if( path)
	{
		fp = fopen( path , "w") ;
		free( path) ;
	}
	else
	{
		fp = NULL ;
	}

	if( fp)
	{
		p = data ;
		while( p < data + data_size)
		{
			if( ! is_invalid_entry( p))
			{
				fprintf( fp , "%s\n" , p) ;
			}

			p += (strlen( p) + 1) ;
		}
	}

	for( tbl_idx = 0 ; tbl_idx < MAX_TABLES ; tbl_idx++)
	{
		for( ent_idx = 0 ; ent_idx < tables[tbl_idx].num ; ent_idx++)
		{
			if( tables[tbl_idx].entries[ent_idx] < data ||
			    data + data_size <= tables[tbl_idx].entries[ent_idx])
			{
			#ifdef  DEBUG
				kik_debug_printf( "Free %s\n" ,
					tables[tbl_idx].entries[ent_idx]) ;
			#endif

				if( fp)
				{
					fprintf( fp , "%s\n" , tables[tbl_idx].entries[ent_idx]) ;
				}

				free( tables[tbl_idx].entries[ent_idx]) ;
			}
		}

		free( tables[tbl_idx].entries) ;
		tables[tbl_idx].num = 0 ;
	}

	if( fp)
	{
		fclose( fp) ;
	}
}

static size_t
mkf_str_to(
	char *  dst ,
	size_t  dst_len ,
	mkf_char_t *  src ,
	u_int  src_len ,
	mkf_conv_t *  conv
	)
{
	(*conv->init)( conv) ;

	return  (*conv->convert)( conv , dst , dst_len , mkf_str_parser_init( src , src_len)) ;
}

static char *
file_search(
	table_t *  tables ,
	mkf_conv_t *  dic_conv ,
	mkf_parser_t *  dic_parser ,
	mkf_char_t *  caption ,
	u_int  caption_len ,
	mkf_conv_t *  conv
	)
{
	int  idx ;
	u_int  count ;
	char  buf[1024] ;
	size_t  filled_len ;

	filled_len = mkf_str_to( buf , sizeof(buf) - 2 , caption , caption_len , dic_conv) ;
	buf[filled_len] = ' ' ;
	buf[filled_len + 1] = '\0' ;
#ifdef  DEBUG
	kik_debug_printf( "Searching %s\n" , buf) ;
#endif

	idx = calc_index( buf) ;

	for( count = 0 ; count < tables[idx].num ; count++)
	{
		if( strncmp( buf , tables[idx].entries[count] , filled_len + 1) == 0)
		{
			char *  p ;

			strcpy( buf + filled_len ,
				tables[idx].entries[count] + filled_len + 1) ;

			(*dic_parser->init)( dic_parser) ;
			if( im_convert_encoding( dic_parser , conv ,
						 buf , &p , strlen( buf)))
			{
				return  p ;
			}
		}
	}

	return  NULL ;
}


static char *
serv_search(
	mkf_conv_t *  dic_conv ,
	mkf_parser_t *  dic_parser ,
	mkf_char_t *  caption ,
	u_int  caption_len ,
	mkf_conv_t *  conv
	)
{
	struct sockaddr_in  sa ;
	struct hostent *  host ;
	char  buf[1024] ;
	char *  p ;
	size_t  filled_len ;

	if( sock == -1)
	{
		char *  serv ;
		int  port ;

		port = 1178 ;
		if( global_dict && *global_dict)
		{
			char *  p ;

			if( ( p = alloca( strlen( global_dict) + 1)))
			{
				char *  port_str ;

				strcpy( p , global_dict) ;

				if( kik_parse_uri( NULL , NULL , &serv , &port_str ,
					NULL , NULL , p) && port_str)
				{
					port = atoi( port_str) ;
				}
			}
			else
			{
				return  NULL ;
			}
		}
		else
		{
			serv = "localhost" ;
		}

		if( ( sock = socket( AF_INET , SOCK_STREAM , 0)) == -1)
		{
			return  NULL ;
		}

		memset( &sa , 0 , sizeof(sa)) ;
		sa.sin_family = AF_INET ;
		sa.sin_port = htons( port) ;

		if( ! ( host = gethostbyname( serv)))
		{
			goto  error ;
		}

		memcpy( &sa.sin_addr , host->h_addr_list[0] , sizeof(sa.sin_addr)) ;

		if( connect( sock , &sa , sizeof(struct sockaddr_in)) == -1)
		{
			goto  error ;
		}

	#ifdef  USE_WIN32API
		{
			u_long  val = 0 ;
			ioctlsocket( sock , FIONBIO , &val) ;
		}
	#else
		fcntl( sock , F_SETFL , fcntl( sock , F_GETFL , 0) & ~O_NONBLOCK) ;
	#endif
	}

	buf[0] = '1' ;
	filled_len = mkf_str_to( buf + 1 , sizeof(buf) - 3 , caption , caption_len , dic_conv) ;
	buf[1 + filled_len] = ' ' ;
	buf[1 + filled_len + 1] = '\n' ;
	send( sock , buf , filled_len + 3 , 0) ;
#ifndef  USE_WIN32API
	fsync( sock) ;
#endif
#ifdef  DEBUG
	write( 0 , buf , filled_len + 3) ;
#endif

	p = buf ;
	*p = '4' ;
	if( recv( sock , p , 1 , 0) == 1)
	{
		p += (1 + filled_len) ;		/* skip caption */
		while( recv( sock , p , 1 , 0) == 1 && *p != '\n')
		{
			p++ ;
		}
		*p = '\0' ;

	#ifdef  DEBUG
		write( 0 , buf , p - buf) ;
		write( 0 , "\n" , 1) ;
	#endif
	}

	if( *buf != '1')
	{
		return  NULL ;
	}

	(*dic_parser->init)( dic_parser) ;
	if( im_convert_encoding( dic_parser , conv , buf + 1 , &p , strlen( buf + 1)))
	{
		return  p ;
	}
	else
	{
		return  NULL ;
	}

error:
	closesocket( sock) ;
	sock = -1 ;

	return  NULL ;
}


/* --- static variables --- */

static table_t  global_tables[MAX_TABLES] ;
static char *  global_data ;
static size_t  global_data_size ;
static mkf_conv_t *  global_conv ;
static mkf_parser_t *  global_parser ;

static table_t  local_tables[MAX_TABLES] ;
static char *  local_data ;
static size_t  local_data_size ;
static mkf_conv_t *  local_conv ;
static mkf_parser_t *  local_parser ;


/* --- global functions --- */

void
dict_final(void)
{
	file_unload( local_tables , local_data , local_data_size ,
		kik_get_user_rc_path( "mlterm/skk-jisyo")) ;
	free( local_data) ;
	local_data = NULL ;

	if( local_conv)
	{
		(*local_conv->delete)( local_conv) ;
		(*local_parser->delete)( local_parser) ;
	}

	if( global_data)
	{
		file_unload( global_tables , global_data , global_data_size , NULL) ;
		free( global_data) ;
		global_data = NULL ;
	}
	else
	{
		closesocket( sock) ;
		sock = -1 ;
	}

	if( global_conv)
	{
		(*global_conv->delete)( global_conv) ;
		(*global_parser->delete)( global_parser) ;
	}

	free( global_dict) ;
	global_dict = NULL ;
}

char *
dict_search(
	mkf_char_t *  caption ,
	u_int  caption_len ,
	mkf_conv_t *  conv
	)
{
	char *  path ;
	char *  result ;
	char *  hit[2] = { NULL , NULL } ;
	u_int  count ;
	size_t  len ;

	if( ! local_conv)
	{
		local_conv = mkf_utf8_conv_new() ;
		local_parser = mkf_utf8_parser_new() ;
	}

	if( ! local_data)
	{
		if( ( path = kik_get_user_rc_path( "mlterm/skk-jisyo")))
		{
			local_data = file_load( &local_data_size , local_tables , path , 1) ;
		}
	}

	hit[0] = file_search( local_tables , local_conv , local_parser ,
				caption , caption_len , conv) ;

	if( ! global_conv)
	{
		global_conv = mkf_eucjp_conv_new() ;
		global_parser = mkf_eucjp_parser_new() ;
	}

	if( global_data ||
	    ( global_dict && ( path = strdup( global_dict)) &&
	      ( global_data = file_load( &global_data_size , global_tables , path , 0))))
	{
		hit[1] = file_search( global_tables , global_conv , global_parser ,
				caption , caption_len , conv) ;
	}
	else
	{
		hit[1] = serv_search( global_conv , global_parser ,
				caption , caption_len , conv) ;
	}

#ifdef  DEBUG
	kik_debug_printf( "Candidates: local %s + global %s " , hit[0] , hit[1]) ;
#endif

	len = 0 ;
	for( count = 0 ; count < 2 ; count++)
	{
		if( hit[count])
		{
			len += strlen( hit[count]) ;
		}
	}

	if( len == 0)
	{
		return  NULL ;
	}

	if( ( result = malloc( len + 1)))
	{
		result[0] = '\0' ;

		for( count = 0 ; count < 2 ; count++)
		{
			if( hit[count])
			{
				strcat( result ,
					*result ? strchr( hit[count] , '/') + 1 : hit[count]) ;
			}
		}

		free( hit[0]) ;
		free( hit[1]) ;
	}

#ifdef  DEBUG
	kik_msg_printf( "=> %s\n" , result) ;
#endif

	return  result ;
}

int
dict_add(
	char *  caption ,
	char *  word ,
	mkf_parser_t *  parser
	)
{
	size_t  caption_len ;
	char *  p ;
	int  idx ;
	u_int  count ;
	size_t  word_len ;

	if( ! local_conv)
	{
		local_conv = mkf_utf8_conv_new() ;
		local_parser = mkf_utf8_parser_new() ;
	}

	caption_len = strlen(caption) ;
	/* "* UTF_MAX_SIZE" is for Hankaku kana -> UTF8 */
	if( ! ( p = alloca( caption_len * UTF_MAX_SIZE + 2)))
	{
		return  0 ;
	}

	(*parser->init)( parser) ;
	(*parser->set_str)( parser , caption , caption_len) ;
	(*local_conv->init)( local_conv) ;
	caption_len = (*local_conv->convert)( local_conv , p ,
				caption_len * UTF_MAX_SIZE , parser) ;
	p[caption_len++] = ' ' ;
	p[caption_len] = '\0' ;
	caption = p ;

	word_len = strlen( word) ;
	if( ! ( p = alloca( word_len * UTF_MAX_SIZE + 2)))
	{
		return  0 ;
	}

	(*parser->init)( parser) ;
	(*parser->set_str)( parser , word , word_len) ;
	(*local_conv->init)( local_conv) ;
	word_len = (*local_conv->convert)( local_conv , p , word_len * UTF_MAX_SIZE , parser) ;
	p[word_len++] = '/' ;
	p[word_len] = '\0' ;
	word = p ;

	idx = calc_index( caption) ;
	for( count = 0 ; count < local_tables[idx].num ; count++)
	{
		if( strncmp( caption , local_tables[idx].entries[count] , caption_len) == 0)
		{
			char *  tmp ;
			char *  p1 ;
			char *  p2 ;
			char *  p3 ;

			p1 = local_tables[idx].entries[count] ;

			if( ! ( tmp = alloca( strlen( p1) + word_len + 1)))
			{
				return  0 ;
			}

		#ifdef  DEBUG
			kik_debug_printf( "Adding to dictionary: %s to %s\n" , word , p1) ;
		#endif

			p2 = p1 + caption_len ;
			if( *p2 == '/')
			{
				p2 ++ ;
			}

			memcpy( tmp , p1 , p2 - p1) ;

			strcpy( tmp + (p2 - p1) , word) ;

			if( ( p3 = strstr( p2 , word)) && *(p3 - 1) == '/')
			{
				if( p3 > p2)
				{
					tmp[strlen(tmp) + p3 - p2] = '\0' ;
					memcpy( tmp + strlen(tmp) , p2 , p3 - p2) ;
				}

				p3 += word_len ;
			}
			else
			{
				p3 = p2 ;
			}

			strcpy( tmp + strlen(tmp) , p3) ;

			if( strcmp( tmp , local_tables[idx].entries[count]) != 0)
			{
				invalidate_entry( local_tables[idx].entries[count] ,
					local_data , local_data_size) ;
				local_tables[idx].entries[count] = strdup( tmp) ;
			}

			return  1 ;
		}
	}

	if( ( p = realloc( local_tables[idx].entries ,
			sizeof(*local_tables[idx].entries) * (local_tables[idx].num + 1))))
	{
		local_tables[idx].entries = p ;

		if( ( p = malloc( strlen( caption) + 1 + 1 + strlen( word) + 1 + 1)))
		{
			sprintf( p , "%s/%s" , caption , word) ;
			local_tables[idx].entries[local_tables[idx].num++] = p ;
		#ifdef  DEBUG
			kik_debug_printf( "Adding to dictionary: %s\n" , p) ;
		#endif
		}
	}

	return  0 ;
}

void
dict_set_global(
	char *  dict
	)
{
	size_t  len ;

	free( global_dict) ;
	global_dict = strdup( dict) ;

	if( global_data)
	{
		file_unload( global_tables , global_data , global_data_size , NULL) ;
		free( global_data) ;
		global_data = NULL ;
	}
	else
	{
		closesocket( sock) ;
		sock = -1 ;
	}

	if( global_conv)
	{
		(*global_conv->delete)( global_conv) ;
		(*global_parser->delete)( global_parser) ;
	}

	if( ( len = strlen( dict)) > 5 && strcmp( dict + len - 5 , ":utf8") == 0)
	{
		global_conv = mkf_utf8_conv_new() ;
		global_parser = mkf_utf8_parser_new() ;

		global_dict[len - 5] = '\0' ;
	}
	else
	{
		global_conv = NULL ;
		global_parser = NULL ;
	}
}
