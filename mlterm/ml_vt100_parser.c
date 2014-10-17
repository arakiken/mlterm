/*
 *	$Id$
 */

#include  "ml_vt100_parser.h"

#include  <stdio.h>		/* sprintf */
#include  <string.h>		/* memmove */
#include  <stdlib.h>		/* atoi */
#include  <fcntl.h>		/* open */
#include  <unistd.h>		/* write/getcwd */
#include  <sys/time.h>		/* gettimeofday */
#ifdef  DEBUG
#include  <stdarg.h>		/* va_list */
#endif

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* malloc/free */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */
#include  <kiklib/kik_conf_io.h>/* kik_get_user_rc_path */
#include  <kiklib/kik_str.h>	/* kik_str_alloca_dup */
#include  <kiklib/kik_args.h>
#include  <kiklib/kik_unistd.h>	/* kik_usleep */
#include  <kiklib/kik_locale.h>	/* kik_get_locale */
#include  <mkf/mkf_ucs4_map.h>	/* mkf_map_to_ucs4 */
#include  <mkf/mkf_ucs_property.h>
#include  <mkf/mkf_locale_ucs4_map.h>
#include  <mkf/mkf_ko_kr_map.h>

#include  "ml_iscii.h"
#include  "ml_config_proto.h"
#include  "ml_str_parser.h"


/*
 * kterm BUF_SIZE in ptyx.h is 4096.
 * Maximum size of sequence parsed once is PTY_RD_BUFFER_SIZE * 3.
 * (see ml_parse_vt100_sequence)
 */
#define  PTY_RD_BUFFER_SIZE  3072
#define  MAX_READ_COUNT  3

#define  CTL_BEL	0x07
#define  CTL_BS		0x08
#define  CTL_TAB	0x09
#define  CTL_LF		0x0a
#define  CTL_VT		0x0b
#define  CTL_FF		0x0c
#define  CTL_CR		0x0d
#define  CTL_SO		0x0e
#define  CTL_SI		0x0f
#define  CTL_ESC	0x1b

#define  CURRENT_STR_P(vt100_parser) \
	( (vt100_parser)->r_buf.chars + \
		(vt100_parser)->r_buf.filled_len - (vt100_parser)->r_buf.left)

#define  HAS_XTERM_LISTENER(vt100_parser,method) \
	((vt100_parser)->xterm_listener && ((vt100_parser)->xterm_listener->method))

#define  HAS_CONFIG_LISTENER(vt100_parser,method) \
	((vt100_parser)->config_listener && ((vt100_parser)->config_listener->method))

#if  1
#define  MAX_PS_DIGIT  0xffff
#endif

#if  0
#define  EDIT_DEBUG
#endif

#if  0
#define  EDIT_ROUGH_DEBUG
#endif

#if  0
#define  INPUT_DEBUG
#endif

#if  0
#define  ESCSEQ_DEBUG
#endif

#if  0
#define  OUTPUT_DEBUG
#endif

#if  0
#define  DUMP_HEX
#endif

#if  0
#define  SUPPORT_VTE_CJK_WIDTH
#endif

#if  0
#define  SUPPORT_ITERM2_OSC1337
#endif


typedef struct area
{
	u_int32_t  min ;
	u_int32_t  max ;

} area_t ;


/* --- static variables --- */

static int  use_alt_buffer = 1 ;
static int  use_ansi_colors = 1 ;
static struct
{
	u_int16_t  ucs ;
	u_char  decsp ;

} ucs_to_decsp_table[] =
{
	/* Not line characters */
#if  0
	{ 0xa3 , '}' } ,
	{ 0xb0 , 'f' } ,
	{ 0xb1 , 'g' } ,
	{ 0xb7 , '~' } ,
	{ 0x3c0 , '{' } ,
	{ 0x2260 , '|' } ,
	{ 0x2264 , 'y' } ,
	{ 0x2265 , 'z' } ,
#endif

	/* Line characters */
	{ 0x23ba , 'o' } ,
	{ 0x23bb , 'p' } ,
	{ 0x23bc , 'r' } ,
	{ 0x23bd , 's' } ,
	{ 0x2500 , 'q' } ,
	{ 0x2502 , 'x' } ,
	{ 0x250c , 'l' } ,
	{ 0x2510 , 'k' } ,
	{ 0x2514 , 'm' } ,
	{ 0x2518 , 'j' } ,
	{ 0x251c , 't' } ,
	{ 0x2524 , 'u' } ,
	{ 0x252c , 'w' } ,
	{ 0x2534 , 'v' } ,
	{ 0x253c , 'n' } ,

	{ 0x2592 , 'a' } ,
	{ 0x25c6 , '`' } ,
} ;

static area_t *  unicode_noconv_areas ;
static u_int  num_of_unicode_noconv_areas ;

static area_t *  full_width_areas ;
static u_int  num_of_full_width_areas ;

static char *  auto_detect_encodings ;
static struct
{
	ml_char_encoding_t  encoding ;
	mkf_parser_t *  parser ;

} *  auto_detect ;
static u_int  auto_detect_count ;

static int  use_ttyrec_format ;

#ifdef  USE_LIBSSH2
static int  use_scp_full ;
#endif


/* --- static functions --- */

/* XXX This function should be moved to kiklib */
static void
str_replace(
	char *  str ,
	int  c1 ,
	int  c2
	)
{
	while( *str)
	{
		if( *str == c1)
		{
			*str = c2 ;
		}

		str ++ ;
	}
}

/* XXX This function should be moved to mkf */
static u_char
convert_ucs_to_decsp(
	u_int16_t  ucs
	)
{
	int  l_idx ;
	int  h_idx ;
	int  idx ;

	l_idx = 0 ;
	h_idx = sizeof(ucs_to_decsp_table) / sizeof(ucs_to_decsp_table[0]) - 1 ;

	if( ucs < ucs_to_decsp_table[l_idx].ucs ||
	    ucs_to_decsp_table[h_idx].ucs < ucs)
	{
		return  0 ;
	}

	while( 1)
	{
		idx = (l_idx + h_idx) / 2 ;

		if( ucs == ucs_to_decsp_table[idx].ucs)
		{
			return  ucs_to_decsp_table[idx].decsp ;
		}
		else if( ucs < ucs_to_decsp_table[idx].ucs)
		{
			h_idx = idx ;
		}
		else
		{
			l_idx = idx + 1 ;
		}

		if( l_idx >= h_idx)
		{
			return  0 ;
		}
	}
}

/* XXX This function should be moved to mkf */
static u_int16_t
convert_decsp_to_ucs(
	u_char  decsp
	)
{
	if( '`' <= decsp && decsp <= 'x')
	{
		int  count ;

		for( count = 0 ;
		     count < sizeof(ucs_to_decsp_table) / sizeof(ucs_to_decsp_table[0]) ;
		     count ++)
		{
			if( ucs_to_decsp_table[count].decsp == decsp)
			{
				return  ucs_to_decsp_table[count].ucs ;
			}
		}
	}

	return  0 ;
}

static area_t *
set_area_to_table(
	area_t *  area_table ,
	u_int *  num ,
	char *  areas
	)
{
	char *  area ;

	if( areas == NULL || *areas == '\0')
	{
		free( area_table) ;
		area_table = NULL ;

		return  NULL ;
	}
	else
	{
		void *  p ;

		if( ! ( p = realloc( area_table ,
				sizeof(*area_table) *
				(kik_count_char_in_str( areas , ',') + 2))))
		{
			return  area_table ;
		}

		area_table = p ;
	}

	*num = 0 ;

	while( ( area = kik_str_sep( &areas , ",")))
	{
		u_int  min ;
		u_int  max ;

		if( ml_parse_unicode_area( area , &min , &max))
		{
			u_int  count ;

			for( count = 0 ; count < *num ; count++)
			{
				if( area_table[count].min <= min &&
				    area_table[count].max >= max)
				{
					break ;
				}

				if( min <= area_table[count].min &&
				    max >= area_table[count].max)
				{
					area_table[count].min = min ;
					area_table[count].max = max ;

					break ;
				}
			}

			if( count == *num)
			{
				area_table[*num].min = min ;
				area_table[(*num)++].max = max ;
			}
		}
	}

#ifdef  __DEBUG
	{
		u_int  count ;

		for( count = 0 ; count < *num ; count++)
		{
			kik_debug_printf( "AREA %d-%d\n" ,
				area_table[count].min , area_table[count].max) ;
		}
	}
#endif

	return  area_table ;
}

static inline int
is_noconv_unicode(
	u_char *  ch
	)
{
	if( unicode_noconv_areas || ch[2] == 0x20)
	{
		u_int  count ;
		u_int32_t  code ;

		code = mkf_bytes_to_int( ch , 4) ;

		for( count = 0 ; count < num_of_unicode_noconv_areas ; count++)
		{
			if( unicode_noconv_areas[count].min <= code &&
			    code <= unicode_noconv_areas[count].max)
			{
				return  1 ;
			}
		}

		/*
		 * Don't convert these characters in order not to show them.
		 * see ml_char_cols().
		 */
		if( ( 0x200c <= code && code <= 0x200f) ||
		    ( 0x202a <= code && code <= 0x202e))
		{
			return  1 ;
		}
	}

	return  0 ;
}

static inline mkf_property_t
modify_ucs_property(
	u_int32_t  code ,
	mkf_property_t  prop
	)
{
	if( full_width_areas && ! (prop & MKF_FULLWIDTH))
	{
		u_int  count ;

		for( count = 0 ; count < num_of_full_width_areas ; count++)
		{
			if( full_width_areas[count].min <= code &&
			    code <= full_width_areas[count].max)
			{
				return  (prop & ~MKF_AWIDTH) | MKF_FULLWIDTH ;
			}
		}
	}

	return  prop ;
}

static inline mkf_property_t
get_ucs_property(
	u_int32_t  code
	)
{
	return  modify_ucs_property( code , mkf_get_ucs_property( code)) ;
}

static void
start_vt100_cmd(
	ml_vt100_parser_t *  vt100_parser ,
	int  trigger_xterm_event	/* dispatch to x_screen or not. */
	)
{
	ml_set_use_multi_col_char( vt100_parser->use_multi_col_char) ;

	if( trigger_xterm_event && HAS_XTERM_LISTENER(vt100_parser,start))
	{
		/*
		 * XXX Adhoc implementation.
		 * Converting visual -> logical in xterm_listener->start.
		 */
		(*vt100_parser->xterm_listener->start)( vt100_parser->xterm_listener->self) ;
	}
	else
	{
		ml_screen_logical( vt100_parser->screen) ;
	}
}

static void
stop_vt100_cmd(
	ml_vt100_parser_t *  vt100_parser ,
	int  trigger_xterm_event	/* dispatch to x_screen or not. */
	)
{
	ml_screen_render( vt100_parser->screen) ;
	ml_screen_visual( vt100_parser->screen) ;

	if( trigger_xterm_event && HAS_XTERM_LISTENER(vt100_parser,stop))
	{
		(*vt100_parser->xterm_listener->stop)( vt100_parser->xterm_listener->self) ;
	}
}

static void
interrupt_vt100_cmd(
	ml_vt100_parser_t *  vt100_parser
	)
{
	if( HAS_XTERM_LISTENER(vt100_parser,interrupt))
	{
		ml_screen_render( vt100_parser->screen) ;
		ml_screen_visual( vt100_parser->screen) ;

		(*vt100_parser->xterm_listener->interrupt)( vt100_parser->xterm_listener->self) ;

		ml_screen_logical( vt100_parser->screen) ;
	}
}

static int
change_read_buffer_size(
	ml_read_buffer_t *  r_buf ,
	size_t  len
	)
{
	void *  p ;

	if( ! ( p = realloc( r_buf->chars , len)))
	{
		return  0 ;
	}

	r_buf->chars = p ;
	r_buf->len = len ;

	/*
	 * Not check if r_buf->left and r_buf->filled_len is larger than r_buf->len.
	 * It should be checked before calling this function.
	 */

	return  1 ;
}

static char *
get_home_file_path(
	const char *  prefix ,
	const char *  name ,
	const char *  suffix
	)
{
	char *  file_name ;

	if( ! ( file_name = alloca( 7 + strlen( prefix) + 1 + strlen( name) + 1 +
				strlen( suffix) + 1)))
	{
		return  NULL ;
	}

	sprintf( file_name , "mlterm/%s%s.%s" , prefix , name , suffix) ;
	str_replace( file_name + 7 , '/' , '_') ;

	return  kik_get_user_rc_path( file_name) ;
}

/*
 * 0: Error
 * 1: No error
 * >=2: Probable
 */
static int
parse_string(
	mkf_parser_t *  cc_parser ,
	u_char *  str ,
	size_t  len
	)
{
	mkf_char_t  ch ;
	int  ret ;
	u_int  nfull ;
	u_int  nkana ;

	ret = 1 ;
	nfull = 0 ;
	nkana = 0 ;
	(*cc_parser->init)( cc_parser) ;
	(*cc_parser->set_str)( cc_parser , str , len) ;

	while(1)
	{
		if( ! (*cc_parser->next_char)( cc_parser , &ch))
		{
			if( cc_parser->is_eos)
			{
				if( nkana * 8 > nfull)
				{
					/* kana is over 12.5%. */
					ret = 2 ;
				}

				return  ret ;
			}
			else
			{
				if( ((str[len - cc_parser->left]) & 0x7f) <= 0x1f)
				{
					/* skip C0 or C1 */
					mkf_parser_increment( cc_parser) ;
				}
				else
				{
					return  0 ;
				}
			}
		}
		else if( ch.size > 1)
		{
			if( ch.cs == ISO10646_UCS4_1)
			{
				if( ret == 1 && ch.property == MKF_FULLWIDTH)
				{
					ret = 2 ;
				}
			}
			else
			{
				if( IS_CS94MB(ch.cs))
				{
					if( ch.ch[0] <= 0x20 || ch.ch[0] == 0x7f ||
					    ch.ch[1] <= 0x20 || ch.ch[1] == 0x7f)
					{
						/* mkf can return illegal character code. */
						return  0 ;
					}
					else if( ret == 1 &&
						 ( ch.cs == JISX0208_1983 ||
						   ch.cs == JISC6226_1978 ||
						   ch.cs == JISX0213_2000_1) &&
						 ( ch.ch[0] == 0x24 || ch.ch[0] == 0x25) &&
						 0x21 <= ch.ch[1] && ch.ch[1] <= 0x73)
					{
						/* Hiragana/Katakana */
						nkana ++ ;
					}
				}

				nfull ++ ;
			}
		}
	}
}

/* Check auto_detect_count > 0 before calling this function. */
static void
detect_encoding(
	ml_vt100_parser_t *  vt100_parser
	)
{
	u_char *  str ;
	size_t  len ;
	size_t  count ;
	u_int  idx ;
	int  cur_idx ;
	int  cand_idx ;
	int  threshold ;

	str = vt100_parser->r_buf.chars ;
	len = vt100_parser->r_buf.filled_len ;

	for( count = 0 ; count < len - 1 ; count++)
	{
		if( str[count] >= 0x80 && str[count + 1] >= 0x80)
		{
			goto  detect ;
		}
	}

	return ;

detect:
	cur_idx = -1 ;
	threshold = 0 ;
	for( idx = 0 ; idx < auto_detect_count ; idx++)
	{
		if( auto_detect[idx].encoding == vt100_parser->encoding)
		{
			if( ( threshold = parse_string( auto_detect[idx].parser , str , len)) > 1)
			{
				return ;
			}

			cur_idx = idx ;
			break ;
		}
	}

	cand_idx = -1 ;
	for( idx = 0 ; idx < auto_detect_count ; idx++)
	{
		int  ret ;

		if( idx != cur_idx &&
		    ( ret = parse_string( auto_detect[idx].parser , str , len)) > threshold)
		{
			cand_idx = idx ;

			if( ret > 1)
			{
				break ;
			}
		}
	}

	if( cand_idx >= 0)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG
			" Character encoding is changed to %s.\n" ,
			ml_get_char_encoding_name( auto_detect[cand_idx].encoding)) ;
	#endif

		ml_vt100_parser_change_encoding( vt100_parser ,
			auto_detect[cand_idx].encoding) ;
	}
}

inline static int
is_dcs_or_osc(
	u_char *  str	/* The length should be 2 or more. */
	)
{
	return  *str == 0x90 ||
	        memcmp( str , "\x1bP" , 2) == 0 ||
	        memcmp( str , "\x1b]" , 2) == 0 ;
}

static void
write_ttyrec_header(
	int  fd ,
	size_t  len ,
	int  keep_time
	)
{
	u_int32_t  buf[3] ;

	if( ! keep_time)
	{
		struct timeval  tval ;

		gettimeofday( &tval , NULL) ;
	#ifdef  WORDS_BIGENDIAN
		buf[0] = LE32DEC(((u_char*)&tval.tv_sec)) ;
		buf[1] = LE32DEC(((u_char*)&tval.tv_usec)) ;
		buf[2] = LE32DEC(((u_char*)&len)) ;
	#else
		buf[0] = tval.tv_sec ;
		buf[1] = tval.tv_usec ;
		buf[2] = len ;
	#endif

	#if  __DEBUG
		kik_debug_printf( "write len %d at %d\n" ,
			len , lseek( fd , 0 , SEEK_CUR)) ;
	#endif

		write( fd , buf , 12) ;
	}
	else
	{
		lseek( fd , 8 , SEEK_CUR) ;

	#ifdef  WORDS_BIGENDIAN
		buf[0] = LE32DEC(((u_char*)&len)) ;
	#else
		buf[0] = len ;
	#endif

		write( fd , buf , 4) ;
	}
}

static int
receive_bytes(
	ml_vt100_parser_t *  vt100_parser
	)
{
	size_t  len ;

	if( vt100_parser->r_buf.left == vt100_parser->r_buf.len)
	{
		/* Buffer is full => Expand buffer */

		len = vt100_parser->r_buf.len >= PTY_RD_BUFFER_SIZE * MAX_READ_COUNT ?
			 PTY_RD_BUFFER_SIZE * 10 : PTY_RD_BUFFER_SIZE ;

		if( ! change_read_buffer_size( &vt100_parser->r_buf ,
			vt100_parser->r_buf.len + len))
		{
			return  0 ;
		}
	}
	else
	{
		if( 0 < vt100_parser->r_buf.left &&
		    vt100_parser->r_buf.left < vt100_parser->r_buf.filled_len)
		{
			memmove( vt100_parser->r_buf.chars , CURRENT_STR_P(vt100_parser) ,
				vt100_parser->r_buf.left * sizeof( u_char)) ;
		}

		/* vt100_parser->r_buf.left must be always less than vt100_parser->r_buf.len */
		if( ( len = vt100_parser->r_buf.len - vt100_parser->r_buf.left) >
			PTY_RD_BUFFER_SIZE &&
		    ! is_dcs_or_osc( vt100_parser->r_buf.chars))
		{
			len = PTY_RD_BUFFER_SIZE ;
		}
	}

	if( ( vt100_parser->r_buf.new_len =
		ml_read_pty( vt100_parser->pty ,
			vt100_parser->r_buf.chars + vt100_parser->r_buf.left , len)) == 0)
	{
		vt100_parser->r_buf.filled_len = vt100_parser->r_buf.left ;
	
		return  0 ;
	}

	if( vt100_parser->logging_vt_seq)
	{
		if( vt100_parser->log_file == -1)
		{
			char *  path ;

			if( ! ( path = get_home_file_path( "" ,
						ml_pty_get_slave_name( vt100_parser->pty) + 5 ,
						"log")))
			{
				goto  end ;
			}


			if( ( vt100_parser->log_file =
				open( path , O_CREAT | O_WRONLY , 0600)) == -1)
			{
				free( path) ;

				goto  end ;
			}

			free( path) ;

			/*
			 * O_APPEND in open() forces lseek(0,SEEK_END) in write()
			 * and disables lseek(pos,SEEK_SET) before calling write().
			 * So don't specify O_APPEND in open() and call lseek(0,SEEK_END)
			 * manually after open().
			 */
			lseek( vt100_parser->log_file , 0 , SEEK_END) ;
			kik_file_set_cloexec( vt100_parser->log_file) ;

			if( use_ttyrec_format)
			{
				char  seq[6 + DIGIT_STR_LEN(int) * 2 + 1] ;

				sprintf( seq , "\x1b[8;%d;%dt" ,
					ml_screen_get_rows( vt100_parser->screen) ,
					ml_screen_get_cols( vt100_parser->screen)) ;
				write_ttyrec_header( vt100_parser->log_file ,
					strlen( seq) , 0) ;
				write( vt100_parser->log_file , seq , strlen(seq)) ;
			}
		}

		if( use_ttyrec_format)
		{
			if( vt100_parser->r_buf.left > 0)
			{
				lseek( vt100_parser->log_file ,
					lseek( vt100_parser->log_file , 0 , SEEK_CUR) -
						vt100_parser->r_buf.filled_len - 12 ,
					SEEK_SET) ;

				if( vt100_parser->r_buf.left < vt100_parser->r_buf.filled_len)
				{
					write_ttyrec_header( vt100_parser->log_file ,
						vt100_parser->r_buf.filled_len -
						vt100_parser->r_buf.left , 1) ;

					lseek( vt100_parser->log_file ,
						lseek( vt100_parser->log_file , 0 , SEEK_CUR) +
							vt100_parser->r_buf.filled_len -
							vt100_parser->r_buf.left ,
						SEEK_SET) ;
				}
			}

			write_ttyrec_header( vt100_parser->log_file ,
				vt100_parser->r_buf.left + vt100_parser->r_buf.new_len , 0) ;

			write( vt100_parser->log_file ,
				vt100_parser->r_buf.chars ,
				vt100_parser->r_buf.left + vt100_parser->r_buf.new_len) ;
		}
		else
		{
			write( vt100_parser->log_file ,
				vt100_parser->r_buf.chars + vt100_parser->r_buf.left ,
				vt100_parser->r_buf.new_len) ;
		}

	#ifndef  USE_WIN32API
		fsync( vt100_parser->log_file) ;
	#endif
	}
	else
	{
		if ( vt100_parser->log_file != -1)
		{
			close( vt100_parser->log_file) ;
			vt100_parser->log_file = -1 ;
		}
        }

end:
	vt100_parser->r_buf.filled_len =
		( vt100_parser->r_buf.left += vt100_parser->r_buf.new_len) ;

	if( vt100_parser->r_buf.filled_len <= PTY_RD_BUFFER_SIZE)
	{
		/* Shrink buffer */
		change_read_buffer_size( &vt100_parser->r_buf , PTY_RD_BUFFER_SIZE) ;
	}

	if( vt100_parser->use_auto_detect && auto_detect_count > 0)
	{
		detect_encoding( vt100_parser) ;
	}

#ifdef  INPUT_DEBUG
	{
		size_t  count ;

		kik_debug_printf( KIK_DEBUG_TAG " pty msg (len %d) is received:" ,
			vt100_parser->r_buf.left) ;

		for( count = 0 ; count < vt100_parser->r_buf.left ; count ++)
		{
		#ifdef  DUMP_HEX
			if( isprint( vt100_parser->r_buf.chars[count]))
			{
				kik_msg_printf( "%c " , vt100_parser->r_buf.chars[count]) ;
			}
			else
			{
				kik_msg_printf( "%.2x " , vt100_parser->r_buf.chars[count]) ;
			}
		#else
			kik_msg_printf( "%c" , vt100_parser->r_buf.chars[count]) ;
		#endif
		}

		kik_msg_printf( "[END]\n") ;
	}
#endif

	return  1 ;
}

/*
 * If buffer exists, vt100_parser->w_buf.last_ch is cached.
 * If buffer doesn't exist, vt100_parser->w_buf.last_ch is cleared.
 */
static int
flush_buffer(
	ml_vt100_parser_t *  vt100_parser
	)
{
	ml_write_buffer_t *  buffer ;

	buffer = &vt100_parser->w_buf ;

	if( buffer->filled_len == 0)
	{
		/* last_ch is cleared. */
		buffer->last_ch = NULL ;

		return  0 ;
	}
	
#ifdef  OUTPUT_DEBUG
	{
		u_int  count ;

		kik_msg_printf( "\nflushing chars(%d)...==>" , buffer->filled_len) ;
		for( count = 0 ; count < buffer->filled_len ; count ++)
		{
			char *  bytes ;

			bytes = ml_char_code( &buffer->chars[count]) ;

			if( ml_char_size( &buffer->chars[count]) == 2)
			{
			#ifdef  DUMP_HEX
				kik_msg_printf( "%x%x" , bytes[0] | 0x80 , bytes[1] | 0x80) ;
			#else
				kik_msg_printf( "%c%c" , bytes[0] | 0x80 , bytes[1] | 0x80) ;
			#endif
			}
			else
			{
			#ifdef  DUMP_HEX
				kik_msg_printf( "%x" , bytes[0]) ;
			#else
				kik_msg_printf( "%c" , bytes[0]) ;
			#endif
			}
		}

		kik_msg_printf( "<===\n") ;
	}
#endif

	(*buffer->output_func)( vt100_parser->screen , buffer->chars , buffer->filled_len) ;

	/* last_ch which will be used & cleared in REP sequence is cached. */
	buffer->last_ch = &buffer->chars[buffer->filled_len - 1] ;
	/* buffer is cleared. */
	buffer->filled_len = 0 ;

#ifdef  EDIT_DEBUG
	ml_edit_dump( vt100_parser->screen->edit) ;
#endif

	return  1 ;
}

static void
put_char(
	ml_vt100_parser_t *  vt100_parser ,
	u_int32_t  ch ,
	mkf_charset_t  cs ,
	mkf_property_t  prop
	)
{
	ml_color_t  fg_color ;
	ml_color_t  bg_color ;
	int  is_fullwidth ;
	int  is_comb ;
	int  is_bold ;
	int  is_italic ;
	int  underline_style ;
	int  is_crossed_out ;
	int  is_blinking ;

	if( vt100_parser->w_buf.filled_len == PTY_WR_BUFFER_SIZE)
	{
		flush_buffer( vt100_parser) ;
	}

	/*
	 * checking width property of the char.
	 */

	is_fullwidth = 0 ;

	if( prop & MKF_FULLWIDTH)
	{
		is_fullwidth = 1 ;
	}
	else if( prop & MKF_AWIDTH)
	{
	#ifdef  SUPPORT_VTE_CJK_WIDTH
		char *  env ;
	#endif

		if( vt100_parser->col_size_of_width_a == 2)
		{
			is_fullwidth = 1 ;
		}
	#ifdef  SUPPORT_VTE_CJK_WIDTH
		else if( ( env = getenv( "VTE_CJK_WIDTH")) &&
		         ( strcmp( env , "wide") == 0 || strcmp( env , "1") == 0))
		{
			is_fullwidth = 1 ;
		}
	#endif
	}

#ifdef  __DEBUG
	kik_debug_printf( "%x %d %x => %s\n" , ch , len , cs ,
		is_fullwidth ? "Fullwidth" : "Single") ;
#endif

	if( ( prop & MKF_COMBINING) &&
	    ( vt100_parser->use_char_combining
	#if  ! defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND)
	      || ( ch == '\xe9' && IS_ISCII(cs)) /* nukta is always combined. */
	#endif
	      ))
	{
		is_comb = 1 ;
	}
	else
	{
		is_comb = 0 ;
	}

	if( vt100_parser->cs != cs)
	{
		vt100_parser->cs = cs ;
	}

	fg_color = vt100_parser->fg_color ;
	is_italic = vt100_parser->is_italic ;
	is_crossed_out = vt100_parser->is_crossed_out ;
	is_blinking = vt100_parser->is_blinking ;
	underline_style = vt100_parser->underline_style ;
	if( cs == ISO10646_UCS4_1 && 0x2580 <= ch && ch <= 0x259f)
	{
		/* prevent these block characters from being drawn doubly. */
		is_bold = 0 ;
	}
	else
	{
		is_bold = vt100_parser->is_bold ;
	}

	if( fg_color == ML_FG_COLOR)
	{
		if( is_italic && (vt100_parser->alt_color_mode & ALT_COLOR_ITALIC))
		{
			is_italic = 0 ;
			fg_color = ML_ITALIC_COLOR ;
		}

		if( is_crossed_out && (vt100_parser->alt_color_mode & ALT_COLOR_CROSSED_OUT))
		{
			is_crossed_out = 0 ;
			fg_color = ML_CROSSED_OUT_COLOR ;
		}

		if( is_blinking && (vt100_parser->alt_color_mode & ALT_COLOR_BLINKING))
		{
			is_blinking = 0 ;
			fg_color = ML_BLINKING_COLOR ;
		}

		if( underline_style && (vt100_parser->alt_color_mode & ALT_COLOR_UNDERLINE))
		{
			underline_style = UNDERLINE_NONE ;
			fg_color = ML_UNDERLINE_COLOR ;
		}
	}

	if( is_bold)
	{
		if( ( vt100_parser->alt_color_mode & ALT_COLOR_BOLD) &&
		    vt100_parser->fg_color == ML_FG_COLOR)
		{
			is_bold = 0 ;
			fg_color = ML_BOLD_COLOR ;
		}
		else if( IS_VTSYS_BASE_COLOR(fg_color))
		{
			fg_color |= ML_BOLD_COLOR_MASK ;
		}
	}

	if( vt100_parser->is_reversed)
	{
		bg_color = fg_color ;
		fg_color = vt100_parser->bg_color ;
	}
	else
	{
		bg_color = vt100_parser->bg_color ;
	}

	if( ! vt100_parser->screen->use_dynamic_comb && is_comb)
	{
		if( vt100_parser->w_buf.filled_len == 0)
		{
			/*
			 * ml_line_set_modified() is done in ml_screen_combine_with_prev_char()
			 * internally.
			 */
			if( ml_screen_combine_with_prev_char( vt100_parser->screen ,
				ch , cs , is_fullwidth , is_comb ,
				fg_color , bg_color , is_bold , is_italic ,
				underline_style , is_crossed_out , is_blinking))
			{
				return ;
			}
		}
		else
		{
			if( ml_char_combine(
				&vt100_parser->w_buf.chars[vt100_parser->w_buf.filled_len - 1] ,
				ch , cs , is_fullwidth , is_comb ,
				fg_color , bg_color , is_bold , is_italic ,
				underline_style , is_crossed_out , is_blinking))
			{
				return ;
			}
		}

		/*
		 * if combining failed , char is normally appended.
		 */
	}

	ml_char_set( &vt100_parser->w_buf.chars[vt100_parser->w_buf.filled_len++] , ch ,
		cs , is_fullwidth , is_comb ,
		fg_color , bg_color , is_bold , is_italic ,
		underline_style , is_crossed_out , is_blinking) ;

	if( ! vt100_parser->screen->use_dynamic_comb && cs == ISO10646_UCS4_1)
	{
		/*
		 * Arabic combining
		 */

		ml_char_t *  prev2 ;
		ml_char_t *  prev ;
		ml_char_t *  cur ;
		int  n ;

		cur = &vt100_parser->w_buf.chars[vt100_parser->w_buf.filled_len - 1] ;
		n = 0 ;

		if( vt100_parser->w_buf.filled_len >= 2)
		{
			prev = cur - 1 ;
		}
		else
		{
			if( ! ( prev = ml_screen_get_n_prev_char( vt100_parser->screen , ++n)))
			{
				return ;
			}
		}
		
		if( vt100_parser->w_buf.filled_len >= 3)
		{
			prev2 = cur - 2  ;
		}
		else
		{
			/* possibly NULL */
			prev2 = ml_screen_get_n_prev_char( vt100_parser->screen , ++n) ;
		}
		
		if( vt100_parser->use_bidi && ml_is_arabic_combining( prev2 , prev , cur))
		{
			if( vt100_parser->w_buf.filled_len >= 2)
			{
				if( ml_char_combine( prev ,
					ch , cs , is_fullwidth , is_comb ,
					fg_color , bg_color , is_bold ,
					vt100_parser->is_italic , vt100_parser->underline_style ,
					vt100_parser->is_crossed_out , vt100_parser->is_blinking))
				{
					vt100_parser->w_buf.filled_len -- ;
				}
			}
			else
			{
				/*
				 * ml_line_set_modified() is done in
				 * ml_screen_combine_with_prev_char() internally.
				 */
				if( ml_screen_combine_with_prev_char( vt100_parser->screen ,
					ch , cs , is_fullwidth , is_comb ,
					fg_color , bg_color , is_bold ,
					vt100_parser->is_italic , vt100_parser->underline_style ,
					vt100_parser->is_crossed_out , vt100_parser->is_blinking))
				{
					vt100_parser->w_buf.filled_len -- ;
				}
			}
		}
	}
}

static void
push_to_saved_names(
	ml_vt100_saved_names_t *  saved ,
	char *  name
	)
{
	void *  p ;

	if( ! ( p = realloc( saved->names , (saved->num + 1) * sizeof(name))))
	{
		return ;
	}

	saved->names = p ;
	saved->names[saved->num ++] = name ? strdup( name) : NULL ;
}

static char *
pop_from_saved_names(
	ml_vt100_saved_names_t *  saved
	)
{
	char *  name ;

	name = saved->names[--saved->num] ;

	if( saved->num == 0)
	{
		free( saved->names) ;
		saved->names = NULL ;
	}

	return  name ;
}


/*
 * VT100_PARSER Escape Sequence Commands.
 */

static void
save_cursor(
	ml_vt100_parser_t *  vt100_parser
	)
{
	ml_vt100_storable_states_t *  dest ;

	dest = (ml_screen_is_alternative_edit( vt100_parser->screen)) ?
		&(vt100_parser->saved_alternate)
		: &(vt100_parser->saved_normal) ;
	dest->is_saved = 1 ;
	dest->fg_color = vt100_parser->fg_color ;
	dest->bg_color = vt100_parser->bg_color ;
	dest->is_bold  = vt100_parser->is_bold ;
	dest->is_italic = vt100_parser->is_italic ;
	dest->underline_style = vt100_parser->underline_style ;
	dest->is_reversed = vt100_parser->is_reversed ;
	dest->is_crossed_out = vt100_parser->is_crossed_out ;
	dest->is_blinking = vt100_parser->is_blinking ;
	dest->cs = vt100_parser->cs ;

	ml_screen_save_cursor( vt100_parser->screen) ;
}

static void
restore_cursor(
	ml_vt100_parser_t *  vt100_parser
	)
{
        ml_vt100_storable_states_t *src;

        src = (ml_screen_is_alternative_edit(  vt100_parser->screen) ) ?
                &(vt100_parser->saved_alternate)
                : &(vt100_parser->saved_normal) ;
	if( src->is_saved)
	{
		vt100_parser->fg_color = src->fg_color ;
		ml_screen_set_bce_fg_color( vt100_parser->screen , src->fg_color) ;
		vt100_parser->bg_color = src->bg_color ;
		ml_screen_set_bce_bg_color( vt100_parser->screen , src->bg_color) ;
		vt100_parser->is_bold = src->is_bold ;
		vt100_parser->is_italic = src->is_italic ;
		vt100_parser->underline_style = src->underline_style ;
		vt100_parser->is_reversed = src->is_reversed ;
		vt100_parser->is_crossed_out = src->is_crossed_out ;
		vt100_parser->is_blinking = src->is_blinking ;
		if( IS_ENCODING_BASED_ON_ISO2022(vt100_parser->encoding))
		{
			if( ( src->cs == DEC_SPECIAL)
			 && ( src->cs != vt100_parser->cs) )
			{
				/* force grapchics mode by sending \E(0 to current parser*/
				u_char  DEC_SEQ[] = { CTL_ESC, '(', '0'} ;
				mkf_char_t  ch ;
				mkf_parser_t *  parser;
					
				ml_init_encoding_parser( vt100_parser) ;
				parser = vt100_parser->cc_parser;
				(*parser->set_str)( parser, DEC_SEQ, sizeof(DEC_SEQ)) ;
				(*parser->next_char)( parser, &ch) ;
			}
		}
		else
		{
			/* XXX: what to do for g0/g1? */
			if( src->cs == DEC_SPECIAL)
			{
				vt100_parser->gl = DEC_SPECIAL ;
			}
			else
			{
				vt100_parser->gl = US_ASCII ;
			}
		}
	}
	ml_screen_restore_cursor( vt100_parser->screen) ;
}

static void
resize(
	ml_vt100_parser_t *  vt100_parser ,
	u_int  width ,
	u_int  height ,
	int  by_char
	)
{
	if( HAS_XTERM_LISTENER(vt100_parser,resize))
	{
		if( by_char)
		{
			/*
			 * XXX Not compatible with xterm.
			 * width(cols) or height(rows) == 0 means full screen width
			 * or height in xterm.
			 * Note that ml_vt100_parser.c depends on following behavior.
			 */
			if( width == 0)
			{
				width = ml_screen_get_cols( vt100_parser->screen) ;
			}

			if( height == 0)
			{
				height = ml_screen_get_rows( vt100_parser->screen) ;
			}

			ml_screen_resize( vt100_parser->screen , width , height) ;

			/*
			 * xterm_listener::resize(0,0) means that screen should be
			 * resized according to the size of pty.
			 */
			width = 0 ;
			height = 0 ;
		}

		stop_vt100_cmd( vt100_parser , 0) ;
		(*vt100_parser->xterm_listener->resize)(
			vt100_parser->xterm_listener->self , width , height) ;
		start_vt100_cmd( vt100_parser , 0) ;
	}
}

static void
reverse_video(
	ml_vt100_parser_t *  vt100_parser ,
	int  flag
	)
{
	if( HAS_XTERM_LISTENER(vt100_parser,reverse_video))
	{
		stop_vt100_cmd( vt100_parser , 0) ;
		(*vt100_parser->xterm_listener->reverse_video)(
			vt100_parser->xterm_listener->self , flag) ;
		start_vt100_cmd( vt100_parser , 0) ;
	}
}

static void
set_mouse_report(
	ml_vt100_parser_t *  vt100_parser ,
	ml_mouse_report_mode_t  mode
	)
{
	if( HAS_XTERM_LISTENER(vt100_parser,set_mouse_report))
	{
		stop_vt100_cmd( vt100_parser , 0) ;

		vt100_parser->mouse_mode = mode ;
		(*vt100_parser->xterm_listener->set_mouse_report)(
			vt100_parser->xterm_listener->self) ;

		start_vt100_cmd( vt100_parser , 0) ;
	}
}

static void
request_locator(
	ml_vt100_parser_t *  vt100_parser
	)
{
	if( HAS_XTERM_LISTENER(vt100_parser,request_locator))
	{
		ml_mouse_report_mode_t  orig ;

		stop_vt100_cmd( vt100_parser , 0) ;

		if( vt100_parser->mouse_mode < LOCATOR_CHARCELL_REPORT)
		{
			orig = vt100_parser->mouse_mode ;
			vt100_parser->mouse_mode = LOCATOR_CHARCELL_REPORT ;
		}
		else
		{
			orig = 0 ;
		}

		vt100_parser->locator_mode |= LOCATOR_REQUEST ;

		(*vt100_parser->xterm_listener->request_locator)(
			vt100_parser->xterm_listener->self) ;

		vt100_parser->locator_mode |= LOCATOR_REQUEST ;

		if( orig)
		{
			orig = vt100_parser->mouse_mode ;
		}

		start_vt100_cmd( vt100_parser , 0) ;
	}
}

static void
set_window_name(
	ml_vt100_parser_t *  vt100_parser ,
	u_char *  name		/* should be malloc'ed or NULL. */
	)
{
	free( vt100_parser->win_name) ;
	vt100_parser->win_name = name ;

	if( HAS_XTERM_LISTENER(vt100_parser,set_window_name))
	{
	#if  0
		stop_vt100_cmd( vt100_parser , 0) ;
	#endif
		(*vt100_parser->xterm_listener->set_window_name)(
			vt100_parser->xterm_listener->self , name) ;
	#if  0
		start_vt100_cmd( vt100_parser , 0) ;
	#endif
	}
}

static void
set_icon_name(
	ml_vt100_parser_t *  vt100_parser ,
	u_char *  name		/* should be malloc'ed or NULL. */
	)
{
	free( vt100_parser->icon_name) ;
	vt100_parser->icon_name = name ;

	if( HAS_XTERM_LISTENER(vt100_parser,set_icon_name))
	{
	#if  0
		stop_vt100_cmd( vt100_parser , 0) ;
	#endif
		(*vt100_parser->xterm_listener->set_icon_name)(
			vt100_parser->xterm_listener->self , name) ;
	#if  0
		start_vt100_cmd( vt100_parser , 0) ;
	#endif
	}
}

static void
switch_im_mode(
	ml_vt100_parser_t *  vt100_parser
	)
{
	if( HAS_XTERM_LISTENER(vt100_parser,switch_im_mode))
	{
	#if  0
		stop_vt100_cmd( vt100_parser , 0) ;
	#endif
		(*vt100_parser->xterm_listener->switch_im_mode)(
					vt100_parser->xterm_listener->self) ;
	#if  0
		start_vt100_cmd( vt100_parser , 0) ;
	#endif
	}
}

static int
im_is_active(
	ml_vt100_parser_t *  vt100_parser
	)
{
	if( HAS_XTERM_LISTENER(vt100_parser,im_is_active))
	{
		return  (*vt100_parser->xterm_listener->im_is_active)(
					vt100_parser->xterm_listener->self) ;
	}
	else
	{
		return  0 ;
	}
}

static void
set_modkey_mode(
	ml_vt100_parser_t *  vt100_parser ,
	int  key ,
	int  mode
	)
{
#if  0
	if( key == 1 && mode <= 3)
	{
		vt100_parser->modify_cursor_keys = mode ;
	}
	else if( key == 2 && mode <= 3)
	{
		vt100_parser->modify_function_keys = mode ;
	}
	else
#endif
	if( key == 4 && mode <= 2)
	{
		vt100_parser->modify_other_keys = mode ;
	}
}

static void
report_window_size(
	ml_vt100_parser_t *  vt100_parser ,
	int  by_char
	)
{
	if( HAS_XTERM_LISTENER(vt100_parser,get_window_size))
	{
		int  ps ;
		u_int  width ;
		u_int  height ;
		char seq[ 5 + 1 /* ps */ + DIGIT_STR_LEN(u_int) * 2 + 1] ;

		if( by_char)
		{
			width = ml_screen_get_logical_cols( vt100_parser->screen) ;
			height = ml_screen_get_logical_rows( vt100_parser->screen) ;
			ps = 8 ;
		}
		else
		{
			if( ! (*vt100_parser->xterm_listener->get_window_size)(
					vt100_parser->xterm_listener->self , &width , &height))
			{
				return ;
			}

			ps = 4 ;
		}

		sprintf( seq , "\x1b[%d;%d;%dt" , ps , height , width) ;
		ml_write_to_pty( vt100_parser->pty , seq , strlen(seq)) ;
	}
}

#ifndef  NO_IMAGE
static int
cursor_char_is_picture_and_modified(
	ml_screen_t *  screen
	)
{
	ml_line_t *  line ;
	ml_char_t *  ch ;

	if( ( line = ml_screen_get_cursor_line( screen)) &&
	    ml_line_is_modified( line) &&
	    ( ch = ml_char_at( line , ml_screen_cursor_char_index( screen))) &&
	    ml_get_picture_char( ch))
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

/* Don't call this if vt100_parser->sixel_scrolling is true. */
static int
check_sixel_anim(
	ml_screen_t *  screen ,
	u_char *  str ,
	size_t  left
	)
{
	ml_line_t *  line ;
	ml_char_t *  ch ;

	if( ( line = ml_screen_get_line( screen , 0)) &&
	    ( ch = ml_char_at( line , 0)) &&
	    ml_get_picture_char( ch))
	{
		while( --left > 0)
		{
			if( *(++str) == '\x1b')
			{
				if( --left == 0)
				{
					break ;
				}

				if( *(++str) == 'P')
				{
					/* It seems animation sixel. */
					return  1 ;
				}
			}
		}
	}

	return  0 ;
}

static void
show_picture(
	ml_vt100_parser_t *  vt100_parser ,
	char *  file_path ,
	int  clip_beg_col ,
	int  clip_beg_row ,
	int  clip_cols ,
	int  clip_rows ,
	int  img_cols ,
	int  img_rows ,
	int  is_sixel	/* 0: not sixel, 1: sixel, 2: sixel anim, 3: macro */
	)
{
#ifndef  DONT_OPTIMIZE_DRAWING_PICTURE
	if( is_sixel == 2)
	{
		(*vt100_parser->xterm_listener->show_sixel)(
			vt100_parser->xterm_listener->self , file_path) ;

		vt100_parser->yield = 1 ;

		return ;
	}
#endif	/* DONT_OPTIMIZE_DRAWING_PICTURE */

	if( HAS_XTERM_LISTENER(vt100_parser,get_picture_data))
	{
		ml_char_t *  data ;

		if( ( data = (*vt100_parser->xterm_listener->get_picture_data)(
				vt100_parser->xterm_listener->self ,
				file_path , &img_cols , &img_rows)) &&
		    clip_beg_row < img_rows && clip_beg_col < img_cols)
		{
			ml_char_t *  p ;
			int  row ;
			int  cursor_col ;
			int  orig_auto_wrap ;

			if( clip_cols == 0)
			{
				clip_cols = img_cols - clip_beg_col ;
			}

			if( clip_rows == 0)
			{
				clip_rows = img_rows - clip_beg_row ;
			}

			if( clip_beg_row + clip_rows > img_rows)
			{
				clip_rows = img_rows - clip_beg_row ;
			}

			if( clip_beg_col + clip_cols > img_cols)
			{
				clip_cols = img_cols - clip_beg_col ;
			}

			p = data + (img_cols * clip_beg_row) ;
			row = 0 ;

			if( is_sixel && ! vt100_parser->sixel_scrolling)
			{
				ml_screen_save_cursor( vt100_parser->screen) ;
				ml_screen_cursor_invisible( vt100_parser->screen) ;
				ml_screen_go_upward( vt100_parser->screen ,
					ml_screen_cursor_row( vt100_parser->screen)) ;
				ml_screen_goto_beg_of_line( vt100_parser->screen) ;
			}

			if( cursor_char_is_picture_and_modified( vt100_parser->screen))
			{
				/* Perhaps it is animation. */
				interrupt_vt100_cmd( vt100_parser) ;
				vt100_parser->yield = 1 ;
			}

			orig_auto_wrap = ml_screen_is_auto_wrap( vt100_parser->screen) ;
			ml_screen_set_auto_wrap( vt100_parser->screen , 0) ;
			cursor_col = ml_screen_cursor_col( vt100_parser->screen) ;

			while( 1)
			{
				ml_screen_overwrite_chars( vt100_parser->screen ,
					p + clip_beg_col , clip_cols) ;

				if( ++row >= clip_rows)
				{
					break ;
				}

				if( is_sixel && ! vt100_parser->sixel_scrolling)
				{
					if( ! ml_screen_go_downward( vt100_parser->screen , 1))
					{
						break ;
					}
				}
				else
				{
					ml_screen_line_feed( vt100_parser->screen) ;
				}

				ml_screen_go_horizontally( vt100_parser->screen , cursor_col) ;

				p += img_cols ;
			}

			if( is_sixel)
			{
				if( vt100_parser->sixel_scrolling)
				{
					ml_screen_line_feed( vt100_parser->screen) ;
					ml_screen_go_horizontally( vt100_parser->screen ,
						cursor_col) ;
				}
				else
				{
					ml_screen_restore_cursor( vt100_parser->screen) ;
					ml_screen_cursor_visible( vt100_parser->screen) ;
				}
			}

			ml_str_delete( data , img_cols * img_rows) ;

			ml_screen_set_auto_wrap( vt100_parser->screen , orig_auto_wrap) ;

			if( strstr( file_path , "://"))
			{
				/* Showing remote image is very heavy. */
				vt100_parser->yield = 1 ;
			}
		}
	}
}
#endif

static void
snapshot(
	ml_vt100_parser_t *  vt100_parser ,
	ml_char_encoding_t  encoding ,
	char *  file_name
	)
{
	int  beg ;
	int  end ;
	ml_char_t *  buf ;
	u_int  num ;
	char *  path ;
	FILE *  file ;
	u_char  conv_buf[512] ;
	mkf_parser_t *  ml_str_parser ;
	mkf_conv_t *  conv ;

	beg = - ml_screen_get_num_of_logged_lines( vt100_parser->screen) ;
	end = ml_screen_get_rows( vt100_parser->screen) ;

	num = ml_screen_get_region_size( vt100_parser->screen , 0 , beg , 0 , end , 0) ;

	if( ( buf = ml_str_alloca( num)) == NULL)
	{
		return ;
	}

	ml_screen_copy_region( vt100_parser->screen , buf , num , 0 , beg , 0 , end , 0) ;

	if( ( path = alloca( 7 + strlen( file_name) + 4 + 1)) == NULL)
	{
		return ;
	}
	sprintf( path , "mlterm/%s.snp" , file_name) ;

	if( ( path = kik_get_user_rc_path( path)) == NULL)
	{
		return ;
	}

	file = fopen( path , "w") ;
	free( path) ;
	if( ! file)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Failed to open %s\n" , file_name) ;
	#endif

		return ;
	}

	if( ! ( ml_str_parser = ml_str_parser_new()))
	{
		fclose( file) ;

		return ;
	}

	(*ml_str_parser->init)( ml_str_parser) ;
	ml_str_parser_set_str( ml_str_parser , buf , num) ;

	if( encoding == ML_UNKNOWN_ENCODING || ( conv = ml_conv_new( encoding)) == NULL)
	{
		conv = vt100_parser->cc_conv ;
	}

	while( ! ml_str_parser->is_eos &&
	       ( num = (*conv->convert)( conv , conv_buf , sizeof( conv_buf) , ml_str_parser)) > 0)
	{
		fwrite( conv_buf , num , 1 , file) ;
	}

	if( conv != vt100_parser->cc_conv)
	{
		(*conv->delete)( conv) ;
	}

	(*ml_str_parser->delete)( ml_str_parser) ;
	fclose( file) ;
}


static int
base64_decode(
	char *  decoded ,
	char *  encoded ,
	size_t  e_len
	)
{
	size_t  d_pos ;
	size_t  e_pos ;
	/* ASCII -> Base64 order */
	int8_t  conv_tbl[] =
	{
		/* 0x2b - */
		62, -1, -1, -1, 63,
		/* 0x30 - */
		52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -2, -1, -1,
		/* 0x40 - */
		-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
		/* 0x50 - */
		15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
		/* 0x60 - */
		-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
		/* 0x70 - 7a */
		41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
	} ;

	d_pos = e_pos = 0 ;

	while( e_len >= e_pos + 4)
	{
		size_t  count ;
		int8_t  bytes[4] ;

		for( count = 0 ; count < 4 ; e_pos ++)
		{
			if( encoded[e_pos] < 0x2b || 0x7a < encoded[e_pos] ||
			    (bytes[count] = conv_tbl[encoded[e_pos] - 0x2b]) == -1)
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG
					" Illegal Base64 %s\n" , encoded) ;
			#endif
			}
			else
			{
				count ++ ;
			}
		}

		decoded[d_pos++] = (((bytes[0] << 2) & 0xfc) | ((bytes[1] >> 4) & 0x3)) ;

		if( bytes[2] != -2)
		{
			decoded[d_pos++] =
				(((bytes[1] << 4) & 0xf0) | ((bytes[2] >> 2) & 0xf)) ;
		}
		else
		{
			break ;
		}

		if( bytes[3] != -2)
		{
			decoded[d_pos++] = (((bytes[2] << 6) & 0xc0) | (bytes[3] & 0x3f)) ;
		}
		else
		{
			break ;
		}
	}

#ifdef  DEBUG
	decoded[d_pos] = '\0' ;
	kik_debug_printf( KIK_DEBUG_TAG " Base64 Decode %s => %s\n" , encoded , decoded) ;
#endif

	return  d_pos ;
}


static void
set_col_size_of_width_a(
	ml_vt100_parser_t *  vt100_parser ,
	u_int  col_size_a
	)
{
	if( col_size_a == 1 || col_size_a == 2)
	{
		vt100_parser->col_size_of_width_a = col_size_a ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" col size should be 1 or 2. default value 1 is used.\n") ;
	#endif

		vt100_parser->col_size_of_width_a = 1 ;
	}
}

static void  soft_reset( ml_vt100_parser_t *  vt100_parser) ;

/*
 * This function will destroy the content of pt.
 */
static void
config_protocol_set(
	ml_vt100_parser_t *  vt100_parser ,
	char *  pt ,
	int  save
	)
{
	int  ret ;
	char *  dev ;

	ret = ml_parse_proto_prefix( &dev , &pt , save) ;
	if( ret <= 0)
	{
		/*
		 * ret == -1: do_challenge failed.
		 * ret ==  0: illegal format.
		 * to_menu is necessarily 0 because it is pty that msg should be returned to.
		 */
		ml_response_config( vt100_parser->pty ,
			ret < 0 ? "forbidden" : "error" , NULL , 0) ;

		return ;
	}

	if( dev && strlen( dev) <= 7 && strstr( dev , "font"))
	{
		char *  key ;
		char *  val ;

		if( ml_parse_proto( NULL , &key , &val , &pt , 0 , 0) && val &&
		    HAS_CONFIG_LISTENER(vt100_parser,set_font))
		{
			/*
			 * Screen is redrawn not in vt100_parser->config_listener->set_font
			 * but in stop_vt100_cmd, so it is not necessary to hold
			 * vt100_parser->config_listener->set_font between stop_vt100_cmd and
			 * start_vt100_cmd.
			 */
		#if  0
			stop_vt100_cmd( vt100_parser , 0) ;
		#endif

			(*vt100_parser->config_listener->set_font)(
				vt100_parser->config_listener->self ,
				dev , key , val , save) ;

		#if  0
			start_vt100_cmd( vt100_parser , 0) ;
		#endif
		}
	}
	else if( dev && strcmp( dev , "color") == 0)
	{
		char *  key ;
		char *  val ;

		if( ml_parse_proto( NULL , &key , &val , &pt , 0 , 0) && val &&
		    HAS_CONFIG_LISTENER(vt100_parser,set_color))
		{
			/*
			 * Screen is redrawn not in vt100_parser->config_listener->set_color
			 * but in stop_vt100_cmd, so it is not necessary to hold
			 * vt100_parser->config_listener->set_font between stop_vt100_cmd and
			 * start_vt100_cmd.
			 */
		#if  0
			stop_vt100_cmd( vt100_parser , 0) ;
		#endif

			(*vt100_parser->config_listener->set_color)(
				vt100_parser->config_listener->self , dev , key , val, save) ;

		#if  0
			start_vt100_cmd( vt100_parser , 0) ;
		#endif
		}
	}
	else
	{
		stop_vt100_cmd( vt100_parser , 0) ;

		if( ( ! HAS_CONFIG_LISTENER(vt100_parser,exec) ||
	              ! (*vt100_parser->config_listener->exec)(
				vt100_parser->config_listener->self , pt)))
		{
			kik_conf_write_t *  conf ;

			if( save)
			{
				char *  path ;

				/* XXX */
				if( ( path = kik_get_user_rc_path( "mlterm/main")) == NULL)
				{
					return ;
				}

				conf = kik_conf_write_open( path) ;
				free( path) ;
			}
			else
			{
				conf = NULL ;
			}

			/* accept multiple key=value pairs. */
			while( pt)
			{
				char *  key ;
				char *  val ;

				if( ! ml_parse_proto( dev ? NULL : &dev ,
						&key , &val , &pt , 0 , 1))
				{
					break ;
				}

				if( conf)
				{
					/* XXX */
					if( strcmp( key , "xim") != 0)
					{
						kik_conf_io_write( conf , key , val) ;
					}
				}

				if( val == NULL)
				{
					val = "" ;
				}

				if( HAS_CONFIG_LISTENER(vt100_parser,set) &&
				    (*vt100_parser->config_listener->set)(
						vt100_parser->config_listener->self ,
						dev , key , val))
				{
					if( ! vt100_parser->config_listener)
					{
						/* pty changed. */
						break ;
					}
				}

				dev = NULL ;
			}

			if( conf)
			{
				kik_conf_write_close( conf) ;

				if( HAS_CONFIG_LISTENER(vt100_parser,saved))
				{
					(*vt100_parser->config_listener->saved)() ;
				}
			}
		}

		start_vt100_cmd( vt100_parser , 0) ;
	}
}

static void
config_protocol_set_simple(
	ml_vt100_parser_t *  vt100_parser ,
	char *  key ,
	char *  val
	)
{
	if( HAS_CONFIG_LISTENER(vt100_parser,set))
	{
		stop_vt100_cmd( vt100_parser , 0) ;

		(*vt100_parser->config_listener->set)(
			vt100_parser->config_listener->self , NULL , key , val) ;

		start_vt100_cmd( vt100_parser , 0) ;
	}
}

/*
 * This function will destroy the content of pt.
 */
static void
config_protocol_get(
	ml_vt100_parser_t *  vt100_parser ,
	char *  pt ,
	int  to_menu ,
	int *  flag
	)
{
	char *  dev ;
	char *  key ;
	int  ret ;

	/*
	 * It is assumed that screen is not redrawn not in
	 * vt100_parser->config_listener->get, so vt100_parser->config_listener->get
	 * is not held between stop_vt100_cmd and start_vt100_cmd.
	 */
#if  0
	stop_vt100_cmd( vt100_parser , 0) ;
#endif

	ret = ml_parse_proto( &dev , &key , NULL , &pt , to_menu == 0 , 0) ;
	if( ret <= 0)
	{
		/*
		 * ret == -1: do_challenge failed.
		 * ret ==  0: illegal format.
		 * to_menu is necessarily 0 because it is pty that msg should be returned to.
		 */
		ml_response_config( vt100_parser->pty ,
			ret < 0 ? "forbidden" : "error" , NULL , 0) ;

		return ;
	}

	if( dev && strlen( dev) <= 7 && strstr( dev , "font"))
	{
		char *  cs ;

		if( ret == 0)
		{
			cs = key ;
		}
		else if( ! ( cs = kik_str_sep( &key , ",")) || ! key)
		{
			return ;
		}
		
		if( HAS_CONFIG_LISTENER(vt100_parser,get_font))
		{
			(*vt100_parser->config_listener->get_font)(
				vt100_parser->config_listener->self ,
				dev , key /* font size */ , cs , to_menu) ;
		}
	}
	else if( HAS_CONFIG_LISTENER(vt100_parser,get))
	{
		(*vt100_parser->config_listener->get)(
			vt100_parser->config_listener->self , dev , key , to_menu) ;
	}

#if  0
	start_vt100_cmd( vt100_parser , 0) ;
#endif
}

#ifdef  SUPPORT_ITERM2_OSC1337
/*
 * This function will destroy the content of pt.
 */
static void
iterm2_proprietary_set(
	ml_vt100_parser_t *  vt100_parser ,
	char *  pt
	)
{
	char *  path ;

	if( strncmp( pt , "File=" , 5) == 0 &&
	    ( path = get_home_file_path( "" ,
			ml_pty_get_slave_name( vt100_parser->pty) + 5 , "img")))
	{
		/* See http://www.iterm2.com/images.html (2014/03/20) */

		char *  args ;
		char *  encoded ;
		char *  decoded ;
		size_t  e_len ;
		u_int  width ;
		u_int  height ;

		args = pt + 5 ;
		width = height = 0 ;

		if( ( encoded = strchr( args , ':')))
		{
			char *  beg ;
			char *  end ;

			*(encoded++) = '\0' ;

			if( ( beg = strstr( args , "width=")) &&
			    ( end = strchr( beg , ';')))
			{
				*(end--) = '\0' ;
				if( '0' <= *end && *end <= '9')
				{
					width = atoi( beg + 6) ;
				}
			}

			if( ( beg = strstr( args , "height=")) &&
			    ( end = strchr( beg , ';')))
			{
				*(end--) = '\0' ;
				if( '0' <= *end && *end <= '9')
				{
					height = atoi( beg + 7) ;
				}
			}
		}

		if( ( e_len = strlen( encoded)) > 0 && ( decoded = malloc( e_len)))
		{
			size_t  d_len ;
			FILE *  fp ;

			if( ( d_len = base64_decode( decoded , encoded , e_len)) > 0 &&
			    ( fp = fopen( path , "w")))
			{
				fwrite( decoded , 1 , d_len , fp) ;
				fclose( fp) ;

				show_picture( vt100_parser , path ,
						0 , 0 , 0 , 0 , width , height , 0) ;
			}

			free( decoded) ;
		}

		free( path) ;
	}
}
#endif

static int
change_char_fine_color(
	ml_vt100_parser_t *  vt100_parser ,
	int *  ps ,
	int  num
	)
{
	int  proceed ;
	ml_color_t  color ;

	if( ps[0] != 38 && ps[0] != 48)
	{
		return  0 ;
	}

	if( num >= 3 && ps[1] == 5)
	{
		proceed = 3 ;
		color = ps[2] ;
	}
	else if( num >= 5 && ps[1] == 2)
	{
		proceed = 5 ;
		color = ml_get_closest_color( ps[2] , ps[3] , ps[4]) ;
	}
	else
	{
		return  1 ;
	}

	if( use_ansi_colors)
	{
		if( ps[0] == 38)
		{
			vt100_parser->fg_color = color ;
			ml_screen_set_bce_fg_color( vt100_parser->screen , color) ;
		}
		else /* if( ps[0] == 48) */
		{
			vt100_parser->bg_color = color ;
			ml_screen_set_bce_bg_color( vt100_parser->screen , color) ;
		}
	}

	return  proceed ;
}

static void
change_char_attr(
	ml_vt100_parser_t *  vt100_parser ,
	int  flag
	)
{
	ml_color_t  fg_color ;
	ml_color_t  bg_color ;

	fg_color = vt100_parser->fg_color ;
	bg_color = vt100_parser->bg_color ;

	if( flag == 0)
	{
		/* Normal */
		fg_color = ML_FG_COLOR ;
		bg_color = ML_BG_COLOR ;
		vt100_parser->is_bold = 0 ;
		vt100_parser->is_italic = 0 ;
		vt100_parser->underline_style = 0 ;
		vt100_parser->is_reversed = 0 ;
		vt100_parser->is_crossed_out = 0 ;
		vt100_parser->is_blinking = 0 ;
	}
	else if( flag == 1)
	{
		/* Bold */
		vt100_parser->is_bold = 1 ;
	}
	else if( flag == 2)
	{
		/* XXX Faint */
		vt100_parser->is_bold = 0 ;
	}
	else if( flag == 3)
	{
		/* Italic */
		vt100_parser->is_italic = 1 ;
	}
	else if( flag == 4)
	{
		/* Underscore */
		vt100_parser->underline_style = UNDERLINE_NORMAL ;
	}
	else if( flag == 5 || flag == 6)
	{
		/* Blink (6 is repidly blinking) */
		vt100_parser->is_blinking = 1 ;
		ml_screen_enable_blinking( vt100_parser->screen) ;
	}
	else if( flag == 7)
	{
		/* Inverse */
		vt100_parser->is_reversed = 1 ;
	}
#if  0
	else if( flag == 8)
	{
		/* Hidden */
	}
#endif
	else if( flag == 9)
	{
		vt100_parser->is_crossed_out = 1 ;
	}
	else if( flag == 21)
	{
		/* Double underscore */
		vt100_parser->underline_style = UNDERLINE_DOUBLE ;
	}
	else if( flag == 22)
	{
		/* Bold */
		vt100_parser->is_bold = 0 ;
	}
	else if( flag == 23)
	{
		/* Italic */
		vt100_parser->is_italic = 0 ;
	}
	else if( flag == 24)
	{
		/* Underline */
		vt100_parser->underline_style = 0 ;
	}
	else if( flag == 25)
	{
		/* blink */
		vt100_parser->is_blinking = 0 ;
	}
	else if( flag == 27)
	{
		vt100_parser->is_reversed = 0 ;
	}
#if  0
	else if( flag == 28)
	{
		/* Not hidden */
	}
#endif
	else if( flag == 29)
	{
		vt100_parser->is_crossed_out = 0 ;
	}
	else if( use_ansi_colors)
	{
		/* Color attributes */

		if( 30 <= flag && flag <= 37)
		{
			/* 30=ML_BLACK(0) ... 37=ML_WHITE(7) */
			fg_color = flag - 30 ;
		}
		else if( flag == 39)
		{
			/* default fg */
			fg_color = ML_FG_COLOR ;
		}
		else if( 40 <= flag && flag <= 47)
		{
			/* 40=ML_BLACK(0) ... 47=ML_WHITE(7) */
			bg_color = flag - 40 ;
		}
		else if( flag == 49)
		{
			bg_color = ML_BG_COLOR ;
		}
		else if( 90 <= flag && flag <= 97)
		{
			fg_color = (flag - 90) | ML_BOLD_COLOR_MASK ;
		}
		else if( 100 <= flag && flag <= 107)
		{
			bg_color = (flag - 100) | ML_BOLD_COLOR_MASK ;
		}
	#ifdef  DEBUG
		else
		{
			kik_warn_printf( KIK_DEBUG_TAG " unknown char attr flag(%d).\n" , flag) ;
		}
	#endif
	}

	if( fg_color != vt100_parser->fg_color)
	{
		vt100_parser->fg_color = fg_color ;
		ml_screen_set_bce_fg_color( vt100_parser->screen , fg_color) ;
	}

	if( bg_color != vt100_parser->bg_color)
	{
		vt100_parser->bg_color = bg_color ;
		ml_screen_set_bce_bg_color( vt100_parser->screen , bg_color) ;
	}
}

static void
get_rgb(
	ml_vt100_parser_t *  vt100_parser ,
	int  ps ,
	ml_color_t  color
	)
{
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;
	char  rgb[] = "rgb:xxxx/xxxx/xxxx" ;
	char  seq[2 + (DIGIT_STR_LEN(int) + 1) * 2 + sizeof(rgb) + 1] ;

	if( ps >= 10) /* IS_FG_BG_COLOR(color) */
	{
		if( ! HAS_XTERM_LISTENER(vt100_parser,get_rgb) ||
		    ! (*vt100_parser->xterm_listener->get_rgb)(
			vt100_parser->xterm_listener->self , &red , &green , &blue , color))
		{
			return ;
		}
	}
	else if( ! ml_get_color_rgba( color , &red , &green , &blue , NULL))
	{
		return ;
	}

	sprintf( rgb + 4 , "%.2x%.2x/%.2x%.2x/%.2x%.2x" ,
		red , red , green , green , blue , blue) ;

	if( ps >= 10)
	{
		/* ps: 10 = fg , 11 = bg , 12 = cursor bg */
		sprintf( seq , "\x1b]%d;%s\x07" , ps , rgb) ;
	}
	else
	{
		/* ps: 4 , 5 */
		sprintf( seq , "\x1b]%d;%d;%s\x07" , ps , color , rgb) ;
	}

	ml_write_to_pty( vt100_parser->pty , seq , strlen(seq)) ;
}

/*
 * This function will destroy the content of pt.
 */
static void
change_color_rgb(
	ml_vt100_parser_t *  vt100_parser,
	u_char *  pt
	)
{
	char *  p ;
	
	if( ( p = strchr( pt, ';')))
	{
		if( strcmp( p + 1 , "?") == 0)
		{
			ml_color_t  color ;

			*p = '\0' ;

			if( ( color = ml_get_color( pt)) != ML_UNKNOWN_COLOR)
			{
				get_rgb( vt100_parser , 4 , color) ;
			}
		}
		else
		{
			*p = '=' ;

			if( ( p = alloca( 6 + strlen( pt) + 1)))
			{
				sprintf( p , "color:%s" , pt) ;

				config_protocol_set( vt100_parser , p , 0) ;
			}
		}
	}
}

static void
change_special_color(
	ml_vt100_parser_t *  vt100_parser,
	u_char *  pt
	)
{
	char *  key ;

	if( *pt == '0')
	{
		key = "bd_color" ;
	}
	else if( *pt == '1')
	{
		key = "ul_color" ;
	}
	else if( *pt == '2')
	{
		key = "bl_color" ;
	}
	else
	{
		return ;
	}

	if( *(++pt) == ';' &&
	    strcmp( ++pt , "?") != 0)	/* ?: query rgb */
	{
		config_protocol_set_simple( vt100_parser , key , pt) ;
	}
}

static void
set_selection(
	ml_vt100_parser_t *  vt100_parser ,
	u_char *  encoded
	)
{
	if( HAS_XTERM_LISTENER(vt100_parser,set_selection))
	{
		u_char *  p ;
		u_char *  targets ;
		size_t  e_len ;
		size_t  d_len ;
		u_char *  decoded ;
		mkf_char_t  ch ;
		ml_char_t *  str ;
		u_int  str_len ;

		if( ( p = strchr( encoded , ';')))
		{
			*p = '\0' ;
			targets = encoded ;
			encoded = p + 1 ;
		}
		else
		{
			targets = "s0" ;
		}

		if( ( e_len = strlen( encoded)) < 4 || ! ( decoded = alloca( e_len)) ||
		    ( d_len = base64_decode( decoded , encoded , e_len)) == 0 ||
		    ! ( str = ml_str_new( d_len)))
		{
			return ;
		}

		str_len = 0 ;
		(*vt100_parser->cc_parser->set_str)( vt100_parser->cc_parser , decoded , d_len) ;
		while( (*vt100_parser->cc_parser->next_char)( vt100_parser->cc_parser , &ch))
		{
			ml_char_set( &str[str_len++] , mkf_char_to_int(&ch) ,
				ch.cs , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0) ;
		}

		/*
		 * It is assumed that screen is not redrawn not in
		 * vt100_parser->config_listener->get, so vt100_parser->config_listener->get
		 * is not held between stop_vt100_cmd and start_vt100_cmd.
		 */
	#if  0
		stop_vt100_cmd( vt100_parser , 0) ;
	#endif

		(*vt100_parser->xterm_listener->set_selection)(
			vt100_parser->xterm_listener->self , str , str_len , targets) ;

	#if  0
		start_vt100_cmd( vt100_parser , 0) ;
	#endif
	}
}

static void
delete_macro(
	ml_vt100_parser_t *  vt100_parser ,
	int  id		/* should be less than vt100_parser->num_of_macros */
	)
{
	if( vt100_parser->macros[id].is_sixel)
	{
		unlink( vt100_parser->macros[id].str) ;
		vt100_parser->macros[id].is_sixel = 0 ;
		vt100_parser->macros[id].sixel_num ++ ;
	}

	free( vt100_parser->macros[id].str) ;
	vt100_parser->macros[id].str = NULL ;
}

static void
delete_all_macros(
	ml_vt100_parser_t *  vt100_parser
	)
{
	u_int  count ;

	for( count = 0 ; count < vt100_parser->num_of_macros ; count++)
	{
		delete_macro( vt100_parser , count) ;
	}

	free( vt100_parser->macros) ;
	vt100_parser->macros = NULL ;
	vt100_parser->num_of_macros = 0 ;
}

static u_char *
hex_to_text(
	u_char *  hex
	)
{
	u_char *  text ;
	u_char *  p ;
	size_t  len ;
	size_t  count ;
	int  rep_count ;
	u_char *  rep_beg ;
	int  d[2] ;
	int  sub_count ;

	len = strlen( hex) / 2 + 1 ;
	if( ! ( text = malloc( len)))
	{
		return  NULL ;
	}

	count = 0 ;
	rep_count = 1 ;
	rep_beg = NULL ;
	sub_count = 0 ;

	/* Don't use sscanf() here because it works too slow. */
	while( 1)
	{
		if( '0' <= *hex && *hex <= '9')
		{
			d[sub_count++] = *hex - '0' ;
		}
		else
		{
			u_char  masked_hex ;

			masked_hex = (*hex) & 0xcf ;
			if( 'A' <= masked_hex && masked_hex <= 'F')
			{
				d[sub_count++] = masked_hex - 'A' + 10 ;
			}
			else
			{
				sub_count = 0 ;

				if( *hex == '!')
				{
					rep_beg = NULL ;

					if( ( p = strchr( hex + 1 , ';')))
					{
						*p = '\0' ;
						if( ( rep_count = atoi( hex + 1)) > 1)
						{
							rep_beg = text + count ;
						}
						hex = p ;
					}
				}
				else if( *hex == ';' || *hex == '\0')
				{
					if( rep_beg)
					{
						size_t  rep_len ;

						if( ( rep_len = text + count - rep_beg) > 0)
						{
							len += rep_len * (rep_count - 1) ;
							if( ! ( p = realloc( text , len)))
							{
								free( text) ;

								return  NULL ;
							}
							rep_beg += (p - text) ;
							text = p ;

							while( --rep_count > 0)
							{
								strncpy( text + count ,
									rep_beg , rep_len) ;
								count += rep_len ;
							}
						}

						rep_beg = NULL ;
					}

					if( *hex == '\0')
					{
						goto  end ;
					}
				}
			}
		}

		if( sub_count == 2)
		{
			text[count++] = (d[0] << 4) + d[1] ;
			sub_count = 0 ;
		}

		hex++ ;
	}

end:
	text[count] = '\0' ;

	return  text ;
}

#define  MAX_NUM_OF_MACRO  1024
#define  MAX_DIGIT_OF_MACRO  4

static void
define_macro(
	ml_vt100_parser_t *  vt100_parser ,
	u_char *  param ,
	u_char *  data
	)
{
	u_char *  p ;
	int  ps[3] = { 0 , 0 , 0 } ;
	int  num ;

	p = param ;
	num = 0 ;
	for( p = param ; *p != 'z' ; p++)
	{
		if( ( *p == ';' || *p == '!') && num < 3)
		{
			*p = '\0' ;
			ps[num++] = atoi( param) ;
			param = p + 1 ;
		}
	}

	if( ps[0] >= MAX_NUM_OF_MACRO)
	{
		return ;
	}

	if( ps[1] == 1)
	{
		delete_all_macros( vt100_parser) ;
	}

	if( ps[0] >= vt100_parser->num_of_macros)
	{
		void *  p ;

		if( *data == '\0' ||
		    ! ( p = realloc( vt100_parser->macros ,
				(ps[0] + 1) * sizeof(*vt100_parser->macros))))
		{
			return ;
		}

		memset( p + vt100_parser->num_of_macros * sizeof(*vt100_parser->macros) , 0 ,
			(ps[0] + 1 - vt100_parser->num_of_macros) * sizeof(*vt100_parser->macros)) ;
		vt100_parser->macros = p ;
		vt100_parser->num_of_macros = ps[0] + 1 ;
	}
	else
	{
		delete_macro( vt100_parser , ps[0]) ;

		if( *data == '\0')
		{
			return ;
		}
	}

	if( ps[2] == 1)
	{
		p = vt100_parser->macros[ps[0]].str = hex_to_text( data) ;

	#ifndef  NO_IMAGE
		if( p &&
		    ( *p == 0x90 || ( *(p++) == '\x1b' && *p == 'P')))
		{
			for( p++ ; *p == ';' || ('0' <= *p && *p <= '9') ; p++) ;

			if( *p == 'q' &&
			    ( strrchr( p , 0x9c) ||
			      (( p = strrchr( p , '\\')) && *(p - 1) == '\x1b')))
			{
				char  prefix[5 + MAX_DIGIT_OF_MACRO + 1 +
					DIGIT_STR_LEN(vt100_parser->macros[0].sixel_num) + 2] ;
				char *  path ;

				sprintf( prefix , "macro%d_%d_" , ps[0] ,
					vt100_parser->macros[ps[0]].sixel_num) ;

				if( ( path = get_home_file_path( prefix ,
						ml_pty_get_slave_name(
							vt100_parser->pty) + 5 , "six")))
				{
					FILE *  fp ;

					if( ( fp = fopen( path , "w")))
					{
						fwrite( vt100_parser->macros[ps[0]].str , 1 ,
							strlen( vt100_parser->macros[ps[0]].str) ,
							fp) ;
						fclose( fp) ;

						free( vt100_parser->macros[ps[0]].str) ;
						vt100_parser->macros[ps[0]].str = path ;
						vt100_parser->macros[ps[0]].is_sixel = 1 ;

					#ifdef  DEBUG
						kik_debug_printf( KIK_DEBUG_TAG
							" Register %s to macro %d\n" ,
								path , ps[0]) ;
					#endif
					}
				}
			}
		}
	#endif
	}
	else
	{
		vt100_parser->macros[ps[0]].str = strdup( data) ;
	}
}

static int  write_loopback( ml_vt100_parser_t *  vt100_parser ,
	const u_char *  buf , size_t  len , int  enable_local_echo , int  is_visual) ;

static void
invoke_macro(
	ml_vt100_parser_t *  vt100_parser ,
	int  id
	)
{
	if( id < vt100_parser->num_of_macros && vt100_parser->macros[id].str)
	{
	#ifndef  NO_IMAGE
		if( vt100_parser->macros[id].is_sixel)
		{
			show_picture( vt100_parser , vt100_parser->macros[id].str ,
				0 , 0 , 0 , 0 , 0 , 0 , 3) ;
		}
		else
	#endif
		{
			write_loopback( vt100_parser , vt100_parser->macros[id].str ,
				strlen( vt100_parser->macros[id].str) , 0 , 0) ;
		}
	}
}

static void
clear_line_all(
	ml_vt100_parser_t *  vt100_parser
	)
{
	/*
	 * XXX
	 * cursor position should be restored.
	 */
	ml_screen_goto_beg_of_line( vt100_parser->screen) ;
	ml_screen_clear_line_to_right( vt100_parser->screen) ;
}

static void
clear_display_all(
	ml_vt100_parser_t *  vt100_parser
	)
{
	/*
	 * XXX
	 * cursor position should be restored.
	 */
	ml_screen_goto_home( vt100_parser->screen) ;
	ml_screen_clear_below( vt100_parser->screen) ;
}

static void
soft_reset(
	ml_vt100_parser_t *  vt100_parser
	)
{
	/*
	 * XXX insufficient implementation.
	 */

	/* "CSI ? 25 h" (DECTCEM) */
	ml_screen_cursor_visible( vt100_parser->screen) ;

	/* "CSI l" (IRM) */
	vt100_parser->w_buf.output_func = ml_screen_overwrite_chars ;

	/* "CSI ? 6 l" (DECOM) */
	ml_screen_set_absolute_origin( vt100_parser->screen) ;

	/*
	 * "CSI ? 7 h" (DECAWM) (xterm compatible behavior)
	 * ("CSI ? 7 l" according to VT220 reference manual)
	 */
	ml_screen_set_auto_wrap( vt100_parser->screen , 1) ;

	/* "CSI r" (DECSTBM) */
	ml_screen_set_scroll_region( vt100_parser->screen , -1 , -1) ;

	/* "CSI ? 69 l" */
	ml_screen_set_use_margin( vt100_parser->screen , 0) ;

	/* "CSI m" (SGR) */
	change_char_attr( vt100_parser , 0) ;

	ml_init_encoding_parser( vt100_parser) ;

	( ml_screen_is_alternative_edit( vt100_parser->screen) ?
		&vt100_parser->saved_alternate
		: &vt100_parser->saved_normal)->is_saved = 0 ;

	vt100_parser->mouse_mode = 0 ;
	vt100_parser->ext_mouse_mode = 0 ;
	vt100_parser->is_app_keypad = 0 ;
	vt100_parser->is_app_cursor_keys = 0 ;
	vt100_parser->is_app_escape = 0 ;
	vt100_parser->is_bracketed_paste_mode = 0 ;

	vt100_parser->im_is_active = 0 ;
}

static void
send_device_attributes(
	ml_pty_ptr_t  pty ,
	int  rank
	)
{
	char *  seq ;

	if( rank == 1)
	{
		/* vt100 answerback */
	#ifndef  NO_IMAGE
		seq = "\x1b[?1;2;4;7c" ;
	#else
		seq = "\x1b[?1;2;7c" ;
	#endif
	}
	else if( rank == 2)
	{
		/*
		 * If Pv is greater than 95, vim sets ttymouse=xterm2
		 * automatically.
		 */
		seq = "\x1b[>1;96;0c" ;
	}
	else
	{
		return ;
	}

	ml_write_to_pty( pty , seq , strlen(seq)) ;
}


/*
 * For string outside escape sequences.
 */
static int
increment_str(
	u_char **  str ,
	size_t *  left
	)
{
	if( -- (*left) == 0)
	{
		return  0 ;
	}

	(*str) ++ ;

	return  1 ;
}

/*
 * For string inside escape sequences.
 */
static int
inc_str_in_esc_seq(
	ml_screen_t *  screen ,
	u_char **  str_p ,
	size_t *  left ,
	int  want_ctrl
	)
{
	while( 1)
	{
		if( increment_str( str_p , left) == 0)
		{
			return  0 ;
		}

		if( **str_p < 0x20 || 0x7e < **str_p)
		{
			/*
			 * cursor-control characters inside ESC sequences
			 */
			if( CTL_LF <= **str_p && **str_p <= CTL_FF)
			{
				ml_screen_line_feed( screen) ;
			}
			else if( **str_p == CTL_CR)
			{
				ml_screen_goto_beg_of_line( screen) ;
			}
			else if( **str_p == CTL_BS)
			{
				ml_screen_go_back( screen , 1) ;
			}
			else if( want_ctrl)
			{
				return  1 ;
			}
			else
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" Ignored 0x%x inside escape sequences.\n" , **str_p) ;
			#endif
			}
		}
		else
		{
			return  1 ;
		}
	}
}

/*
 * Set use_c1=0 for 0x9c not to be regarded as ST if str can be encoded by utf8.
 * (UTF-8 uses 0x80-0x9f as printable characters.)
 */
static char *
get_pt_in_esc_seq(
	u_char **  str ,
	size_t *  left ,
	int  use_c1 ,		/* OSC is terminated by not only ST(ESC \) but also 0x9c. */
	int  bel_terminate	/* OSC is terminated by not only ST(ESC \) but also BEL. */
	)
{
	u_char *  pt ;

	pt = *str ;

	while( 1)
	{
		if( ( bel_terminate && **str == CTL_BEL) ||
		    ( use_c1 && **str == 0x9c))
		{
			**str = '\0' ;

			break ;
		}

		if( **str == CTL_ESC)
		{
			if( ! increment_str( str , left))
			{
				return  NULL ;
			}

			if( **str == '\\')
			{
				*((*str) - 1) = '\0' ;

				break ;
			}

			/*
			 * Reset position ahead of unprintable character for compat with xterm.
			 * e.g.) "\x1bP\x1b[A" is parsed as "\x1b[A"
			 */
			(*str) -= 2 ;
			(*left) += 2 ;

			return  NULL ;
		}

		if( ! increment_str( str , left))
		{
			return  NULL ;
		}
	}

	return  pt ;
}

#ifdef  DEBUG
static void
debug_print_unknown(
	const char *  format ,
	...
	)
{
	va_list  arg_list ;

	va_start( arg_list , format) ;

	kik_debug_printf( KIK_DEBUG_TAG " received unknown sequence ") ;
	vfprintf( stderr , format , arg_list) ;
}
#endif


#ifdef  USE_VT52
inline static int
parse_vt52_escape_sequence(
	ml_vt100_parser_t *  vt100_parser	/* vt100_parser->r_buf.left must be more than 0 */
	)
{
	u_char *  str_p ;
	size_t  left ;

	str_p = CURRENT_STR_P(vt100_parser) ;
	left = vt100_parser->r_buf.left ;

	if( ! inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0))
	{
		return  0 ;
	}

	if( *str_p == 'A')
	{
		ml_screen_go_upward( vt100_parser->screen , 1) ;
	}
	else if( *str_p == 'B')
	{
		ml_screen_go_downward( vt100_parser->screen , 1) ;
	}
	else if( *str_p == 'C')
	{
		ml_screen_go_forward( vt100_parser->screen , 1) ;
	}
	else if( *str_p == 'D')
	{
		ml_screen_go_back( vt100_parser->screen , 1) ;
	}
	else if( *str_p == 'F')
	{
		/* Enter graphics mode */
	}
	else if( *str_p == 'G')
	{
		/* Exit graphics mode */
	}
	else if( *str_p == 'H')
	{
		ml_screen_goto( vt100_parser->screen , 0 , 0) ;
	}
	else if( *str_p == 'I')
	{
		if( ml_screen_cursor_row( vt100_parser->screen) == 0)
		{
			ml_screen_scroll_downward( vt100_parser->screen , 1) ;
		}
		else
		{
			ml_screen_go_upward( vt100_parser->screen , 1) ;
		}
	}
	else if( *str_p == 'J')
	{
		ml_screen_clear_below( vt100_parser->screen) ;
	}
	else if( *str_p == 'K')
	{
		ml_screen_clear_line_to_right( vt100_parser->screen) ;
	}
	else if( *str_p == 'Y')
	{
		int  row ;
		int  col ;

		if( ! inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0))
		{
			return  0 ;
		}

		if( *str_p < ' ')
		{
			goto  end ;
		}

		row = *str_p - ' ' ;

		if( ! inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0))
		{
			return  0 ;
		}

		if( *str_p < ' ')
		{
			goto  end ;
		}

		col = *str_p - ' ' ;

		ml_screen_goto( vt100_parser->screen , col , row) ;
	}
	else if( *str_p == 'Z')
	{
		char  msg[] = "\x1b/Z" ;

		ml_write_to_pty( vt100_parser->pty , msg , sizeof( msg) - 1) ;
	}
	else if( *str_p == '=')
	{
		/* Enter altenate keypad mode */
	}
	else if( *str_p == '>')
	{
		/* Exit altenate keypad mode */
	}
	else if( *str_p == '<')
	{
		vt100_parser->is_vt52_mode = 0 ;
	}
	else
	{
		/* not VT52 control sequence */

		return  1 ;
	}

end:
	vt100_parser->r_buf.left = left - 1 ;

	return  1 ;
}
#endif

/*
 * Parse escape/control sequence. Note that *any* valid format sequence
 * is parsed even if mlterm doesn't support it.
 *
 * Return value:
 * 0 => vt100_parser->r_buf.left == 0
 * 1 => Finished parsing. (If current vt100_parser->r_buf.chars is not escape sequence,
 *      return without doing anthing.)
 */
inline static int
parse_vt100_escape_sequence(
	ml_vt100_parser_t *  vt100_parser	/* vt100_parser->r_buf.left must be more than 0 */
	)
{
	u_char *  str_p ;
	size_t  left ;

#if  0
	if( vt100_parser->r_buf.left == 0)
	{
		/* end of string */

		return  1 ;
	}
#endif

	str_p = CURRENT_STR_P(vt100_parser) ;
	left = vt100_parser->r_buf.left ;

	if( *str_p == CTL_ESC)
	{
	#ifdef  ESCSEQ_DEBUG
		kik_msg_printf( "RECEIVED ESCAPE SEQUENCE(current left = %d: ESC", left) ;
	#endif

	#ifdef  USE_VT52
		if( vt100_parser->is_vt52_mode)
		{
			return  parse_vt52_escape_sequence( vt100_parser) ;
		}
	#endif

		if( ! inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0))
		{
			return  0 ;
		}

		if( *str_p == '7')
		{
			/* "ESC 7" save cursor */

			save_cursor( vt100_parser) ;
		}
		else if( *str_p == '8')
		{
			/* "ESC 8" restore cursor */

			restore_cursor( vt100_parser) ;
		}
		else if( *str_p == '=')
		{
			/* "ESC =" application keypad */

			vt100_parser->is_app_keypad = 1 ;
		}
		else if( *str_p == '>')
		{
			/* "ESC >" normal keypad */

			vt100_parser->is_app_keypad = 0 ;
		}
		else if( *str_p == 'D')
		{
			/* "ESC D" index(scroll up) */

			ml_screen_index( vt100_parser->screen) ;
		}
		else if( *str_p == 'E')
		{
			/* "ESC E" next line */

			ml_screen_line_feed( vt100_parser->screen) ;
			ml_screen_goto_beg_of_line( vt100_parser->screen) ;
		}
	#if  0
		else if( *str_p == 'F')
		{
			/* "ESC F" cursor to lower left corner of screen */
		}
	#endif
		else if( *str_p == 'H')
		{
			/* "ESC H" set tab */

			ml_screen_set_tab_stop( vt100_parser->screen) ;
		}
		else if( *str_p == 'M')
		{
			/* "ESC M" reverse index(scroll down) */

			ml_screen_reverse_index( vt100_parser->screen) ;
		}
		else if( *str_p == 'Z')
		{
			/* "ESC Z" return terminal id (Obsolete form of CSI c) */

			send_device_attributes( vt100_parser->pty , 1) ;
		}
		else if( *str_p == 'c')
		{
			/* "ESC c" full reset */

			soft_reset( vt100_parser) ;
			clear_display_all( vt100_parser) ; /* cursor goes home in it. */
			delete_all_macros( vt100_parser) ;
		}
	#if  0
		else if( *str_p == 'l')
		{
			/* "ESC l" memory lock */
		}
	#endif
	#if  0
		else if( *str_p == 'm')
		{
			/* "ESC m" memory unlock */
		}
	#endif
		else if( *str_p == '[')
		{
			/*
			 * "ESC [" (CSI)
			 * CSI P.....P I.....I F
			 *     060-077 040-057 100-176
			 */

		#define  MAX_NUM_OF_PS  10

			u_char  pre_ch ;
			int  ps[MAX_NUM_OF_PS] ;
			int  num ;

			if( ! inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0))
			{
				return  0 ;
			}

			/* Parameter characters(0x30-0x3f) */

			if( 0x3c <= *str_p && *str_p <= 0x3f)
			{
				/* parameter character except numeric, ':' and ';' (< = > ?). */
				pre_ch = *str_p ;

				if( ! inc_str_in_esc_seq( vt100_parser->screen ,
						&str_p , &left , 0))
				{
					return  0 ;
				}
			}
			else
			{
				pre_ch = '\0' ;
			}

			num = 0 ;
			while( 1)
			{
				if( *str_p == ';')
				{
					/*
					 * "CSI ; n" is regarded as "CSI -1 ; n"
					 */
					if( num < MAX_NUM_OF_PS)
					{
						ps[num ++] = -1 ;
					}
				}
				else
				{
					if( '0' <= *str_p && *str_p <= '9')
					{
						u_char  digit[DIGIT_STR_LEN(int) + 1] ;
						int  count ;

						if( *str_p == '0')
						{
							/* 000000001 -> 01 */
							while( left > 1 && *(str_p + 1) == '0')
							{
								str_p ++ ;
								left -- ;
							}
						}

						digit[0] = *str_p ;

						for( count = 1 ; count < DIGIT_STR_LEN(int) ;
							count++)
						{
							if( ! inc_str_in_esc_seq(
									vt100_parser->screen ,
									&str_p , &left , 0))
							{
								return  0 ;
							}

							if( *str_p < '0' || '9' < *str_p)
							{
								break ;
							}

							digit[count] = *str_p ;
						}

						digit[count] = '\0' ;
						if( num < MAX_NUM_OF_PS)
						{
							ps[num ++] = atoi( digit) ;
						#ifdef  MAX_PS_DIGIT
							if( ps[num - 1] > MAX_PS_DIGIT)
							{
								ps[num - 1] = MAX_PS_DIGIT ;
							}
						#endif
						}
					}

					if( *str_p < 0x30 || 0x3f < *str_p)
					{
						/* Is not a parameter character(0x30-0x3f). */

						/*
						 * "CSI 0 n" is *not* regarded as "CSI 0 ; -1 n"
						 * "CSI n" is regarded as "CSI -1 n"
						 * "CSI 0 ; n" is regarded as "CSI 0 ; -1 n"
						 * "CSI ; n" which has been already regarded as
						 * "CSI -1 ; n" is regarded as "CSI -1 ; -1 n"
						 */
						if( num == 0 ||
						    (*(str_p - 1) == ';' && num < MAX_NUM_OF_PS) )
						{
							ps[num ++] = -1 ;
						}

						/* num is always greater than 0 */

						break ;
					}
				}

				if( ! inc_str_in_esc_seq( vt100_parser->screen ,
						&str_p , &left , 0))
				{
					return  0 ;
				}
			}

			/*
			 * Skip trailing paremeter(0x30-0x3f) and intermediate(0x20-0x2f)
			 * characters.
			 */
			while( 0x20 <= *str_p && *str_p <= 0x3f)
			{
				if( pre_ch == '\0')
				{
					pre_ch = *str_p ;
				}

				if( ! inc_str_in_esc_seq( vt100_parser->screen ,
						&str_p , &left , 0))
				{
					return  0 ;
				}
			}

			if( pre_ch == '?')
			{
				if( *str_p == 'h')
				{
					/* "CSI ? h" DEC Private Mode Set */
					int  count ;

					for( count = 0 ; count < num ; count++)
					{
						if( ps[count] == 1)
						{
							/* "CSI ? 1 h" */

							vt100_parser->is_app_cursor_keys = 1 ;
						}
					#ifdef  USE_VT52
						else if( ps[count] == 2)
						{
							/* "CSI ? 2 h" */
							vt100_parser->is_vt52_mode = 0 ;

							/*
							 * USASCII should be designated for G0-G3
							 * here, but it is temporized by using
							 * ml_init_encoding_parser() etc for now.
							 */

							ml_init_encoding_parser( vt100_parser) ;
						}
					#endif
						else if( ps[count] == 3)
						{
							/* "CSI ? 3 h" */

							/* XTERM compatibility [#1048321] */
							clear_display_all( vt100_parser) ;

							resize( vt100_parser , 132 , 0 , 1) ;
						}
					#if  0
						else if( ps[count] == 4)
						{
							/* "CSI ? 4 h" smooth scrolling */
						}
					#endif
						else if( ps[count] == 5)
						{
							/* "CSI ? 5 h" */

							reverse_video( vt100_parser , 1) ;
						}
						else if( ps[count] == 6)
						{
							/* "CSI ? 6 h" relative origins */

							ml_screen_set_relative_origin(
								vt100_parser->screen) ;
							/*
							 * cursor position is reset
							 * (the same behavior of xterm 4.0.3,
							 * kterm 6.2.0 or so)
							 */
							ml_screen_goto( vt100_parser->screen ,
								0 , 0) ;
						}
						else if( ps[count] == 7)
						{
							/* "CSI ? 7 h" auto wrap */

							ml_screen_set_auto_wrap(
								vt100_parser->screen , 1) ;
						}
					#if  0
						else if( ps[count] == 8)
						{
							/* "CSI ? 8 h" auto repeat */
						}
					#endif
					#if  0
						else if( ps[count] == 9)
						{
							/* "CSI ? 9 h" X10 mouse reporting */
						}
					#endif
						else if( ps[count] == 25)
						{
							/* "CSI ? 25 h" */
							ml_screen_cursor_visible(
								vt100_parser->screen) ;
						}
					#if  0
						else if( ps[count] == 35)
						{
							/* "CSI ? 35 h" shift keys */
						}
					#endif
					#if  0
						else if( ps[count] == 40)
						{
							/* "CSI ? 40 h" 80 <-> 132 */
						}
					#endif
						else if( ps[count] == 47)
						{
							/*
							 * "CSI ? 47 h"
							 * Use alternate screen buffer
							 */

							if( use_alt_buffer)
							{
								ml_screen_use_alternative_edit(
									vt100_parser->screen) ;
							}
						}
						else if( ps[count] == 66)
						{
							/* "CSI ? 66 h" application key pad */
							vt100_parser->is_app_keypad = 1 ;
						}
					#if  0
						else if( ps[count] == 67)
						{
							/* "CSI ? 67 h" have back space */
						}
					#endif
						else if( ps[count] == 69)
						{
							/* "CSI ? 69 h" */

							ml_screen_set_use_margin(
								vt100_parser->screen , 1) ;
						}
						else if( ps[count] == 80)
						{
							/* "CSI ? 80 h" */

							vt100_parser->sixel_scrolling = 0 ;
						}
						else if( ps[count] == 1000)
						{
							/* "CSI ? 1000 h" */

							set_mouse_report( vt100_parser ,
								MOUSE_REPORT) ;
						}
					#if  0
						else if( ps[count] == 1001)
						{
							/* "CSI ? 1001 h" X11 mouse highlighting */
						}
					#endif
						else if( ps[count] == 1002)
						{
							/* "CSI ? 1002 h" */

							set_mouse_report( vt100_parser ,
								BUTTON_EVENT_MOUSE_REPORT) ;
						}
						else if( ps[count] == 1003)
						{
							/* "CSI ? 1003 h" */

							set_mouse_report( vt100_parser ,
								ANY_EVENT_MOUSE_REPORT) ;
						}
						else if( ps[count] == 1004)
						{
							/* "CSI ? 1004 h" */

							vt100_parser->want_focus_event = 1 ;
						}
						else if( ps[count] == 1005)
						{
							/* "CSI ? 1005 h" */

							vt100_parser->ext_mouse_mode =
								EXTENDED_MOUSE_REPORT_UTF8 ;
						}
						else if( ps[count] == 1006)
						{
							/* "CSI ? 1006 h" */

							vt100_parser->ext_mouse_mode =
								EXTENDED_MOUSE_REPORT_SGR ;
						}
						else if( ps[count] == 1015)
						{
							/* "CSI ? 1015 h" */

							vt100_parser->ext_mouse_mode =
								EXTENDED_MOUSE_REPORT_URXVT ;
						}
					#if  0
						else if( ps[count] == 1010)
						{
							/*
							 * "CSI ? 1010 h"
							 * scroll to bottom on tty output inhibit
							 */
						}
					#endif
					#if  0
						else if( ps[count] == 1011)
						{
							/*
							 * "CSI ? 1011 h"
							 * "scroll to bottom on key press
							 */
						}
					#endif
						else if( ps[count] == 1042)
						{
							/* "CSI ? 1042 h" */
							config_protocol_set_simple( vt100_parser ,
								"use_urgent_bell" , "true") ;
						}
						else if( ps[count] == 1047)
						{
							/* "CSI ? 1047 h" */

							if( use_alt_buffer)
							{
								ml_screen_use_alternative_edit(
									vt100_parser->screen) ;
							}
						}
						else if( ps[count] == 1048)
						{
							/* "CSI ? 1048 h" */

							if( use_alt_buffer)
							{
								save_cursor( vt100_parser) ;
							}
						}
						else if( ps[count] == 1049)
						{
							/* "CSI ? 1049 h" */

							if( use_alt_buffer &&
							    ! ml_screen_is_alternative_edit(
								vt100_parser->screen))
							{
								save_cursor( vt100_parser) ;
								ml_screen_use_alternative_edit(
									vt100_parser->screen) ;
								clear_display_all( vt100_parser) ;
							}
						}
						else if( ps[count] == 2004)
						{
							/* "CSI ? 2004 h" */

							vt100_parser->is_bracketed_paste_mode = 1 ;
						}
						else if( ps[count] == 7727)
						{
							/* "CSI ? 7727 h" */

							vt100_parser->is_app_escape = 1 ;
						}
						else if( ps[count] == 8428)
						{
							/* "CSI ? 8428 h" (RLogin original) */

							vt100_parser->col_size_of_width_a = 1 ;
						}
						else if( ps[count] == 8800)
						{
							vt100_parser->unicode_policy |=
								USE_UNICODE_DRCS ;
						}
						else if( ps[count] == 9500)
						{
							/* "CSI ? 9500 h" */

							config_protocol_set_simple( vt100_parser ,
								"use_local_echo" , "true") ;
						}
					#ifdef  DEBUG
						else
						{
							debug_print_unknown( "ESC [ ? %d h\n" ,
								ps[count]) ;
						}
					#endif
					}
				}
				else if( *str_p == 'l')
				{
					/* DEC Private Mode Reset */
					int  count ;

					for( count = 0 ; count < num ; count++)
					{
						if( ps[count] == 1)
						{
							/* "CSI ? 1 l" */

							vt100_parser->is_app_cursor_keys = 0 ;
						}
					#ifdef  USE_VT52
						else if( ps[count] == 2)
						{
							/* "CSI ? 2 l" */
							vt100_parser->is_vt52_mode = 1 ;
						}
					#endif
						else if( ps[count] == 3)
						{
							/* "CSI ? 3 l" */

							/* XTERM compatibility [#1048321] */
							clear_display_all( vt100_parser) ;

							resize( vt100_parser , 80 , 0 , 1) ;
						}
					#if  0
						else if( ps[count] == 4)
						{
							/* "CSI ? 4 l" smooth scrolling */
						}
					#endif
						else if( ps[count] == 5)
						{
							/* "CSI ? 5 l" */

							reverse_video( vt100_parser , 0) ;
						}
						else if( ps[count] == 6)
						{
							/* "CSI ? 6 l" absolute origins */

							ml_screen_set_absolute_origin(
								vt100_parser->screen) ;
							/*
							 * cursor position is reset
							 * (the same behavior of xterm 4.0.3,
							 * kterm 6.2.0 or so)
							 */
							ml_screen_goto( vt100_parser->screen ,
								0 , 0) ;
						}
						else if( ps[count] == 7)
						{
							/* "CSI ? 7 l" auto wrap */

							ml_screen_set_auto_wrap(
								vt100_parser->screen , 0) ;
						}
					#if  0
						else if( ps[count] == 8)
						{
							/* "CSI ? 8 l" auto repeat */
						}
					#endif
					#if  0
						else if( ps[count] == 9)
						{
							/* "CSI ? 9 l" X10 mouse reporting */
						}
					#endif
						else if( ps[count] == 25)
						{
							/* "CSI ? 25 l" */

							ml_screen_cursor_invisible(
								vt100_parser->screen) ;
						}
					#if  0
						else if( ps[count] == 35)
						{
							/* "CSI ? 35 l" shift keys */
						}
					#endif
					#if  0
						else if( ps[count] == 40)
						{
							/* "CSI ? 40 l" 80 <-> 132 */
						}
					#endif
						else if( ps[count] == 47)
						{
							/* "CSI ? 47 l" Use normal screen buffer */

							if( use_alt_buffer)
							{
								ml_screen_use_normal_edit(
									vt100_parser->screen) ;
							}
						}
						else if( ps[count] == 66)
						{
							/* "CSI ? 66 l" application key pad */

							vt100_parser->is_app_keypad = 0 ;
						}
					#if  0
						else if( ps[count] == 67)
						{
							/* "CSI ? 67 l" have back space */
						}
					#endif
						else if( ps[count] == 69)
						{
							/* "CSI ? 69 l" */

							ml_screen_set_use_margin(
								vt100_parser->screen , 0) ;
						}
						else if( ps[count] == 80)
						{
							/* "CSI ? 80 l" */

							vt100_parser->sixel_scrolling = 1 ;
						}
						else if( ps[count] == 1000 ||
							ps[count] == 1002 || ps[count] == 1003)
						{
							/*
							 * "CSI ? 1000 l" "CSI ? 1002 l"
							 * "CSI ? 1003 l"
							 */

							set_mouse_report( vt100_parser , 0) ;
						}
					#if  0
						else if( ps[count] == 1001)
						{
							/* "CSI ? 1001 l" X11 mouse highlighting */
						}
					#endif
						else if( ps[count] == 1004)
						{
							/* "CSI ? 1004 h" */

							vt100_parser->want_focus_event = 0 ;
						}
						else if( ps[count] == 1005 ||
						         ps[count] == 1006 ||
							 ps[count] == 1015)
						{
							/*
							 * "CSI ? 1005 l"
							 * "CSI ? 1006 l"
							 * "CSI ? 1015 l"
							 */
							vt100_parser->ext_mouse_mode = 0 ;
						}
					#if  0
						else if( ps[count] == 1010)
						{
							/*
							 * "CSI ? 1010 l"
							 * scroll to bottom on tty output inhibit
							 */
						}
					#endif
					#if  0
						else if( ps[count] == 1011)
						{
							/*
							 * "CSI ? 1011 l"
							 * scroll to bottom on key press
							 */
						}
					#endif
						else if( ps[count] == 1042)
						{
							/* "CSI ? 1042 l" */
							config_protocol_set_simple( vt100_parser ,
								"use_urgent_bell" , "false") ;
						}
						else if( ps[count] == 1047)
						{
							/* "CSI ? 1047 l" */

							if( use_alt_buffer &&
							    ml_screen_is_alternative_edit(
								vt100_parser->screen))
							{
								clear_display_all( vt100_parser) ;
								ml_screen_use_normal_edit(
									vt100_parser->screen) ;
							}
						}
						else if( ps[count] == 1048)
						{
							/* "CSI ? 1048 l" */

							if( use_alt_buffer)
							{
								restore_cursor( vt100_parser) ;
							}
						}
						else if( ps[count] == 1049)
						{
							/* "CSI ? 1049 l" */

							if( use_alt_buffer &&
							    ml_screen_is_alternative_edit(
								vt100_parser->screen))
							{
								ml_screen_use_normal_edit(
									vt100_parser->screen) ;
								restore_cursor( vt100_parser) ;
							}
						}
						else if( ps[count] == 2004)
						{
							/* "CSI ? 2004 l" */

							vt100_parser->is_bracketed_paste_mode = 0 ;
						}
						else if( ps[count] == 7727)
						{
							/* "CSI ? 7727 l" */

							vt100_parser->is_app_escape = 0 ;
						}
						else if( ps[count] == 8428)
						{
							/* "CSI ? 8428 l" (RLogin original) */

							vt100_parser->col_size_of_width_a = 2 ;
						}
						else if( ps[count] == 8800)
						{
							vt100_parser->unicode_policy &=
								~USE_UNICODE_DRCS ;
						}
						else if( ps[count] == 9500)
						{
							/* "CSI ? 9500 l" */

							config_protocol_set_simple( vt100_parser ,
								"use_local_echo" , "false") ;
						}
					#ifdef  DEBUG
						else
						{
							debug_print_unknown( "ESC [ ? %d l\n" ,
								ps[count]) ;
						}
					#endif
					}
				}
				else if( *str_p == 'n')
				{
					if( ps[0] == 8840)
					{
						/* "CSI ? 8840 n" (TNREPTAMB) */

						char  msg[] = "\x1b[?884Xn" ;

						msg[6] = vt100_parser->col_size_of_width_a + 0x30 ;
						ml_write_to_pty( vt100_parser->pty , msg ,
							sizeof(msg) - 1) ;
					}
				}
			#if  0
				else if( *str_p == 'r')
				{
					/* "CSI ? r"  Restore DEC Private Mode */
				}
			#endif
			#if  0
				else if( *str_p == 's')
				{
					/* "CSI ? s" Save DEC Private Mode */
				}
			#endif
				/* Other final character */
				else if( 0x40 <= *str_p && *str_p <= 0x7e)
				{
				#ifdef  DEBUG
					debug_print_unknown( "ESC [ %c\n" , *str_p) ;
				#endif
				}
				else
				{
					/* not VT100 control sequence. */

				#ifdef  ESCSEQ_DEBUG
					kik_msg_printf( "=> not VT100 control sequence.\n") ;
				#endif

					return  1 ;
				}
			}
			else if( pre_ch == '!')
			{
				if( *str_p == 'p')
				{
					/* "CSI ! p" Soft terminal reset */

					soft_reset( vt100_parser) ;
				}
			}
			else if( pre_ch == '$')
			{
				int  count ;

				if( *str_p == 'x')
				{
					int  tmp ;

					/* Move Pc to the end. */
					tmp = ps[0] ;
					memmove( ps , ps + 1 , sizeof(ps[0]) * 4) ;
					ps[4] = tmp ;
				}

				/* Set the default values to the 0-3 parameters. */
				for( count = 0 ; count < 4 ; count++)
				{
					if( count >= num || ps[count] <= 0)
					{
						if( count == 2)
						{
							ps[count] = ml_screen_get_rows(
								    vt100_parser->screen) ;
						}
						else if( count == 3)
						{
							ps[count] = ml_screen_get_cols(
								    vt100_parser->screen) ;
						}
						else
						{
							ps[count] = 1 ;
						}
					}
				}

				if( ps[3] >= ps[1] && ps[2] >= ps[0])
				{
					if( *str_p == 'z')
					{
						/* "CSI ... $ z" DECERA */
						ml_screen_erase_area(
							vt100_parser->screen ,
							ps[1] - 1 , ps[0] - 1 ,
							ps[3] - ps[1] + 1 ,
							ps[2] - ps[0] + 1) ;
					}
					else if( *str_p == 'v' && num >= 7)
					{
						/* "CSI ... $ v" DECCRA */
						ml_screen_copy_area(
							vt100_parser->screen ,
							ps[1] - 1 , ps[0] - 1 ,
							ps[3] - ps[1] + 1 ,
							ps[2] - ps[0] + 1 ,
							ps[6] - 1 , ps[5] - 1) ;
					}
					else if( *str_p == 'x' && num >= 1)
					{
						/* "CSI ... $ x" DECFRA */
						ml_screen_fill_area(
							vt100_parser->screen ,
							ps[4] ,
							ps[1] - 1 , ps[0] - 1 ,
							ps[3] - ps[1] + 1 ,
							ps[2] - ps[0] + 1) ;
					}
				}
			}
			else if( pre_ch == '<')
			{
				/* Teraterm compatible IME control sequence */

				if( *str_p == 'r')
				{
					/* Restore IME state */
					if( vt100_parser->im_is_active !=
						im_is_active( vt100_parser))
					{
						switch_im_mode( vt100_parser) ;
					}
				}
				else if( *str_p == 's')
				{
					/* Save IME state */

					vt100_parser->im_is_active = im_is_active( vt100_parser) ;
				}
				else if( *str_p == 't')
				{
					/* ps[0] = 0 (Close), ps[0] = 1 (Open) */

					if( ps[0] != im_is_active( vt100_parser))
					{
						switch_im_mode( vt100_parser) ;
					}
				}
			}
			else if( pre_ch == '>')
			{
				if( *str_p == 'c')
				{
					/* "CSI > c" Secondary DA */

					send_device_attributes( vt100_parser->pty , 2) ;
				}
				else if( *str_p == 'm')
				{
					/* "CSI > m" */

					if( ps[0] == -1)
					{
						/* reset to initial value. */
						set_modkey_mode( vt100_parser , 1 , 2) ;
						set_modkey_mode( vt100_parser , 2 , 2) ;
						set_modkey_mode( vt100_parser , 4 , 0) ;
					}
					else
					{
						if( num == 1 || ps[1] == -1)
						{
							if( ps[0] == 1 || /* modifyCursorKeys */
							    ps[0] == 2)   /* modifyFunctionKeys */
							{
								/* The default is 2. */
								ps[1] = 2 ;
							}
							else /* if( ps[0] == 4) */
							{
								/*
								 * modifyOtherKeys
								 * The default is 0.
								 */
								ps[1] = 0 ;
							}
						}
						
						set_modkey_mode( vt100_parser , ps[0] , ps[1]) ;
					}
				}
				else if( *str_p == 'n')
				{
					/* "CSI > n" */

					if( num == 1)
					{
						if( ps[0] == -1)
						{
							ps[0] = 2 ;
						}

						/* -1: don't send modifier key code. */
						set_modkey_mode( vt100_parser , ps[0] , -1) ;
					}
				}
				else if( *str_p == 'p')
				{
					/* "CSI > p" pointer mode */

					if( HAS_XTERM_LISTENER(vt100_parser,hide_cursor))
					{
						(*vt100_parser->xterm_listener->hide_cursor)(
							vt100_parser->xterm_listener->self ,
							ps[0] == 2 ? 1 : 0) ;
					}
				}
				else
				{
					/*
					 * "CSI > T", "CSI > c", "CSI > t"
					 */
				}
			}
			else if( pre_ch == ' ')
			{
				if( *str_p == 'q')
				{
					/* CSI SP q */

					if( ps[0] < 2)
					{
						config_protocol_set_simple( vt100_parser ,
							"blink_cursor" , "true") ;
					}
					else if( ps[0] == 2)
					{
						config_protocol_set_simple( vt100_parser ,
							"blink_cursor" , "false") ;
					}
				}
				else if( *str_p == '@')
				{
					/* CSI SP @ (SL) */
					ml_screen_scroll_leftward( vt100_parser->screen , ps[0]) ;
				}
				else if( *str_p == 'A')
				{
					/* CSI SP A (SR) */
					ml_screen_scroll_rightward( vt100_parser->screen , ps[0]) ;
				}
				else
				{
					/*
					 * "CSI SP t"(DECSWBV), "CSI SP u"(DECSMBV)
					 */
				}
			}
			else if( pre_ch == '*')
			{
				if( ps[0] == -1)
				{
					ps[0] = 0 ;
				}

				if( *str_p == 'z')
				{
					invoke_macro( vt100_parser , ps[0]) ;
				}
			}
			else if( pre_ch == '\'')
			{
				if( *str_p == '|')
				{
					/* DECRQLP */

					request_locator( vt100_parser) ;
				}
				else if( *str_p == '{')
				{
					/* DECSLE */
					int  count ;

					for( count = 0 ; count < num ; count++)
					{
						if( ps[count] == 1)
						{
							vt100_parser->locator_mode |=
								LOCATOR_BUTTON_DOWN ;
						}
						else if( ps[count] == 2)
						{
							vt100_parser->locator_mode &=
								~LOCATOR_BUTTON_DOWN ;
						}
						else if( ps[count] == 3)
						{
							vt100_parser->locator_mode |=
								LOCATOR_BUTTON_UP ;
						}
						else if( ps[count] == 4)
						{
							vt100_parser->locator_mode &=
								~LOCATOR_BUTTON_UP ;
						}
						else
						{
							vt100_parser->locator_mode &=
								~(LOCATOR_BUTTON_UP|
								  LOCATOR_BUTTON_DOWN) ;
						}
					}
				}
				else if( *str_p == 'w')
				{
					/* DECEFR Filter Rectangle */

					if( num == 4)
					{
					#if  0
						if( top > ps[0] || left > ps[1] ||
						    bottom < ps[2] || right < ps[3])
						{
							/*
							 * XXX
							 * An outside rectangle event should
							 * be sent immediately.
							 */
						}
					#endif

						vt100_parser->loc_filter.top = ps[0] ;
						vt100_parser->loc_filter.left = ps[1] ;
						vt100_parser->loc_filter.bottom = ps[2] ;
						vt100_parser->loc_filter.right = ps[3] ;
					}

					vt100_parser->locator_mode |= LOCATOR_FILTER_RECT ;
				}
				else if( *str_p == 'z')
				{
					/* DECELR */

					vt100_parser->locator_mode &= ~LOCATOR_FILTER_RECT ;
					memset( &vt100_parser->loc_filter , 0 ,
						sizeof(vt100_parser->loc_filter)) ;
					if( ps[0] <= 0)
					{
						if( vt100_parser->mouse_mode >=
							LOCATOR_CHARCELL_REPORT)
						{
							set_mouse_report( vt100_parser , 0) ;
						}
					}
					else
					{
						vt100_parser->locator_mode |=
							ps[0] == 2 ? LOCATOR_ONESHOT : 0 ;

						set_mouse_report( vt100_parser ,
							(num == 1 || ps[1] <= 0 || ps[1] == 2) ?
								LOCATOR_CHARCELL_REPORT :
								LOCATOR_PIXEL_REPORT) ;
					}
				}
			}
			/* Other pre_ch(0x20-0x2f or 0x3a-0x3f) */
			else if( pre_ch)
			{
				/*
				 * "CSI " p"(DECSCL), "CSI " q"(DECSCA)
				 * "CSI ' {"(DECSLE), "CSI ' |"(DECRQLP)
				 * etc
				 */

			#ifdef  DEBUG
				debug_print_unknown( "ESC [ %c %c\n" , pre_ch , *str_p) ;
			#endif
			}
			else if( *str_p == '@')
			{
				/* "CSI @" insert blank chars */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				/* inserting ps[0] blank characters. */
				ml_screen_insert_blank_chars( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'A' || *str_p == 'k')
			{
				/* "CSI A" = CUU , "CSI k" = VPB */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_go_upward( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'B' || *str_p == 'e')
			{
				/* "CSI B" = CUD , "CSI e" = VPR */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_go_downward( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'C' || *str_p == 'a')
			{
				/* "CSI C" = CUF , "CSI a" = HPR */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_go_forward( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'D' || *str_p == 'j')
			{
				/* "CSI D" = CUB , "CSI j" = HPB */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_go_back( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'E')
			{
				/* "CSI E" down and goto first column */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_go_downward( vt100_parser->screen , ps[0]) ;
				ml_screen_goto_beg_of_line( vt100_parser->screen) ;
			}
			else if( *str_p == 'F')
			{
				/* "CSI F" up and goto first column */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_go_upward( vt100_parser->screen , ps[0]) ;
				ml_screen_goto_beg_of_line( vt100_parser->screen) ;
			}
			else if( *str_p == 'G' || *str_p == '`')
			{
				/*
				 * "CSI G"(CHA) "CSI `"(HPA)
				 * cursor position absolute.
				 */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_go_horizontally( vt100_parser->screen , ps[0] - 1) ;
			}
			else if( *str_p == 'H' || *str_p == 'f')
			{
				/* "CSI H" "CSI f" */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				if( num <= 1 || ps[1] <= 0)
				{
					ps[1] = 1 ;
				}

				ml_screen_goto( vt100_parser->screen , ps[1] - 1 , ps[0] - 1) ;
			}
			else if( *str_p == 'I')
			{
				/* "CSI I" cursor forward tabulation (CHT) */

				if( ps[0] == -1)
				{
					/*
					 * "CSI 0 I" => No tabulation.
					 * "CSI I" => 1 taburation.
					 */
					ps[0] = 1 ;
				}

				ml_screen_vertical_forward_tabs( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'J')
			{
				/* "CSI J" Erase in Display */

				if( ps[0] <= 0)
				{
					ml_screen_clear_below( vt100_parser->screen) ;
				}
				else if( ps[0] == 1)
				{
					ml_screen_clear_above( vt100_parser->screen) ;
				}
				else if( ps[0] == 2)
				{
					clear_display_all( vt100_parser) ;
				}
			}
			else if( *str_p == 'K')
			{
				/* "CSI K" Erase in Line */

				if( ps[0] <= 0)
				{
					ml_screen_clear_line_to_right( vt100_parser->screen) ;
				}
				else if( ps[0] == 1)
				{
					ml_screen_clear_line_to_left( vt100_parser->screen) ;
				}
				else if( ps[0] == 2)
				{
					clear_line_all( vt100_parser) ;
				}
			}
			else if( *str_p == 'L')
			{
				/* "CSI L" */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_insert_new_lines( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'M')
			{
				/* "CSI M" */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_delete_lines( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'P')
			{
				/* "CSI P" delete chars */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_delete_cols( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'S')
			{
				/* "CSI S" scroll up */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_scroll_upward( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'T')
			{
				/* "CSI T" scroll down */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_scroll_downward( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'X')
			{
				/* "CSI X" erase characters */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_clear_cols( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'Z')
			{
				/* "CSI Z" cursor backward tabulation (CBT) */

				if( ps[0] == -1)
				{
					/*
					 * "CSI 0 Z" => No tabulation.
					 * "CSI Z" => 1 taburation.
					 */
					ps[0] = 1 ;
				}

				ml_screen_vertical_backward_tabs( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'b')
			{
				/* "CSI b" repeat the preceding graphic character(REP) */

				if( vt100_parser->w_buf.last_ch)
				{
					int  count ;

					if( ps[0] <= 0)
					{
						ps[0] = 1 ;
					}

					for( count = 0 ; count < ps[0] ; count++)
					{
						(*vt100_parser->w_buf.output_func)(
							vt100_parser->screen ,
							vt100_parser->w_buf.last_ch , 1) ;
					}

					vt100_parser->w_buf.last_ch = NULL ;
				}
			}
			else if( *str_p == 'c')
			{
				/* "CSI c" Primary DA */

				send_device_attributes( vt100_parser->pty , 1) ;
			}
			else if( *str_p == 'd')
			{
				/* "CSI d" line position absolute(VPA) */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_go_vertically( vt100_parser->screen , ps[0] - 1) ;
			}
			else if( *str_p == 'g')
			{
				/* "CSI g" tab clear */

				if( ps[0] <= 0)
				{
					ml_screen_clear_tab_stop( vt100_parser->screen) ;
				}
				else if( ps[0] == 3)
				{
					ml_screen_clear_all_tab_stops( vt100_parser->screen) ;
				}
			}
			else if( *str_p == 'l')
			{
				/* "CSI l" */
				int  count ;

				for( count = 0 ; count < num ; count++)
				{
					if( ps[count] == 4)
					{
						/* replace mode */

						vt100_parser->w_buf.output_func =
							ml_screen_overwrite_chars ;
					}
				}
			}
			else if( *str_p == 'h')
			{
				/* "CSI h" */
				int  count ;

				for( count = 0 ; count < num ; count++)
				{
					if( ps[count] == 4)
					{
						/* insert mode */

						vt100_parser->w_buf.output_func =
							ml_screen_insert_chars ;
					}
				#ifdef  DEBUG
					else
					{
						debug_print_unknown( "ESC [ %d h\n" ,
							ps[count]) ;
					}
				#endif
				}
			}
			else if( *str_p == 'm')
			{
				/* "CSI m" */
				int  count ;

				for( count = 0 ; count < num ; )
				{
					int  proceed ;

					if( ( proceed = change_char_fine_color( vt100_parser ,
								ps + count , num - count)))
					{
						count += proceed ;
					}
					else
					{
						if( ps[count] <= 0)
						{
							ps[count] = 0 ;
						}

						change_char_attr( vt100_parser , ps[count++]) ;
					}
				}
			}
			else if( *str_p == 'n')
			{
				/* "CSI n" device status report */

				if( ps[0] == 5)
				{
					ml_write_to_pty( vt100_parser->pty , "\x1b[0n" , 4) ;
				}
				else if( ps[0] == 6)
				{
					char  seq[4 + DIGIT_STR_LEN(u_int) * 2 + 1] ;

					sprintf( seq , "\x1b[%d;%dR" ,
						ml_screen_cursor_relative_row(
							vt100_parser->screen) + 1 ,
						ml_screen_cursor_relative_col(
							vt100_parser->screen) + 1) ;

					ml_write_to_pty( vt100_parser->pty , seq , strlen( seq)) ;
				}
			}
			else if( *str_p == 'r')
			{
				/* "CSI r" set scroll region */

				if( ps[0] <= 0)
				{
					ps[0] = 0 ;
				}

				if( num <= 1 || ps[1] <= 0)
				{
					ps[1] = 0 ;
				}

				if( ml_screen_set_scroll_region( vt100_parser->screen ,
						ps[0] - 1 , ps[1] - 1))
				{
					ml_screen_goto( vt100_parser->screen , 0 , 0) ;
				}
			}
			else if( *str_p == 's')
			{
				/* "CSI s" */

				if( num <= 1 || ps[1] <= 0)
				{
					ps[1] = ml_screen_get_cols( vt100_parser->screen) ;
				}

				if( ! ml_screen_set_margin( vt100_parser->screen ,
						ps[0] <= 0 ? 0 : ps[0] - 1 , ps[1] - 1)
				    && num == 1 && ps[0] == -1)
				{
					save_cursor( vt100_parser) ;
				}
			}
			else if( *str_p == 't')
			{
				/* "CSI t" */

				if( num == 3)
				{
					if( ps[0] == 4)
					{
						resize( vt100_parser , ps[2] , ps[1] , 0) ;
					}
					else if( ps[0] == 8)
					{
						resize( vt100_parser , ps[2] , ps[1] , 1) ;
					}
				}
				else if( num == 2)
				{
					if( ps[0] == 22)
					{
						if( ps[1] == 0 || ps[1] == 1)
						{
							push_to_saved_names(
								&vt100_parser->saved_icon_names ,
								vt100_parser->icon_name) ;
						}

						if( ps[1] == 0 || ps[1] == 2)
						{
							push_to_saved_names(
								&vt100_parser->saved_win_names ,
								vt100_parser->win_name) ;
						}
					}
					else if( ps[0] == 23)
					{
						if( ( ps[1] == 0 || ps[1] == 1) &&
						    vt100_parser->saved_icon_names.num > 0)
						{
							set_icon_name( vt100_parser ,
								pop_from_saved_names(
								&vt100_parser->saved_icon_names)) ;
						}

						if( ( ps[1] == 0 || ps[1] == 2) &&
						    vt100_parser->saved_win_names.num > 0)
						{
							set_window_name( vt100_parser ,
								pop_from_saved_names(
								&vt100_parser->saved_win_names)) ;
						}
					}
				}
				else
				{
					if( ps[0] == 14)
					{
						report_window_size( vt100_parser , 0) ;
					}
					else if( ps[0] == 18)
					{
						report_window_size( vt100_parser , 1) ;
					}
				}
			}
			else if( *str_p == 'u')
			{
				/* "CSI u" */

				restore_cursor( vt100_parser) ;
			}
			else if( *str_p == 'x')
			{
				/* "CSI x" request terminal parameters */

				/* XXX the same as rxvt */

				if( ps[0] <= 0)
				{
					ps[0] = 0 ;
				}

				if( ps[0] == 0 || ps[0] == 1)
				{
					char seq[] = "\x1b[X;1;1;112;112;1;0x" ;

					/* '+ 0x30' lets int to char */
					seq[2] = ps[0] + 2 + 0x30 ;

					ml_write_to_pty( vt100_parser->pty ,
						seq , sizeof( seq) - 1) ;
				}
			}
		#if  0
			else if( *str_p == '^')
			{
				/* "CSI ^" initiate hilite mouse tracking. */
			}
		#endif
			/* Other final character */
			else if( 0x40 <= *str_p && *str_p <= 0x7e)
			{
			#ifdef  DEBUG
				debug_print_unknown( "ESC [ %c\n" , *str_p) ;
			#endif
			}
			else
			{
				/* not VT100 control sequence. */

			#ifdef  ESCSEQ_DEBUG
				kik_msg_printf( "=> not VT100 control sequence.\n") ;
			#endif

				return  1 ;
			}
		}
		else if( *str_p == ']')
		{
			/* "ESC ]" (OSC) */

			char  digit[DIGIT_STR_LEN(int) + 1] ;
			int  count ;
			int  ps ;
			u_char *  pt ;

			if( ! inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0))
			{
				return  0 ;
			}

			for( count = 0 ; count < DIGIT_STR_LEN(int) ; count++)
			{
				if( '0' <= *str_p && *str_p <= '9')
				{
					digit[count] = *str_p ;

					if( ! inc_str_in_esc_seq( vt100_parser->screen ,
							&str_p , &left , 0))
					{
						return  0 ;
					}
				}
				else
				{
					break ;
				}
			}

			if( count > 0 && *str_p == ';')
			{
				digit[count] = '\0' ;

				/* if digit is illegal , ps is set 0. */
				ps = atoi( digit) ;
			#ifdef  MAX_PS_DIGIT
				if( ps > MAX_PS_DIGIT)
				{
					ps = MAX_PS_DIGIT ;
				}
			#endif

				if( ! inc_str_in_esc_seq( vt100_parser->screen ,
						&str_p , &left , 1))
				{
					return  0 ;
				}
			}
			else
			{
				/* Illegal OSC format */
				ps = -1 ;
			}

			pt = str_p ;
			/*
			 * +1 in case str_p[left - vt100_parser->r_buf.new_len] points
			 * "\\" of "\x1b\\".
			 */
			if( left > vt100_parser->r_buf.new_len + 1)
			{
				str_p += (left - vt100_parser->r_buf.new_len - 1) ;
				left = vt100_parser->r_buf.new_len + 1 ;
			}

			if( ! get_pt_in_esc_seq( &str_p , &left , 0 , 1))
			{
				if( left == 0)
				{
					return  0 ;
				}
			#ifdef  DEBUG
				else
				{
					debug_print_unknown( "ESC ] %d ; ???\n" , ps) ;
				}
			#endif
			}
			else if( ps == 0)
			{
				/* "OSC 0" change icon name and window title */

				if( *pt != '\0')
				{
					set_window_name( vt100_parser , strdup( pt)) ;
					set_icon_name( vt100_parser , strdup( pt)) ;
				}
			}
			else if( ps == 1)
			{
				/* "OSC 1" change icon name */

				if( *pt != '\0')
				{
					set_icon_name( vt100_parser , strdup( pt)) ;
				}
			}
			else if( ps == 2)
			{
				/* "OSC 2" change window title */

				if( *pt != '\0')
				{
					set_window_name( vt100_parser , strdup( pt)) ;
				}
			}
			else if( ps == 4)
			{
				/* "OSC 4" change 256 color */

				change_color_rgb( vt100_parser , pt) ;
			}
			else if( ps == 5)
			{
				/* "OSC 5" change colorBD/colorUL */

				change_special_color( vt100_parser , pt) ;
			}
			else if( ps == 10)
			{
				/* "OSC 10" fg color */

				if( strcmp( pt , "?") == 0)	/* ?:query rgb */
				{
					get_rgb( vt100_parser , ps , ML_FG_COLOR) ;
				}
				else
				{
					config_protocol_set_simple( vt100_parser ,
						"fg_color" , pt) ;
				}
			}
			else if( ps == 11)
			{
				/* "OSC 11" bg color */

				if( strcmp( pt , "?") == 0)	/* ?:query rgb */
				{
					get_rgb( vt100_parser , ps , ML_BG_COLOR) ;
				}
				else
				{
					config_protocol_set_simple( vt100_parser ,
						"bg_color" , pt) ;
				}
			}
			else if( ps == 12)
			{
				/* "OSC 12" cursor bg color */

				if( strcmp( pt , "?") != 0)	/* ?:query rgb */
				{
					config_protocol_set_simple( vt100_parser ,
						"cursor_bg_color" , pt) ;
				}
			}
			else if( ps == 20)
			{
				/* "OSC 20" (Eterm compatible) */

				/* edit commands */
				char *  p ;

				/* XXX discard all adjust./op. settings.*/
				/* XXX may break multi-byte char string. */
				if( ( p = strchr( pt , ';')))
				{
					*p = '\0';
				}
				if( ( p = strchr( pt , ':')))
				{
					*p = '\0';
				}

				if( *pt == '\0')
				{
					/*
					 * Do not change current edit but
					 * alter diaplay setting.
					 * XXX nothing can be done for now.
					 */

					return  0 ;
				}

				config_protocol_set_simple( vt100_parser , "wall_picture" , pt) ;
			}
		#if  0
			else if( ps == 46)
			{
				/* "OSC 46" change log file */
			}
			else if( ps == 50)
			{
				/* "OSC 50" set font */
			}
		#endif
			else if( ps == 52)
			{
				set_selection( vt100_parser , pt) ;
			}
		#if  0
			else if( ps == 110)
			{
				config_protocol_set_simple( vt100_parser ,
						"fg_color" , "black") ;
			}
			else if( ps == 111)
			{
				config_protocol_set_simple( vt100_parser ,
						"bg_color" , "white") ;
			}
			else if( ps == 112)
			{
				config_protocol_set_simple( vt100_parser ,
						"cursor_bg_color" , "black") ;
			}
		#endif
		#ifdef  SUPPORT_ITERM2_OSC1337
			else if( ps == 1337)
			{
				iterm2_proprietary_set( vt100_parser , pt) ;
			}
		#endif
			else if( ps == 5379)
			{
				/* "OSC 5379" set */

				config_protocol_set( vt100_parser , pt , 0) ;
			}
			else if( ps == 5380)
			{
				/* "OSC 5380" get */

				config_protocol_get( vt100_parser , pt , 0 , NULL) ;
			}
			else if( ps == 5381)
			{
				/* "OSC 5381" get(menu) */

				config_protocol_get( vt100_parser , pt , 1 , NULL) ;
			}
			else if( ps == 5383)
			{
				/* "OSC 5383" set&save */

				config_protocol_set( vt100_parser , pt , 1) ;
			}
		#ifdef  DEBUG
			else if( ps == -1)
			{
				debug_print_unknown( "ESC ] %s\n" , pt) ;
			}
			else
			{
				debug_print_unknown( "ESC ] %d ; %s\n" , ps , pt) ;
			}
		#endif
		}
		else if( *str_p == 'P')
		{
			/* "ESC P" DCS */

			u_char *  dcs_beg ;
		#ifndef  NO_IMAGE
			char *  path ;
		#endif

			while(1)
			{
				/* ESC P ... */
				dcs_beg = str_p - 1 ;
				break ;

			parse_dcs:
				/* 0x90 ... */
				dcs_beg = str_p ;
				break ;
			}

			do
			{
				if( ! increment_str( &str_p , &left))
				{
					return  0 ;
				}
			}
			while( *str_p == ';' || ('0' <= *str_p && *str_p <= '9')) ;

		#ifndef  NO_IMAGE
			if( /* sixel */
			    ( *str_p == 'q' &&
			      ( path = get_home_file_path( "" ,
						ml_pty_get_slave_name( vt100_parser->pty) + 5 ,
						"six"))) ||
			    /* ReGIS */
			    ( *str_p == 'p' &&
			      ( path = get_home_file_path( "" ,
						ml_pty_get_slave_name( vt100_parser->pty) + 5 ,
						"rgs"))))
			{
				int  is_end ;
				FILE *  fp ;

				if( left > 2 && *(str_p + 1) == '\0')
				{
					fp = fopen( path , "a") ;
					is_end = *(str_p + 2) ;
					/*
					 * dcs_beg will equal to str_p after str_p is
					 * incremented by the following increment_str().
					 */
					dcs_beg = (str_p += 2) + 1 ;
					left -= 2 ;
				}
				else
				{
					char *  format ;
					ml_color_t  color ;
					u_int8_t  red ;
					u_int8_t  green ;
					u_int8_t  blue ;

					fp = fopen( path , "w") ;
					is_end = 0 ;

					if( strcmp( path + strlen(path) - 4 , ".rgs") == 0)
					{
						/* Clear background by ML_BG_COLOR */

						/* 13 + 3*3 + 1 = 23 */
						format = "S(I(R%dG%dB%d))S(E)" ;
						color = ML_BG_COLOR ;
					}
					else if( strcmp( path + strlen(path) - 4 , ".six") == 0)
					{
						/*
						 * Set ML_FG_COLOR to the default value of
						 * the first entry of the sixel palette.
						 */

						/* 7 + 3*3 + 1 = 17 */
						format = "#0;2;%d;%d;%d" ;
						color = ML_FG_COLOR ;
					}
					else
					{
						format = NULL ;
					}

					if( format &&
					    HAS_XTERM_LISTENER(vt100_parser,get_rgb) &&
					    (*vt100_parser->xterm_listener->get_rgb)(
						vt100_parser->xterm_listener->self ,
						&red , &green , &blue , color))
					{
						char  color_seq[23] ;

						if( color == ML_FG_COLOR)
						{
							/* sixel */
							red = red * 100 / 255 ;
							green = green * 100 / 255 ;
							blue = blue * 100 / 255 ;
						}

						fwrite( dcs_beg , 1 ,
							str_p - dcs_beg + 1 , fp) ;
						sprintf( color_seq , format ,
							red , green , blue) ;
						fwrite( color_seq , 1 ,
							strlen(color_seq) , fp) ;
						dcs_beg = str_p + 1 ;
					}

					/*
					 * +1 in case str_p[left - vt100_parser->r_buf.new_len]
					 * points "\\" of "\x1b\\".
					 */
					if( left > vt100_parser->r_buf.new_len + 1)
					{
						str_p += (left - vt100_parser->r_buf.new_len - 1) ;
						left = vt100_parser->r_buf.new_len + 1 ;
					}
				}

				while( 1)
				{
					if( ! increment_str( &str_p , &left))
					{
						if( is_end == 2)
						{
							left ++ ;
							break ;
						}

						if( vt100_parser->logging_vt_seq &&
						    use_ttyrec_format)
						{
							fclose( fp) ;
							free( path) ;

							return  0 ;
						}

						fwrite( dcs_beg , 1 , str_p - dcs_beg + 1 , fp) ;

						vt100_parser->r_buf.left = 0 ;
						if( ! receive_bytes( vt100_parser))
						{
							fclose( fp) ;
							memcpy( vt100_parser->r_buf.chars ,
								strcmp( path + strlen(path) - 4 ,
									".six") == 0 ?
									"\x1bPq\0" :
									"\x1bPp\0" ,
									4) ;
							free( path) ;
							vt100_parser->r_buf.chars[4] = is_end ;
							vt100_parser->r_buf.filled_len =
								vt100_parser->r_buf.left = 5 ;

							/* No more data in pty. */
							vt100_parser->yield = 1 ;

							return  0 ;
						}

						dcs_beg = str_p = CURRENT_STR_P(vt100_parser) ;
						left = vt100_parser->r_buf.left ;
					}

					if( is_end == 2)
					{
						u_char *  p ;

						p = str_p ;

						/* \x38 == '8' */
						if( strncmp( p , "\x1d\x38k @\x1f" , 6) == 0)
						{
							/* XXX Hack for biplane.six */
							p += 6 ;
						}

						if( *p == 0x90 ||
						    /* XXX If left == 0 and next char is 'P'... */
						    ( *p == CTL_ESC && left > p - str_p + 1 &&
						      *(p + 1) == 'P') )
						{
							/* continued ... */
							is_end = 0 ;
						}
						else
						{
							str_p -- ;
							left ++ ;
							break ;
						}
					}
					/*
					 * 0x9c is regarded as ST here, because sixel sequence
					 * unuses it certainly.
					 */
					else if( *str_p == 0x9c)
					{
						is_end = 2 ;
					}
					else if( *str_p == CTL_ESC)
					{
						is_end = 1 ;
					}
					else if( is_end == 1)
					{
						if( *str_p == '\\')
						{
							is_end = 2 ;
						}
						else
						{
							is_end = 0 ;
						}
					}
				}

				fwrite( dcs_beg , 1 , str_p - dcs_beg + 1 , fp) ;
				fclose( fp) ;

				if( strcmp( path + strlen(path) - 4 , ".six") == 0)
				{
					show_picture( vt100_parser , path ,
						0 , 0 , 0 , 0 , 0 , 0 ,
						(! vt100_parser->sixel_scrolling &&
						check_sixel_anim( vt100_parser->screen ,
							str_p , left)) ? 2 : 1) ;
				}
				else
				{
					/* ReGIS */
					int  orig_flag ;

					orig_flag = vt100_parser->sixel_scrolling ;
					vt100_parser->sixel_scrolling = 0 ;
					show_picture( vt100_parser , path ,
						0 , 0 , 0 , 0 , 0 , 0 , 1) ;
					vt100_parser->sixel_scrolling = orig_flag ;
				}

				free( path) ;
			}
			else
		#endif  /* NO_IMAGE */
			if( *str_p == '{')
			{
				/* DECDLD */

				u_char *  pt ;
				ml_drcs_font_t *  font ;
				int  num ;
				u_char *  p ;
				int  ps[8] ;
				int  idx ;
				int  is_end ;
				u_int  width ;
				u_int  height ;

				while( 1)
				{
					if( *str_p == 0x9c ||
					    ( *str_p == CTL_ESC && left > 1 &&
					      *(str_p + 1) == '\\'))
					{
						*str_p = '\0' ;
						increment_str( &str_p , &left) ;
						break ;
					}
					else if( ! increment_str( &str_p , &left))
					{
						return  0 ;
					}
				}

				if( *dcs_beg == '\x1b')
				{
					pt = dcs_beg + 2 ;
				}
				else /* if( *dcs_beg == '\x90') */
				{
					pt = dcs_beg + 1 ;
				}

				for( num = 0 ; num < 8 ; num ++)
				{
					p = pt ;

					while( '0' <= *pt && *pt <= '9')
					{
						pt ++ ;
					}

					if( *pt == ';' || *pt == '{')
					{
						*(pt ++) = '\0' ;
					}
					else
					{
						break ;
					}

					ps[num] = *p ? atoi( p) : 0 ;
				}

				if( *pt == ' ')
				{
					/* ESC ( SP Ft */
					pt ++ ;
				}

				ml_drcs_select( vt100_parser->drcs) ;

				if( num != 8)
				{
					/* illegal format */
				}
				else if( *pt == '\0')
				{
					if( ps[2] == 2)
					{
						ml_drcs_final_full() ;
					}
				}
				else if( 0x30 <= *pt && *pt <= 0x7e)
				{
					mkf_charset_t  cs ;

					if( ps[7] == 0)
					{
						idx = ps[1] == 0 ? 1 : ps[1] ;
						cs = CS94SB_ID(*pt) ;
					}
					else
					{
						idx = ps[1] ;
						cs = CS96SB_ID(*pt) ;
					}

					if( ps[2] == 0)
					{
						ml_drcs_final( cs) ;
					}
					else if( ps[2] == 2)
					{
						ml_drcs_final_full() ;
					}

					font = ml_drcs_get_font( cs , 1) ;

					if( ps[3] <= 4 || ps[3] >= 255)
					{
						width = 15 ;
					}
					else
					{
						width = ps[3] ;
					}

					if( ps[6] == 0 || ps[6] >= 255)
					{
						height = 12 ;
					}
					else
					{
						height = ps[6] ;
					}

					while( 1)
					{
						p = ++pt ;

						while( *pt == '/' || ('?' <= *pt && *pt <= '~'))
						{
							pt ++ ;
						}

						if( *pt)
						{
							*pt = '\0' ;
							is_end = 0 ;
						}
						else
						{
							is_end = 1 ;
						}

						if( *p)
						{
							if( strlen(p) == (width + 1) *
								((height + 5) / 6) - 1)
							{
								ml_drcs_add( font , idx ,
									p , width , height) ;
							}
						#ifdef  DEBUG
							else
							{
								kik_debug_printf( KIK_DEBUG_TAG
								    "DRCS illegal size %s\n" ,
								    p) ;
							}
						#endif

							idx ++ ;
						}

						if( is_end)
						{
							break ;
						}
					}
				}
			}
			else
			{
				u_char *  data ;

				if( *str_p == '!' && *(str_p + 1) == 'z')
				{
					/* DECDMAC */

					if( left <= 2)
					{
						left = 0 ;

						return  0 ;
					}

					data = (str_p += 2) ;
					left -= 2 ;
				}
				else
				{
					if( ! increment_str( &str_p , &left))
					{
						return  0 ;
					}

					data = NULL ;
				}

				/*
				 * +1 in case str_p[left - vt100_parser->r_buf.new_len] points
				 * "\\" of "\x1b\\".
				 */
				if( left > vt100_parser->r_buf.new_len + 1)
				{
					str_p += (left - vt100_parser->r_buf.new_len - 1) ;
					left = vt100_parser->r_buf.new_len + 1 ;
				}

				if( get_pt_in_esc_seq( &str_p , &left , 1 , 0))
				{
					if( data)
					{
						define_macro( vt100_parser ,
							dcs_beg + (*dcs_beg == '\x1b' ? 2 : 1) ,
							data) ;
					}
				}
				else if( left == 0)
				{
					return  0 ;
				}
			}
		}
		else if( *str_p == 'X' || *str_p == '^' || *str_p == '_')
		{
			/*
			 * "ESC X" SOS
			 * "ESC ^" PM
			 * "ESC _" APC
			 */
			if( ! inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0))
			{
				return  0 ;
			}

			/* +1 in case str_p[left - new_len] points "\\" of "\x1b\\". */
			if( left > vt100_parser->r_buf.new_len + 1)
			{
				str_p += (left - vt100_parser->r_buf.new_len - 1) ;
				left = vt100_parser->r_buf.new_len + 1 ;
			}

			if( ! get_pt_in_esc_seq( &str_p , &left , 1 , 0) && left == 0)
			{
				return  0 ;
			}
		}
		/* Other final character */
		else if( 0x30 <= *str_p && *str_p <= 0x7e)
		{
		#ifdef  DEBUG
			debug_print_unknown( "ESC %c\n" , *str_p) ;
		#endif
		}
		/* intermediate character */
		else if( 0x20 <= *str_p && *str_p <= 0x2f)
		{
			/*
			 * ESC I.....I  F
			 * 033 040-057  060-176
			 */
			u_int  ic_num ;

			ic_num = 0 ;

		#ifdef  DEBUG
			debug_print_unknown( "ESC") ;
		#endif

			/* In case more than one intermediate(0x20-0x2f) chars. */
			do
			{
				ic_num ++ ;

			#ifdef  DEBUG
				kik_msg_printf( " %c" , *str_p) ;
			#endif

				if( ! inc_str_in_esc_seq( vt100_parser->screen ,
							&str_p , &left , 0))
				{
					return  0 ;
				}
			}
			while( 0x20 <= *str_p && *str_p <= 0x2f) ;

		#ifdef  DEBUG
			kik_msg_printf( " %c\n" , *str_p) ;
		#endif

			if( ic_num == 1 || ic_num == 2)
			{
				if( ic_num == 1 && *(str_p - 1) == '#')
				{
					if( *str_p == '8')
					{
						/* "ESC # 8" DEC screen alignment test */

						ml_screen_set_scroll_region(
							vt100_parser->screen , -1 , -1) ;
						ml_screen_set_use_margin(
							vt100_parser->screen , 0) ;
						ml_screen_fill_area( vt100_parser->screen , 'E' ,
							0 , 0 ,
							ml_screen_get_cols(vt100_parser->screen) ,
							ml_screen_get_rows(vt100_parser->screen)) ;
					}
				}
				else if( *(str_p - ic_num) == '(' ||
				         *(str_p - ic_num) == '$')
				{
					/*
					 * "ESC ("(Registered CS),
					 * "ESC ( SP"(DRCS) or "ESC $"
					 * See ml_convert_to_internal_ch() about CS94MB_ID.
					 */

					if( IS_ENCODING_BASED_ON_ISO2022(vt100_parser->encoding))
					{
						/* ESC ( will be processed in mkf. */
						return  1 ;
					}

					vt100_parser->g0 = *(str_p - ic_num) == '$' ?
								CS94MB_ID(*str_p) :
								CS94SB_ID(*str_p) ;

					if( ! vt100_parser->is_so)
					{
						vt100_parser->gl = vt100_parser->g0 ;
					}
				}
				else if( *(str_p - ic_num) == ')')
				{
					/* "ESC )"(Registered CS) or "ESC ( SP"(DRCS) */

					if( IS_ENCODING_BASED_ON_ISO2022(vt100_parser->encoding))
					{
						/* ESC ) will be processed in mkf. */
						return  1 ;
					}

					vt100_parser->g1 = CS94SB_ID(*str_p) ;

					if( vt100_parser->is_so)
					{
						vt100_parser->gl = vt100_parser->g1 ;
					}
				}
				else
				{
					/*
					 * "ESC SP F", "ESC SP G", "ESC SP L", "ESC SP M",
					 * "ESC SP N" etc ...
					 */
				}
			}
		}
		else
		{
			/* not VT100 control sequence. */

		#ifdef  ESCSEQ_DEBUG
			kik_msg_printf( "=> not VT100 control sequence.\n") ;
		#endif

			return  1 ;
		}

	#ifdef  ESCSEQ_DEBUG
		kik_msg_printf( "\n") ;
	#endif
	}
	else if( *str_p == CTL_SI)
	{
		if( IS_ENCODING_BASED_ON_ISO2022(vt100_parser->encoding))
		{
			/* SI will be processed in mkf. */
			return  1 ;
		}

	#ifdef  ESCSEQ_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " receiving SI\n") ;
	#endif

		vt100_parser->gl = vt100_parser->g0 ;
		vt100_parser->is_so = 0 ;
	}
	else if( *str_p == CTL_SO)
	{
		if( IS_ENCODING_BASED_ON_ISO2022(vt100_parser->encoding))
		{
			/* SO will be processed in mkf. */
			return  1 ;
		}

	#ifdef  ESCSEQ_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " receiving SO\n") ;
	#endif

		vt100_parser->gl = vt100_parser->g1 ;
		vt100_parser->is_so = 1 ;
	}
	else if( CTL_LF <= *str_p && *str_p <= CTL_FF)
	{
	#ifdef  ESCSEQ_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " receiving LF\n") ;
	#endif

		ml_screen_line_feed( vt100_parser->screen) ;
	}
	else if( *str_p == CTL_CR)
	{
	#ifdef  ESCSEQ_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " receiving CR\n") ;
	#endif

		ml_screen_goto_beg_of_line( vt100_parser->screen) ;
	}
	else if( *str_p == CTL_TAB)
	{
	#ifdef  ESCSEQ_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " receiving TAB\n") ;
	#endif

		ml_screen_vertical_forward_tabs( vt100_parser->screen , 1) ;
	}
	else if( *str_p == CTL_BS)
	{
	#ifdef  ESCSEQ_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " receiving BS\n") ;
	#endif

		ml_screen_go_back( vt100_parser->screen , 1) ;
	}
	else if( *str_p == CTL_BEL)
	{
	#ifdef  ESCSEQ_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " receiving BEL\n") ;
	#endif

		if( HAS_XTERM_LISTENER(vt100_parser,bel))
		{
			stop_vt100_cmd( vt100_parser , 0) ;
			(*vt100_parser->xterm_listener->bel)(
				vt100_parser->xterm_listener->self) ;
			/*
			 * XXX
			 * start_vt100_cmd( ... , *1*) erases cursor which
			 * xterm_listener::bell drew if bell mode is visual.
			 */
			start_vt100_cmd( vt100_parser , 1) ;
		}
	}
	else if( *str_p == 0x90)
	{
		 goto  parse_dcs ;
	}
	else
	{
		/* not VT100 control sequence */

		return  1 ;
	}

#ifdef  EDIT_DEBUG
	ml_edit_dump( vt100_parser->screen->edit) ;
#endif

	vt100_parser->r_buf.left = left - 1 ;

	return  1 ;
}

/*
 * XXX
 * mkf_map_ucs4_to_iscii() in mkf_ucs4_iscii.h is used directly in
 * parse_vt100_sequence(), though it should be used internally in mkf library
 */
int  mkf_map_ucs4_to_iscii( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

static int
parse_vt100_sequence(
	ml_vt100_parser_t *  vt100_parser
	)
{
	mkf_char_t  ch ;
	size_t  prev_left ;

	while( 1)
	{
		prev_left = vt100_parser->r_buf.left ;

		/*
		 * parsing character encoding.
		 */
		(*vt100_parser->cc_parser->set_str)( vt100_parser->cc_parser ,
			CURRENT_STR_P(vt100_parser) , vt100_parser->r_buf.left) ;
		while( (*vt100_parser->cc_parser->next_char)( vt100_parser->cc_parser , &ch))
		{
			int  ret ;

			ret = ml_convert_to_internal_ch( &ch ,
				vt100_parser->unicode_policy , vt100_parser->gl) ;

			if( ret == 1)
			{
			#if  ! defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND)
				if( IS_ISCII(ch.cs) && ch.size == 2)
				{
					ch.size = 1 ;
					put_char( vt100_parser , mkf_char_to_int(&ch) ,
						ch.cs , ch.property) ;
					ch.ch[0] = ch.ch[1] ;
					/* nukta is always combined. */
					ch.property |= MKF_COMBINING ;
				}
			#endif

				put_char( vt100_parser , mkf_char_to_int(&ch) ,
					ch.cs , ch.property) ;

				vt100_parser->r_buf.left = vt100_parser->cc_parser->left ;
			}
			else if( ret == -1)
			{
				/*
				 * This is a control sequence (C0 or C1), so
				 * reparsing this char in vt100_escape_sequence() ...
				 */

				vt100_parser->cc_parser->left ++ ;
				vt100_parser->cc_parser->is_eos = 0 ;

				break ;
			}
		}

		vt100_parser->r_buf.left = vt100_parser->cc_parser->left ;

		flush_buffer( vt100_parser) ;

		if( vt100_parser->cc_parser->is_eos)
		{
			/* expect more input */
			break ;
		}

		/*
		 * parsing other vt100 sequences.
		 * (vt100_parser->w_buf is always flushed here.)
		 */

		if( ! parse_vt100_escape_sequence( vt100_parser))
		{
			/* expect more input */
			break ;
		}

	#ifdef  EDIT_ROUGH_DEBUG
		ml_edit_dump( vt100_parser->screen->edit) ;
	#endif

		if( vt100_parser->r_buf.left == prev_left)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				" unrecognized sequence[%.2x] is received , ignored...\n" ,
				*CURRENT_STR_P(vt100_parser)) ;
		#endif

			vt100_parser->r_buf.left -- ;
		}

		if( vt100_parser->r_buf.left == 0)
		{
			break ;
		}
	}

	/*
	 * It is not necessary to process pending events for other windows on
	 * framebuffer because there is only one active window.
	 */
#ifndef  USE_FRAMEBUFFER
	if( vt100_parser->yield)
	{
		vt100_parser->yield = 0 ;

		return  0 ;
	}
#endif

	return  1 ;
}

static int
write_loopback(
	ml_vt100_parser_t *  vt100_parser ,
	const u_char *  buf ,
	size_t  len ,
	int  enable_local_echo ,
	int  is_visual
	)
{
	char *  orig_buf ;
	size_t  orig_left ;

	if( vt100_parser->r_buf.len < len &&
	    ! change_read_buffer_size( &vt100_parser->r_buf , len))
	{
		return  0 ;
	}

	if( (orig_left = vt100_parser->r_buf.left) > 0)
	{
		if( ! ( orig_buf = alloca( orig_left)))
		{
			return  0 ;
		}

		memcpy( orig_buf , CURRENT_STR_P(vt100_parser) , orig_left) ;
	}

	memcpy( vt100_parser->r_buf.chars , buf , len) ;
	vt100_parser->r_buf.filled_len = vt100_parser->r_buf.left = len ;

	if( is_visual)
	{
		start_vt100_cmd( vt100_parser , 1) ;
	}

	if( enable_local_echo)
	{
		ml_screen_enable_local_echo( vt100_parser->screen) ;
	}
	/*
	 * bidi and visual-indian is always stopped from here.
	 * If you want to call {start|stop}_vt100_cmd (where ml_xterm_event_listener is called),
	 * the second argument of it shoule be 0.
	 */
	parse_vt100_sequence( vt100_parser) ;

	if( is_visual)
	{
		stop_vt100_cmd( vt100_parser , 1) ;
	}

	if( orig_left > 0)
	{
		memcpy( vt100_parser->r_buf.chars , orig_buf , orig_left) ;
		vt100_parser->r_buf.filled_len = vt100_parser->r_buf.left = orig_left ;
	}

	return  1 ;
}


/* --- global functions --- */

void
ml_set_use_alt_buffer(
	int  use
	)
{
	use_alt_buffer = use ;
}

void
ml_set_use_ansi_colors(
	int  use
	)
{
	use_ansi_colors = use ;
}

void
ml_set_unicode_noconv_areas(
	char *  areas
	)
{
	unicode_noconv_areas = set_area_to_table( unicode_noconv_areas ,
					&num_of_unicode_noconv_areas , areas) ;
}

void
ml_set_full_width_areas(
	char *  areas
	)
{
	full_width_areas = set_area_to_table( full_width_areas ,
					&num_of_full_width_areas , areas) ;
}

void
ml_set_use_ttyrec_format(
	int  use
	)
{
	use_ttyrec_format = use ;
}

#ifdef  USE_LIBSSH2
void
ml_set_use_scp_full(
	int  use
	)
{
	use_scp_full = use ;
}
#endif

ml_vt100_parser_t *
ml_vt100_parser_new(
	ml_screen_t *  screen ,
	ml_char_encoding_t  encoding ,
	int  is_auto_encoding ,
	int  use_auto_detect ,
	int  logging_vt_seq ,
	ml_unicode_policy_t  policy ,
	u_int  col_size_a ,
	int  use_char_combining ,
	int  use_multi_col_char ,
	const char *  win_name ,
	const char *  icon_name ,
	ml_alt_color_mode_t  alt_color_mode
	)
{
	ml_vt100_parser_t *  vt100_parser ;

	if( ( vt100_parser = calloc( 1 , sizeof( ml_vt100_parser_t))) == NULL)
	{
		return  NULL ;
	}

	ml_str_init( vt100_parser->w_buf.chars , PTY_WR_BUFFER_SIZE) ;
	vt100_parser->w_buf.output_func = ml_screen_overwrite_chars ;

	vt100_parser->screen = screen ;

	vt100_parser->log_file = -1 ;
	
	vt100_parser->cs = UNKNOWN_CS ;
	vt100_parser->fg_color = ML_FG_COLOR ;
	vt100_parser->bg_color = ML_BG_COLOR ;
	vt100_parser->use_char_combining = use_char_combining ;
	vt100_parser->use_multi_col_char = use_multi_col_char ;
	vt100_parser->use_auto_detect = use_auto_detect ;
	vt100_parser->logging_vt_seq = logging_vt_seq ;
	vt100_parser->unicode_policy = policy ;

	if( ( vt100_parser->cc_conv = ml_conv_new( encoding)) == NULL)
	{
		goto  error ;
	}

	if( ( vt100_parser->cc_parser = ml_parser_new( encoding)) == NULL)
	{
		goto  error ;
	}

	if( ( vt100_parser->drcs = ml_drcs_new()) == NULL)
	{
		goto  error ;
	}

	vt100_parser->encoding = encoding ;

	if( win_name)
	{
		vt100_parser->win_name = strdup( win_name) ;
	}

	if( icon_name)
	{
		vt100_parser->icon_name = strdup( icon_name) ;
	}

	vt100_parser->gl = US_ASCII ;
	vt100_parser->g0 = US_ASCII ;
	vt100_parser->g1 = US_ASCII ;

	set_col_size_of_width_a( vt100_parser , col_size_a) ;

#if  0
	/* Default value of modify_*_keys except modify_other_keys is 2. */
	vt100_parser->modify_cursor_keys = 2 ;
	vt100_parser->modify_function_keys = 2 ;
#endif

	vt100_parser->sixel_scrolling = 1 ;

	vt100_parser->alt_color_mode = alt_color_mode ;

	return  vt100_parser ;

error:
	if( vt100_parser->cc_conv)
	{
		(*vt100_parser->cc_conv->delete)( vt100_parser->cc_conv) ;
	}

	if( vt100_parser->cc_parser)
	{
		(*vt100_parser->cc_parser->delete)( vt100_parser->cc_parser) ;
	}

	if( vt100_parser->drcs)
	{
		ml_drcs_delete( vt100_parser->drcs) ;
	}

	free( vt100_parser) ;

	return  NULL ;
}

int
ml_vt100_parser_delete(
	ml_vt100_parser_t *  vt100_parser
	)
{
	ml_str_final( vt100_parser->w_buf.chars , PTY_WR_BUFFER_SIZE) ;
	(*vt100_parser->cc_parser->delete)( vt100_parser->cc_parser) ;
	(*vt100_parser->cc_conv->delete)( vt100_parser->cc_conv) ;
	ml_drcs_delete( vt100_parser->drcs) ;
	delete_all_macros( vt100_parser) ;

	if( vt100_parser->log_file != -1)
	{
		close( vt100_parser->log_file) ;
	}

	free( vt100_parser->r_buf.chars) ;

	free( vt100_parser->win_name) ;
	free( vt100_parser->icon_name) ;
	free( vt100_parser->saved_win_names.names) ;
	free( vt100_parser->saved_icon_names.names) ;

	free( vt100_parser) ;
	
	return  1 ;
}

void
ml_vt100_parser_set_pty(
	ml_vt100_parser_t *  vt100_parser ,
	ml_pty_ptr_t  pty
	)
{
	vt100_parser->pty = pty ;
}

void
ml_vt100_parser_set_xterm_listener(
	ml_vt100_parser_t *  vt100_parser ,
	ml_xterm_event_listener_t *  xterm_listener
	)
{
	vt100_parser->xterm_listener = xterm_listener ;
}

void
ml_vt100_parser_set_config_listener(
	ml_vt100_parser_t *  vt100_parser ,
	ml_config_event_listener_t *  config_listener
	)
{
	vt100_parser->config_listener = config_listener ;
}

int
ml_parse_vt100_sequence(
	ml_vt100_parser_t *  vt100_parser
	)
{
	int  count ;

	if( ml_screen_local_echo_wait( vt100_parser->screen , 500))
	{
		return  1 ;
	}

	if( ! vt100_parser->pty || receive_bytes( vt100_parser) == 0)
	{
		return  0 ;
	}

	start_vt100_cmd( vt100_parser , 1) ;

	ml_screen_disable_local_echo( vt100_parser->screen) ;

	/*
	 * bidi and visual-indian is always stopped from here.
	 * If you want to call {start|stop}_vt100_cmd (where ml_xterm_event_listener is called),
	 * the second argument of it shoule be 0.
	 */

	/* Maximum size of sequence parsed once is PTY_RD_BUFFER_SIZE * 3. */
	count = 0 ;
	while( parse_vt100_sequence( vt100_parser) &&
	       /*
	        * XXX
	        * It performs well to read as large amount of data as possible
		* on framebuffer on old machines.
		*/
	#if  (! defined(__NetBSD__) && ! defined(__OpenBSD__)) || ! defined(USE_FRAMEBUFFER)
	       /* (PTY_RD_BUFFER_SIZE / 2) is baseless. */
	       vt100_parser->r_buf.filled_len >= (PTY_RD_BUFFER_SIZE / 2) &&
	       (++count) < MAX_READ_COUNT &&
	#endif
	       receive_bytes( vt100_parser)) ;

	stop_vt100_cmd( vt100_parser , 1) ;

	return  1 ;
}

void
ml_reset_pending_vt100_sequence(
	ml_vt100_parser_t *  vt100_parser
	)
{
	if( vt100_parser->r_buf.left >= 2 &&
	    is_dcs_or_osc( CURRENT_STR_P(vt100_parser)))
	{
		/* Reset DCS or OSC */
		vt100_parser->r_buf.left = 0 ;
	}
}

int
ml_vt100_parser_write_loopback(
	ml_vt100_parser_t *  vt100_parser ,
	const u_char *  buf ,
	size_t  len
	)
{
	return  write_loopback( vt100_parser , buf , len , 0 , 1) ;
}

#ifdef  __ANDROID__
int
ml_vt100_parser_preedit(
	ml_vt100_parser_t *  vt100_parser ,
	const u_char *  buf ,
	size_t  len
	)
{
	if( ! vt100_parser->underline_style)
	{
		char *  new_buf ;
		size_t  new_len ;

		if( ( new_buf = alloca( ( new_len = 4 + len + 5))))
		{
			memcpy( new_buf , "\x1b[4m" , 4) ;
			memcpy( new_buf + 4 , buf , len) ;
			memcpy( new_buf + 4 + len , "\x1b[24m" , 5) ;
			buf = new_buf ;
			len = new_len ;
		}
	}

	return  write_loopback( vt100_parser , buf , len , 1 , 1) ;
}
#endif

int
ml_vt100_parser_local_echo(
	ml_vt100_parser_t *  vt100_parser ,
	const u_char *  buf ,
	size_t  len
	)
{
	size_t  count ;

	for( count = 0 ; count < len ; count++)
	{
		if( buf[count] < 0x20)
		{
			ml_screen_local_echo_wait( vt100_parser->screen , 0) ;
			ml_parse_vt100_sequence( vt100_parser) ;

			return  1 ;
		}
	}

	ml_parse_vt100_sequence( vt100_parser) ;

	if( ! vt100_parser->underline_style)
	{
		char *  new_buf ;
		size_t  new_len ;

		if( ( new_buf = alloca( ( new_len = 4 + len + 5))))
		{
			memcpy( new_buf , "\x1b[4m" , 4) ;
			memcpy( new_buf + 4 , buf , len) ;
			memcpy( new_buf + 4 + len , "\x1b[24m" , 5) ;
			buf = new_buf ;
			len = new_len ;
		}
	}

	return  write_loopback( vt100_parser , buf , len , 1 , 1) ;
}

int
ml_vt100_parser_change_encoding(
	ml_vt100_parser_t *  vt100_parser ,
	ml_char_encoding_t  encoding
	)
{
	mkf_parser_t *  cc_parser ;
	mkf_conv_t *  cc_conv ;

	cc_conv = ml_conv_new( encoding) ;
	cc_parser = ml_parser_new( encoding) ;

	if( cc_parser == NULL || cc_conv == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " encoding not changed.\n") ;
	#endif
		if( cc_parser)
		{
			(*cc_parser->delete)( cc_parser) ;
		}

		if( cc_conv)
		{
			(*cc_conv->delete)( cc_conv) ;
		}

		return  0 ;
	}
	
#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " encoding changed.\n") ;
#endif

	(*vt100_parser->cc_parser->delete)( vt100_parser->cc_parser) ;
	(*vt100_parser->cc_conv->delete)( vt100_parser->cc_conv) ;

	vt100_parser->encoding = encoding ;
	vt100_parser->cc_parser = cc_parser ;
	vt100_parser->cc_conv = cc_conv ;

	/* reset */
	vt100_parser->gl = US_ASCII ;
	vt100_parser->g0 = US_ASCII ;
	vt100_parser->g1 = US_ASCII ;
	vt100_parser->is_so = 0 ;
	
	return  1 ;
}

size_t
ml_vt100_parser_convert_to(
	ml_vt100_parser_t *  vt100_parser ,
	u_char *  dst ,
	size_t  len ,
	mkf_parser_t *  parser
	)
{
	return  (*vt100_parser->cc_conv->convert)( vt100_parser->cc_conv , dst , len , parser) ;
}

int
ml_init_encoding_parser(
	ml_vt100_parser_t *  vt100_parser
	)
{
	(*vt100_parser->cc_parser->init)( vt100_parser->cc_parser) ;
	vt100_parser->gl = US_ASCII ;
	vt100_parser->g0 = US_ASCII ;
	vt100_parser->g1 = US_ASCII ;
	vt100_parser->is_so = 0 ;

	return  1 ;
}

int
ml_init_encoding_conv(
	ml_vt100_parser_t *  vt100_parser
	)
{
	(*vt100_parser->cc_conv->init)( vt100_parser->cc_conv) ;

	/*
	 * XXX
	 * this causes unexpected behaviors in some applications(e.g. biew) ,
	 * but this is necessary , since 0x00 - 0x7f is not necessarily US-ASCII
	 * in these encodings but key input or selection paste assumes that
	 * 0x00 - 0x7f should be US-ASCII at the initial state.
	 */
	if( IS_STATEFUL_ENCODING(vt100_parser->encoding))
	{
		ml_init_encoding_parser(vt100_parser) ;
	}

	return  1 ;
}

int
ml_set_auto_detect_encodings(
	char *  encodings
	)
{
	char *  p ;
	u_int  count ;

	if( auto_detect_count > 0)
	{
		for( count = 0 ; count < auto_detect_count ; count++)
		{
			(*auto_detect[count].parser->delete)( auto_detect[count].parser) ;
		}

		free( auto_detect) ;
		auto_detect_count = 0 ;
	}

	free( auto_detect_encodings) ;

	if( *encodings == '\0')
	{
		auto_detect_encodings = NULL ;

		return  1 ;
	}
	else
	{
		auto_detect_encodings = strdup( encodings) ;
	}

	if( ! ( auto_detect = malloc( sizeof(*auto_detect) *
					(kik_count_char_in_str( encodings , ',') + 1))))
	{
		return  0 ;
	}

	while( ( p = kik_str_sep( &encodings , ",")))
	{
		if( ( auto_detect[auto_detect_count].encoding = ml_get_char_encoding( p)) !=
			ML_UNKNOWN_ENCODING)
		{
			auto_detect_count ++ ;
		}
	}

	if( auto_detect_count == 0)
	{
		free( auto_detect) ;

		return  0 ;
	}

	for( count = 0 ; count < auto_detect_count ; count++)
	{
		auto_detect[count].parser = ml_parser_new( auto_detect[count].encoding) ;
	}

	return  1 ;
}

/*
 * Return value
 *  1:  Succeed
 *  0:  Error
 *  -1: Control sequence
 */
int
ml_convert_to_internal_ch(
	mkf_char_t *  orig_ch ,
#if  0
	ml_char_encoding_t  encoding ,
#endif
	ml_unicode_policy_t  unicode_policy ,
	mkf_charset_t  gl
	)
{
	mkf_char_t  ch ;

	ch = *orig_ch ;

	/*
	 * UCS <-> OTHER CS
	 */
	if( ch.cs == ISO10646_UCS4_1)
	{
		u_char  decsp ;

		if( ch.ch[2] == 0x00 && ch.ch[3] <= 0x7f &&
		    ch.ch[1] == 0x00 && ch.ch[0] == 0x00)
		{
			/* this is always done */
			ch.ch[0] = ch.ch[3] ;
			ch.size = 1 ;
			ch.cs = US_ASCII ;
		}
		else if( ( unicode_policy & NOT_USE_UNICODE_BOXDRAW_FONT) &&
			 ( decsp = convert_ucs_to_decsp( mkf_char_to_int(&ch))))
		{
			ch.ch[0] = decsp ;
			ch.size = 1 ;
			ch.cs = DEC_SPECIAL ;
			ch.property = 0 ;
		}
	#if  1
		/* See http://github.com/saitoha/drcsterm/ */
		else if( ( unicode_policy & USE_UNICODE_DRCS) &&
		         ml_convert_unicode_pua_to_drcs( &ch))
		{
			/* do nothing */
		}
	#endif
		else
		{
			mkf_char_t  non_ucs ;
			u_int32_t  code ;

			ch.property = modify_ucs_property(
					(code = mkf_char_to_int( &ch)) ,
					ch.property) ;

			if( unicode_policy & NOT_USE_UNICODE_FONT)
			{
				/* convert ucs4 to appropriate charset */

				if( ! is_noconv_unicode( ch.ch) &&
				    mkf_map_locale_ucs4_to( &non_ucs , &ch) &&
				    non_ucs.cs != ISO8859_6_R &&	/* ARABIC */
				    non_ucs.cs != ISO8859_8_R)		/* HEBREW */
				{
					if( IS_FULLWIDTH_CS( non_ucs.cs))
					{
						non_ucs.property = MKF_FULLWIDTH ;
					}

					ch = non_ucs ;

					goto  end ;
				}
			}

		#if  ! defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND)
			if( 0x900 <= code && code <= 0xd7f)
			{
				if( mkf_map_ucs4_to_iscii( &non_ucs , code))
				{
					ch.ch[0] = non_ucs.ch[0] ;
					/* non_ucs.cs is set if mkf_map_ucs4_to_iscii() fails. */
					ch.cs = non_ucs.cs ;
					ch.size = 1 ;
					/* ch.property is not changed. */
				}
				else
				{
					switch( code & 0x07f)
					{
					case  0x0c:
						ch.ch[0] = '\xa6' ;
						break ;
					case  0x3d:
						ch.ch[0] = '\xea' ;
						break ;
					case  0x44:
						ch.ch[0] = '\xdf' ;
						break ;
					case  0x50:
						ch.ch[0] = '\xa1' ;
						break ;
					case  0x58:
						ch.ch[0] = '\xb3' ;
						break ;
					case  0x59:
						ch.ch[0] = '\xb4' ;
						break ;
					case  0x5a:
						ch.ch[0] = '\xb5' ;
						break ;
					case  0x5b:
						ch.ch[0] = '\xba' ;
						break ;
					case  0x5c:
						ch.ch[0] = '\xbf' ;
						break ;
					case  0x5d:
						ch.ch[0] = '\xc0' ;
						break ;
					case  0x5e:
						ch.ch[0] = '\xc9' ;
						break ;
					case  0x60:
						ch.ch[0] = '\xaa' ;
						break ;
					case  0x61:
						ch.ch[0] = '\xa7' ;
						break ;
					case  0x62:
						ch.ch[0] = '\xdb' ;
						break ;
					case  0x63:
						ch.ch[0] = '\xdc' ;
						break ;
					default:
						goto  end ;
					}

					ch.ch[1] = '\xe9' ;
					/* non_ucs.cs is set if mkf_map_ucs4_to_iscii() fails. */
					ch.cs = non_ucs.cs ;
					ch.size = 2 ;
					/* ch.property is not changed. */
				}
			}
		#endif

		end:
			;
		}
	}
	else if( ch.cs != US_ASCII)
	{
		if( ( unicode_policy & ONLY_USE_UNICODE_FONT) ||
		    /* XXX converting japanese gaiji to ucs. */
		    ch.cs == JISC6226_1978_NEC_EXT ||
		    ch.cs == JISC6226_1978_NECIBM_EXT ||
		    ch.cs == JISX0208_1983_MAC_EXT ||
		    ch.cs == SJIS_IBM_EXT ||
		    /* XXX converting RTL characters to ucs. */
		    ch.cs == ISO8859_6_R ||	/* Arabic */
		    ch.cs == ISO8859_8_R	/* Hebrew */
		#if  0
		    /* GB18030_2000 2-byte chars(==GBK) are converted to UCS */
		    || ( encoding == ML_GB18030 && ch.cs == GBK)
		#endif
			)
		{
			mkf_char_t  ucs ;

			if( mkf_map_to_ucs4( &ucs , &ch))
			{
				ch = ucs ;
				ch.property = get_ucs_property( mkf_char_to_int(&ucs)) ;
			}
		}
		else if( IS_FULLWIDTH_CS( ch.cs))
		{
			ch.property = MKF_FULLWIDTH ;
		}
	}

	if( ch.size == 1)
	{
		/* single byte cs */

		if( (ch.ch[0] == 0x0 || ch.ch[0] == 0x7f))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" 0x0/0x7f sequence is received , ignored...\n") ;
		#endif
			return  0 ;
		}
		else if( (ch.ch[0] & 0x7f) <= 0x1f && ch.cs == US_ASCII)
		{
			/* Control sequence (C0 or C1) */
			return  -1 ;
		}

		if( ml_is_msb_set( ch.cs))
		{
			SET_MSB( ch.ch[0]) ;
		}
		else
		{
			if( ch.cs == US_ASCII && gl != US_ASCII)
			{
				/* XXX prev_ch should not be static. */
				static u_char  prev_ch ;
				static mkf_charset_t  prev_gl = US_ASCII ;

				if( IS_CS94MB( gl))
				{
					if( gl == prev_gl && prev_ch)
					{
						ch.ch[1] = ch.ch[0] ;
						ch.ch[0] = prev_ch ;
						ch.size = 2 ;
						ch.property = MKF_FULLWIDTH ;
						prev_ch = 0 ;
						prev_gl = US_ASCII ;
					}
					else
					{
						prev_ch = ch.ch[0] ;
						prev_gl = gl ;

						return  0 ;
					}
				}

				ch.cs = gl ;
			}

			if( ch.cs == DEC_SPECIAL)
			{
				u_int16_t  ucs ;

				if( ( unicode_policy & ONLY_USE_UNICODE_BOXDRAW_FONT) &&
				    ( ucs = convert_decsp_to_ucs( ch.ch[0])))
				{
					mkf_int_to_bytes( ch.ch , 4 , ucs) ;
					ch.size = 4 ;
					ch.cs = ISO10646_UCS4_1 ;
					ch.property = get_ucs_property( ucs) ;
				}
			}
		}
	}
	else
	{
		/*
		 * NON UCS <-> NON UCS
		 */

		/* multi byte cs */

		/*
		 * XXX hack
		 * how to deal with johab 10-4-4(8-4-4) font ?
		 * is there any uhc font ?
		 */

		if( ch.cs == JOHAB)
		{
			mkf_char_t  uhc ;

			if( mkf_map_johab_to_uhc( &uhc , &ch) == 0)
			{
				return  0 ;
			}

			ch = uhc ;
		}

		/*
		 * XXX
		 * switching option whether this conversion is done should
		 * be introduced.
		 */
		if( ch.cs == UHC)
		{
			mkf_char_t  ksc ;

			if( mkf_map_uhc_to_ksc5601_1987( &ksc , &ch) == 0)
			{
				return  0 ;
			}

			ch = ksc ;
		}
	}

	*orig_ch = ch ;

	return  1 ;
}

void
ml_vt100_parser_set_alt_color_mode(
	ml_vt100_parser_t *  vt100_parser ,
	ml_alt_color_mode_t  mode
	)
{
	vt100_parser->alt_color_mode = mode ;
}

int
true_or_false(
	const char *  str
	)
{
	if( strcmp( str , "true") == 0)
	{
		return  1 ;
	}
	else if( strcmp( str , "false") == 0)
	{
		return  0 ;
	}
	else
	{
		return  -1 ;
	}
}

int
ml_vt100_parser_get_config(
	ml_vt100_parser_t *  vt100_parser ,
	ml_pty_ptr_t  output ,
	char *  key ,
	int  to_menu ,
	int *  flag
	)
{
	char *  value ;
	char  digit[DIGIT_STR_LEN(u_int) + 1] ;
	char  cwd[PATH_MAX] ;

	if( strcmp( key , "encoding") == 0)
	{
		value = ml_get_char_encoding_name( vt100_parser->encoding) ;
	}
	else if( strcmp( key , "is_auto_encoding") == 0)
	{
		if( vt100_parser->is_auto_encoding)
		{
			value = "true" ;
		}
		else
		{
			value = "false" ;
		}
	}
	else if( strcmp( key , "use_alt_buffer") == 0)
	{
		if( use_alt_buffer)
		{
			value = "true" ;
		}
		else
		{
			value = "false" ;
		}
	}
	else if( strcmp( key , "use_ansi_colors") == 0)
	{
		if( use_ansi_colors)
		{
			value = "true" ;
		}
		else
		{
			value = "false" ;
		}
	}
	else if( strcmp( key , "tabsize") == 0)
	{
		sprintf( digit , "%d" , ml_screen_get_tab_size( vt100_parser->screen)) ;
		value = digit ;
	}
	else if( strcmp( key , "logsize") == 0)
	{
		if( ml_screen_log_size_is_unlimited( vt100_parser->screen))
		{
			value = "unlimited" ;
		}
		else
		{
			sprintf( digit , "%d" ,
				ml_screen_get_log_size( vt100_parser->screen)) ;
			value = digit ;
		}
	}
	else if( strcmp( key , "static_backscroll_mode") == 0)
	{
		if( ml_screen_is_backscrolling( vt100_parser->screen) == BSM_STATIC)
		{
			value = "true" ;
		}
		else
		{
			value = "false" ;
		}
	}
	else if( strcmp( key , "use_combining") == 0)
	{
		if( vt100_parser->use_char_combining)
		{
			value = "true" ;
		}
		else
		{
			value = "false" ;
		}
	}
	else if( strcmp( key , "col_size_of_width_a") == 0)
	{
		if( vt100_parser->col_size_of_width_a == 2)
		{
			value = "2" ;
		}
		else
		{
			value = "1" ;
		}
	}
	else if( strcmp( key , "locale") == 0)
	{
		value = kik_get_locale() ;
	}
	else if( strcmp( key , "pwd") == 0)
	{
		value = getcwd( cwd , sizeof(cwd)) ;
	}
	else if( strcmp( key , "logging_vt_seq") == 0)
	{
		if( vt100_parser->logging_vt_seq)
		{
			value = "true" ;
		}
		else
		{
			value = "false" ;
		}
	}
	else if( strcmp( key , "vt_seq_format") == 0)
	{
		if( use_ttyrec_format)
		{
			value = "ttyrec" ;
		}
		else
		{
			value = "raw" ;
		}
	}
	else if( strcmp( key , "rows") == 0)
	{
		sprintf( digit , "%d" ,
			ml_screen_get_logical_rows( vt100_parser->screen)) ;
		value = digit ;
	}
	else if( strcmp( key , "cols") == 0)
	{
		sprintf( digit , "%d" ,
			ml_screen_get_logical_cols( vt100_parser->screen)) ;
		value = digit ;
	}
	else if( strcmp( key , "not_use_unicode_font") == 0)
	{
		if( vt100_parser->unicode_policy & NOT_USE_UNICODE_FONT)
		{
			value = "true" ;
		}
		else
		{
			value = "false" ;
		}
	}
	else if( strcmp( key , "only_use_unicode_font") == 0)
	{
		if( vt100_parser->unicode_policy & ONLY_USE_UNICODE_FONT)
		{
			value = "true" ;
		}
		else
		{
			value = "false" ;
		}
	}
	else if( strcmp( key , "box_drawing_font") == 0)
	{
		if( vt100_parser->unicode_policy & NOT_USE_UNICODE_BOXDRAW_FONT)
		{
			value = "decsp" ;
		}
		else if( vt100_parser->unicode_policy & ONLY_USE_UNICODE_BOXDRAW_FONT)
		{
			value = "unicode" ;
		}
		else
		{
			value = "noconv" ;
		}
	}
	else if( strcmp( key , "auto_detect_encodings") == 0)
	{
		if( ( value = auto_detect_encodings) == NULL)
		{
			value = "" ;
		}
	}
	else if( strcmp( key , "use_auto_detect") == 0)
	{
		if( vt100_parser->use_auto_detect)
		{
			value = "true" ;
		}
		else
		{
			value = "false" ;
		}
	}
	else if( strcmp( key , "allow_scp") == 0)
	{
	#ifdef  USE_LIBSSH2
		if( use_scp_full)
		{
			value = "true" ;
		}
		else
	#endif
		{
			value = "false" ;
		}
	}
	else if( strcmp( key , "challenge") == 0)
	{
		if( to_menu)
		{
			value = ml_get_proto_challenge() ;
		}
		else
		{
			value = "" ;
		}
	}
	else
	{
		/* Continue to process it in x_screen.c */
		return  0 ;
	}

	/* value is never set NULL above. */
#if  0
	if( ! value)
	{
		ml_response_config( term->pty , "error" , NULL , to_menu) ;
	}
#endif

	if( flag)
	{
		*flag = value ? true_or_false( value) : -1 ;
	}
	else
	{
		ml_response_config( output , key , value , to_menu) ;
	}

	return  1 ;
}

int
ml_vt100_parser_set_config(
	ml_vt100_parser_t *  vt100_parser ,
	char *  key ,
	char *  value
	)
{
	if( strcmp( key , "encoding") == 0)
	{
		if( strcmp( value , "auto") == 0)
		{
			vt100_parser->is_auto_encoding = 
				strcasecmp( value , "auto") == 0 ? 1 : 0 ;
		}

		return  0 ;	/* Continue to process it in x_screen.c */
	}
	else if( strcmp( key , "logging_msg") == 0)
	{
		if( true_or_false( value) > 0)
		{
			kik_set_msg_log_file_name( "mlterm/msg.log") ;
		}
		else
		{
			kik_set_msg_log_file_name( NULL) ;
		}
	}
	else if( strcmp( key , "word_separators") == 0)
	{
		ml_set_word_separators( value) ;
	}
	else if( strcmp( key , "use_alt_buffer") == 0)
	{
		int  flag ;

		if( ( flag = true_or_false( value)) != -1)
		{
			use_alt_buffer = flag ;
		}
	}
	else if( strcmp( key , "use_ansi_colors") == 0)
	{
		int  flag ;

		if( ( flag = true_or_false( value)) != -1)
		{
			use_ansi_colors = flag ;
		}
	}
	else if( strcmp( key , "unicode_noconv_areas") == 0)
	{
		ml_set_unicode_noconv_areas( value) ;
	}
	else if( strcmp( key , "unicode_full_width_areas") == 0)
	{
		ml_set_full_width_areas( value) ;
	}
	else if( strcmp( key , "tabsize") == 0)
	{
		u_int  tab_size ;

		if( kik_str_to_uint( &tab_size , value))
		{
			ml_screen_set_tab_size( vt100_parser->screen ,
				tab_size) ;
		}
	}
	else if( strcmp( key , "static_backscroll_mode") == 0)
	{
		ml_bs_mode_t  mode ;
		
		if( strcmp( value , "true") == 0)
		{
			mode = BSM_STATIC ;
		}
		else if( strcmp( value , "false") == 0)
		{
			mode = BSM_DEFAULT ;
		}
		else
		{
			return  1 ;
		}

		ml_set_backscroll_mode( vt100_parser->screen , mode) ;
	}
	else if( strcmp( key , "use_combining") == 0)
	{
		int  flag ;

		if( ( flag = true_or_false( value)) != -1)
		{
			vt100_parser->use_char_combining = flag ;
		}
	}
	else if( strcmp( key , "col_size_of_width_a") == 0)
	{
		u_int  size ;

		if( kik_str_to_uint( &size , value))
		{
			set_col_size_of_width_a( vt100_parser , size) ;
		}
	}
	else if( strcmp( key , "locale") == 0)
	{
		kik_locale_init( value) ;
	}
	else if( strcmp( key , "logging_vt_seq") == 0)
	{
		int  flag ;

		if( ( flag = true_or_false( value)) != -1)
		{
			vt100_parser->logging_vt_seq = flag ;
		}
	}
	else if( strcmp( key , "vt_seq_format") == 0)
	{
		use_ttyrec_format = (strcmp( value , "ttyrec") == 0) ;
	}
	else if( strcmp( key , "box_drawing_font") == 0)
	{
		if( strcmp( value , "unicode") == 0)
		{
			vt100_parser->unicode_policy |= ONLY_USE_UNICODE_BOXDRAW_FONT ;
		}
		else if( strcmp( value , "decsp") == 0)
		{
			vt100_parser->unicode_policy |= NOT_USE_UNICODE_BOXDRAW_FONT ;
		}
		else
		{
			vt100_parser->unicode_policy &=
				(~NOT_USE_UNICODE_BOXDRAW_FONT &
			         ~ONLY_USE_UNICODE_BOXDRAW_FONT) ;
		}
	}
	else if( strcmp( key , "auto_detect_encodings") == 0)
	{
		ml_set_auto_detect_encodings( value) ;
	}
	else if( strcmp( key , "use_auto_detect") == 0)
	{
		int  flag ;

		if( ( flag = true_or_false( value)) != -1)
		{
			vt100_parser->use_auto_detect = flag ;
		}
	}
	else
	{
		/* Continue to process it in x_screen.c */
		return  0 ;
	}

	return  1 ;
}

int
ml_vt100_parser_exec_cmd(
	ml_vt100_parser_t *  vt100_parser ,
	char *  cmd
	)
{
	if( strcmp( cmd , "gen_proto_challenge") == 0)
	{
		ml_gen_proto_challenge() ;
	}
	else if( strcmp( cmd , "full_reset") == 0)
	{
		soft_reset( vt100_parser) ;
	}
	else if( strncmp( cmd , "snapshot" , 8) == 0)
	{
		char **  argv ;
		int  argc ;
		ml_char_encoding_t  encoding ;
		char *  file ;

		argv = kik_arg_str_to_array( &argc , cmd) ;

		if( argc >= 3)
		{
			encoding = ml_get_char_encoding( argv[2]) ;
		}
		else
		{
			encoding = ML_UNKNOWN_ENCODING ;
		}

		if( argc >= 2)
		{
			file = argv[1] ;
		}
		else
		{
			/* skip /dev/ */
			file = ml_pty_get_slave_name( vt100_parser->pty) + 5 ;
		}

		if( strstr( file , ".."))
		{
			/* insecure file name */
			kik_msg_printf( "%s is insecure file name.\n" , file) ;
		}
		else
		{
			snapshot( vt100_parser , encoding , file) ;
		}
	}
#ifndef  NO_IMAGE
	else if( strncmp( cmd , "show_picture " , 13) == 0 ||
	         strncmp( cmd , "add_frame " , 10) == 0)
	{
		int  clip_beg_col = 0 ;
		int  clip_beg_row = 0 ;
		int  clip_cols = 0 ;
		int  clip_rows = 0 ;
		int  img_cols = 0 ;
		int  img_rows = 0 ;
		char **  argv ;
		int  argc ;

		argv = kik_arg_str_to_array( &argc , cmd) ;
		if( argc == 1)
		{
			return  1 ;
		}

		if( argc >= 3)
		{
			int  has_img_size ;

			if( strchr( argv[argc - 1] , '+'))
			{
				sscanf( argv[argc - 1] , "%dx%d+%d+%d" ,
					&clip_cols , &clip_rows ,
					&clip_beg_col , &clip_beg_row) ;

				has_img_size = (argc >= 4) ;
			}
			else
			{
				has_img_size = 1 ;
			}

			if( has_img_size)
			{
				sscanf( argv[2] , "%dx%d" , &img_cols , &img_rows) ;
			}
		}

		if( *argv[0] == 's')
		{
			show_picture( vt100_parser , argv[1] ,
				clip_beg_col , clip_beg_row , clip_cols , clip_rows ,
					img_cols , img_rows , 0) ;
		}
		else if( HAS_XTERM_LISTENER(vt100_parser,add_frame_to_animation))
		{
			(*vt100_parser->xterm_listener->add_frame_to_animation)(
				vt100_parser->xterm_listener->self ,
				argv[1] , &img_cols , &img_rows) ;
		}
	}
#endif
#ifdef  USE_LIBSSH2
	else if( strncmp( cmd , "scp " , 4) == 0)
	{
		char **  argv ;
		int  argc ;

		argv = kik_arg_str_to_array( &argc , cmd) ;

		if( argc == 3 || argc == 4)
		{
			ml_char_encoding_t  encoding ;

			if( ! argv[3] ||
			    ( encoding = ml_get_char_encoding( argv[3]))
			      == ML_UNKNOWN_ENCODING)
			{
				encoding = vt100_parser->encoding ;
			}

			ml_pty_ssh_scp( vt100_parser->pty ,
				vt100_parser->encoding , encoding , argv[2] , argv[1] ,
				use_scp_full) ;
		}
	}
#endif
	else
	{
		return  0 ;
	}

	return  1 ;
}

#define  MOUSE_POS_LIMIT  (0xff - 0x20)
#define  EXT_MOUSE_POS_LIMIT  (0x7ff - 0x20)

void
ml_vt100_parser_report_mouse_tracking(
	ml_vt100_parser_t *  vt100_parser ,
	int  col ,
	int  row ,
	int  button ,
	int  is_released ,	/* is_released is 0 if PointerMotion */
	int  key_state ,
	int  button_state
	)
{
	if( vt100_parser->mouse_mode >= LOCATOR_CHARCELL_REPORT)
	{
		char  seq[10 + DIGIT_STR_LEN(int) * 4 + 1] ;
		int  ev ;
		int  is_outside_filter_rect ;

		is_outside_filter_rect = 0 ;
		if( vt100_parser->loc_filter.top > row ||
		    vt100_parser->loc_filter.left > col ||
		    vt100_parser->loc_filter.bottom < row ||
		    vt100_parser->loc_filter.right < col)
		{
			vt100_parser->loc_filter.top = vt100_parser->loc_filter.bottom = row ;
			vt100_parser->loc_filter.left = vt100_parser->loc_filter.right = col ;

			if( vt100_parser->locator_mode & LOCATOR_FILTER_RECT)
			{
				vt100_parser->locator_mode &= ~LOCATOR_FILTER_RECT ;
				is_outside_filter_rect = 1 ;
			}
		}

		if( button == 0)
		{
			if( is_outside_filter_rect)
			{
				ev = 10 ;
			}
			else if( vt100_parser->locator_mode & LOCATOR_REQUEST)
			{
				ev = 1 ;
			}
			else
			{
				return ;
			}
		}
		else
		{
			if( ( is_released &&
		              ! (vt100_parser->locator_mode & LOCATOR_BUTTON_UP)) ||
		            ( ! is_released &&
		              ! (vt100_parser->locator_mode & LOCATOR_BUTTON_DOWN)))
			{
				return ;
			}

			if( button == 1)
			{
				ev = is_released ? 3 : 2 ;
			}
			else if( button == 2)
			{
				ev = is_released ? 5 : 4 ;
			}
			else if( button == 3)
			{
				ev = is_released ? 7 : 6 ;
			}
			else
			{
				ev = 1 ;
			}
		}

		sprintf( seq , "\x1b[%d;%d;%d;%d;0&w" , ev , button_state , row , col) ;

		ml_write_to_pty( vt100_parser->pty , seq , strlen(seq)) ;

		if( vt100_parser->locator_mode & LOCATOR_ONESHOT)
		{
			set_mouse_report( vt100_parser , 0) ;
			vt100_parser->locator_mode = 0 ;
		}
	}
	else
	{
		/*
		 * Max length is SGR style => ESC [ < %d ; %d(col) ; %d(row) ; %c('M' or 'm') NULL
		 *                            1   1 1 3  1 3       1  3      1 1              1
		 */
		u_char  seq[17] ;
		size_t  seq_len ;

		if( is_released &&
		    vt100_parser->ext_mouse_mode != EXTENDED_MOUSE_REPORT_SGR)
		{
			key_state = 0 ;
			button = 3 ;
		}
		else if( button == 0)
		{
			/* PointerMotion */
			button = 3 + 32 ;
		}
		else
		{
			if( is_released)
			{
				/* for EXTENDED_MOUSE_REPORT_SGR */
				key_state += 0x80 ;
			}

			button -- ;	/* button1 == 0, button2 == 1, button3 == 2 */

			while( button >= 3)
			{
				/* Wheel mouse */
				key_state += 64 ;
				button -= 3 ;
			}
		}

		if( vt100_parser->ext_mouse_mode == EXTENDED_MOUSE_REPORT_SGR)
		{
			sprintf( seq , "\x1b[<%d;%d;%d%c" ,
				(button + key_state) & 0x7f , col , row ,
				((button + key_state) & 0x80) ? 'm' : 'M') ;
			seq_len = strlen(seq) ;
		}
		else if( vt100_parser->ext_mouse_mode == EXTENDED_MOUSE_REPORT_URXVT)
		{
			sprintf( seq , "\x1b[%d;%d;%dM" ,
				0x20 + button + key_state , col , row) ;
			seq_len = strlen(seq) ;
		}
		else
		{
			memcpy( seq , "\x1b[M" , 3) ;
			seq[3] = 0x20 + button + key_state ;

			if( vt100_parser->ext_mouse_mode == EXTENDED_MOUSE_REPORT_UTF8)
			{
				int  ch ;
				u_char *  p ;

				p = seq + 4 ;

				if( col > EXT_MOUSE_POS_LIMIT)
				{
					col = EXT_MOUSE_POS_LIMIT ;
				}

				if( (ch = 0x20 + col) >= 0x80)
				{
					*(p ++) = ((ch >> 6) & 0x1f) | 0xc0 ;
					*(p ++) = (ch & 0x3f) | 0x80 ;
				}
				else
				{
					*(p ++) = ch ;
				}

				if( row > EXT_MOUSE_POS_LIMIT)
				{
					row = EXT_MOUSE_POS_LIMIT ;
				}

				if( (ch = 0x20 + row) >= 0x80)
				{
					*(p ++) = ((ch >> 6) & 0x1f) | 0xc0 ;
					*p = (ch & 0x3f) | 0x80 ;
				}
				else
				{
					*p = ch ;
				}

				seq_len = p - seq + 1 ;
			}
			else
			{
				seq[4] = 0x20 + (col < MOUSE_POS_LIMIT ? col : MOUSE_POS_LIMIT) ;
				seq[5] = 0x20 + (row < MOUSE_POS_LIMIT ? row : MOUSE_POS_LIMIT) ;
				seq_len = 6 ;
			}
		}

		ml_write_to_pty( vt100_parser->pty , seq , seq_len) ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " [reported cursor pos] %d %d\n" , col , row) ;
	#endif
	}
}
