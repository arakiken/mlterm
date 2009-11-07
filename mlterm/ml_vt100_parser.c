/*
 *	$Id$
 */

#include  "ml_vt100_parser.h"

#include  <stdio.h>		/* sprintf */
#include  <string.h>		/* memmove */
#include  <stdlib.h>		/* atoi */
#include  <fcntl.h>		/* open */
#include  <unistd.h>		/* write */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* malloc/free */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_str.h>	/* kik_str_alloca_dup */
#include  <mkf/mkf_ucs4_map.h>	/* mkf_map_to_ucs4 */
#include  <mkf/mkf_ucs_property.h>
#include  <mkf/mkf_locale_ucs4_map.h>
#include  <mkf/mkf_ko_kr_map.h>

#include  "ml_iscii.h"
#include  "ml_config_proto.h"


#define  CTLKEY_BEL	0x07
#define  CTLKEY_BS	0x08
#define  CTLKEY_TAB	0x09
#define  CTLKEY_LF	0x0a
#define  CTLKEY_VT	0x0b
#define  CTLKEY_CR	0x0d
#define  CTLKEY_SO	0x0e
#define  CTLKEY_SI	0x0f
#define  CTLKEY_ESC	0x1b

#define  CURRENT_STR_P(vt100_parser)  (&vt100_parser->seq[(vt100_parser)->len - (vt100_parser)->left])

#define  HAS_XTERM_LISTENER(vt100_parser,method) \
	((vt100_parser)->xterm_listener && ((vt100_parser)->xterm_listener->method))

#define  HAS_CONFIG_LISTENER(vt100_parser,method) \
	((vt100_parser)->config_listener && ((vt100_parser)->config_listener->method))

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

/* Optimization cooperating with OPTIMIZE_REDRAWING macro defined in ml_line.c. */
#if  0
#define  IGNORE_SPACE_FG_COLOR
#endif


/* --- static functions --- */

static void
start_vt100_cmd(
	ml_vt100_parser_t *  vt100_parser
	)
{
	if( vt100_parser->use_char_combining)
	{
		ml_use_char_combining() ;
	}
	else
	{
		ml_unuse_char_combining() ;
	}

	if( vt100_parser->use_multi_col_char)
	{
		ml_use_multi_col_char() ;
	}
	else
	{
		ml_unuse_multi_col_char() ;
	}

	if( HAS_XTERM_LISTENER(vt100_parser,start))
	{
		(*vt100_parser->xterm_listener->start)( vt100_parser->xterm_listener->self) ;
	}

	ml_screen_logical( vt100_parser->screen) ;
}

static void
stop_vt100_cmd(
	ml_vt100_parser_t *  vt100_parser
	)
{
	ml_screen_render( vt100_parser->screen) ;
	ml_screen_visual( vt100_parser->screen) ;

	if( HAS_XTERM_LISTENER(vt100_parser,stop))
	{
		(*vt100_parser->xterm_listener->stop)( vt100_parser->xterm_listener->self) ;
	}
}

static size_t
receive_bytes(
	ml_vt100_parser_t *  vt100_parser
	)
{
	size_t  ret ;
	size_t  left ;

	if( vt100_parser->left > 0)
	{
		memmove( vt100_parser->seq , CURRENT_STR_P(vt100_parser) ,
			vt100_parser->left * sizeof( u_char)) ;
	}

	left = PTYMSG_BUFFER_SIZE - vt100_parser->left ;

	if( ( ret = ml_read_pty( vt100_parser->pty ,
		&vt100_parser->seq[vt100_parser->left] , left)) == 0)
	{
		return  0 ;
	}

	if( vt100_parser->logging_vt_seq)
	{
		if( vt100_parser->log_file == -1)
		{
			char *  path ;
			char *  p ;

			if( ( path = alloca( 11 + strlen( ml_pty_get_slave_name( vt100_parser->pty) + 5) + 1))
				== NULL)
			{
				goto  end ;
			}

			/* +5 removes "/dev/" */
			sprintf( path , "mlterm/%s.log" , ml_pty_get_slave_name( vt100_parser->pty) + 5) ;

			p = path + 7 ;
			while( *p)
			{
				if( *p == '/')
				{
					*p = '_' ;
				}

				p ++ ;
			}

			if( ( path = kik_get_user_rc_path( path)) == NULL)
			{
				goto  end ;
			}

			if( ( vt100_parser->log_file =
				open( path , O_CREAT | O_APPEND | O_WRONLY , 0600)) == -1)
			{
				free( path) ;

				goto  end ;
			}

			free( path) ;

			kik_file_set_cloexec( vt100_parser->log_file) ;
		}

		write( vt100_parser->log_file , &vt100_parser->seq[vt100_parser->left] , ret) ;
	#ifndef  USE_WIN32API
		fsync( vt100_parser->log_file) ;
	#endif
	} else {
		if ( vt100_parser->log_file != -1) {
			close( vt100_parser->log_file) ;
			vt100_parser->log_file = -1;
		}
        }

end:
	vt100_parser->len = ( vt100_parser->left += ret) ;

#ifdef  INPUT_DEBUG
	{
		int  count ;

		kik_debug_printf( KIK_DEBUG_TAG " pty msg (len %d) is received:" , vt100_parser->left) ;

		for( count = 0 ; count < vt100_parser->left ; count ++)
		{
		#ifdef  DUMP_HEX
			kik_msg_printf( "[%.2x]" , vt100_parser->seq[count]) ;
		#else
			kik_msg_printf( "%c" , vt100_parser->seq[count]) ;
		#endif
		}

		kik_msg_printf( "[END]\n") ;
	}
#endif

	return  1 ;
}

static int
flush_buffer(
	ml_vt100_parser_t *  vt100_parser
	)
{
	ml_char_buffer_t *  buffer ;

	buffer = &vt100_parser->buffer ;

	if( buffer->len == 0)
	{
		return  0 ;
	}
	
#ifdef  OUTPUT_DEBUG
	{
		int  count ;

		kik_msg_printf( "\nflushing chars(%d)...==>" , buffer->len) ;
		for( count = 0 ; count < buffer->len ; count ++)
		{
			char *  bytes ;

			bytes = ml_char_bytes( &buffer->chars[count]) ;

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

	(*buffer->output_func)( vt100_parser->screen , buffer->chars , buffer->len) ;

	/* buffer is cleared */
	vt100_parser->buffer.len = 0 ;

#ifdef  EDIT_DEBUG
	ml_edit_dump( vt100_parser->screen->edit) ;
#endif

	return  1 ;
}

static void
put_char(
	ml_vt100_parser_t *  vt100_parser ,
	u_char *  ch ,
	size_t  len ,
	mkf_charset_t  cs ,
	mkf_property_t  prop
	)
{
	ml_color_t  fg_color ;
	ml_color_t  bg_color ;
	int  is_biwidth ;
	int  is_comb ;

	if( vt100_parser->buffer.len == PTYMSG_BUFFER_SIZE)
	{
		flush_buffer( vt100_parser) ;
	}

	is_biwidth = 0 ;

	if( cs == ISO10646_UCS4_1)
	{
		/*
		 * checking width property of the char.
		 */
		 
		if( prop & MKF_BIWIDTH)
		{
			is_biwidth = 1 ;
		}
		else if( prop & MKF_AWIDTH)
		{
			if( vt100_parser->col_size_of_east_asian_width_a == 2)
			{
				is_biwidth = 1 ;
			}
		#if  1
			/* XTERM compatibility */
			else if( ch[0] == 0x0 && ch[1] == 0x0 && ch[2] == 0x30 &&
				(ch[3] == 0x0a || ch[3] == 0x0b || ch[3] == 0x1a || ch[3] == 0x1b) )
			{
				is_biwidth = 1 ;
			}
		#endif
		}
	}

	if( prop & MKF_COMBINING)
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

	if( vt100_parser->is_reversed)
	{
		fg_color = vt100_parser->bg_color ;
		bg_color = vt100_parser->fg_color ;
	}
	else
	{
		fg_color = vt100_parser->fg_color ;
		bg_color = vt100_parser->bg_color ;
	}

#ifdef  IGNORE_SPACE_FG_COLOR
	if( fg_color != ML_FG_COLOR && cs == US_ASCII && ch[0] == ' ' &&
		! vt100_parser->is_bold && ! vt100_parser->is_underlined)
	{
		fg_color = ML_FG_COLOR ;
	}
#endif

	if( ! vt100_parser->screen->use_dynamic_comb && is_comb)
	{
		if( vt100_parser->buffer.len == 0)
		{
			/*
			 * ml_line_set_modified() is done in ml_screen_combine_with_prev_char()
			 * internally.
			 */
			if( ml_screen_combine_with_prev_char( vt100_parser->screen ,
				ch , len , vt100_parser->cs , is_biwidth , is_comb ,
				fg_color , bg_color ,
				vt100_parser->is_bold , vt100_parser->is_underlined))
			{
				return ;
			}
		}
		else
		{
			if( ml_char_combine( &vt100_parser->buffer.chars[vt100_parser->buffer.len - 1] ,
				ch , len , vt100_parser->cs , is_biwidth , is_comb ,
				fg_color , bg_color ,
				vt100_parser->is_bold , vt100_parser->is_underlined))
			{
				return ;
			}
		}

		/*
		 * if combining failed , char is normally appended.
		 */
	}

	ml_char_set( &vt100_parser->buffer.chars[vt100_parser->buffer.len++] , ch , len ,
		vt100_parser->cs , is_biwidth , is_comb ,
		fg_color , bg_color , vt100_parser->is_bold , vt100_parser->is_underlined) ;

	if( ! vt100_parser->screen->use_dynamic_comb && cs == ISO10646_UCS4_1)
	{
		/*
		 * Arabic combining
		 */

		ml_char_t *  prev2 ;
		ml_char_t *  prev ;
		ml_char_t *  cur ;
		int  n ;

		cur = &vt100_parser->buffer.chars[vt100_parser->buffer.len - 1] ;
		n = 0 ;

		if( vt100_parser->buffer.len >= 2)
		{
			prev = cur - 1 ;
		}
		else
		{
			if( ( prev = ml_screen_get_n_prev_char( vt100_parser->screen , ++n)) == NULL)
			{
				return ;
			}
		}
		
		if( vt100_parser->buffer.len >= 3)
		{
			prev2 = cur - 2  ;
		}
		else
		{
			/* possibly NULL */
			prev2 = ml_screen_get_n_prev_char( vt100_parser->screen , ++n) ;
		}
		
		if( ml_is_arabic_combining( prev2 , prev , cur))
		{
			if( vt100_parser->buffer.len >= 2)
			{
				if( ml_char_combine( prev ,
					ch , len , vt100_parser->cs , is_biwidth , is_comb ,
					fg_color , bg_color ,
					vt100_parser->is_bold , vt100_parser->is_underlined))
				{
					vt100_parser->buffer.len -- ;
				}
			}
			else
			{
				/*
				 * ml_line_set_modified() is done in ml_screen_combine_with_prev_char()
				 * internally.
				 */
				if( ml_screen_combine_with_prev_char( vt100_parser->screen ,
					ch , len , vt100_parser->cs , is_biwidth , is_comb ,
					fg_color , bg_color ,
					vt100_parser->is_bold , vt100_parser->is_underlined))
				{
					vt100_parser->buffer.len -- ;
				}
			}
		}
	}
}

static void
save_cursor(
	ml_vt100_parser_t *  vt100_parser
	)
{
	ml_vt100_storable_states_t *  dest ;

	dest = (ml_screen_is_alternative_edit(  vt100_parser->screen) ) ?
		&(vt100_parser->saved_alternate)
		: &(vt100_parser->saved_normal) ;
	dest->is_saved = 1 ;
	dest->fg_color = vt100_parser->fg_color ;
	dest->bg_color = vt100_parser->bg_color ;
	dest->is_bold  = vt100_parser->is_bold ;
	dest->is_underlined = vt100_parser->is_underlined ;
	dest->is_reversed   = vt100_parser->is_reversed ;
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
		vt100_parser->bg_color = src->bg_color ;
		vt100_parser->is_bold  = src->is_bold ;
		vt100_parser->is_underlined = src->is_underlined ;
		vt100_parser->is_reversed = src->is_reversed ;
		if( IS_ENCODING_BASED_ON_ISO2022(vt100_parser->encoding))
		{
			if( ( src->cs == DEC_SPECIAL)
			 && ( src->cs != vt100_parser->cs) )
			{
				/* force grapchics mode by sending \E(0 to current parser*/
				u_char  DEC_SEQ[] = { CTLKEY_ESC, '(', '0'} ;
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
			if( src->cs == DEC_SPECIAL){
				vt100_parser->is_dec_special_in_gl = 1;
			}
			else
			{
				vt100_parser->is_dec_special_in_gl = 0;
			}
		}
	}
	ml_screen_restore_cursor( vt100_parser->screen) ;
}


/*
 * VT100_PARSER Escape Sequence Commands.
 */

/*
 * This function will destroy the content of pt.
 */
static int
config_protocol_set(
	ml_vt100_parser_t *  vt100_parser ,
	char *  pt
	)
{
	if( HAS_CONFIG_LISTENER(vt100_parser,set))
	{
		char *  dev ;
		char *  key ;
		char *  val ;

		stop_vt100_cmd( vt100_parser) ;

		/*
		 * accept multiple key=value pairs.
		 */
		while( pt)
		{
			if( ! ml_parse_proto( &dev , &key , &val , &pt , 0))
			{
				break ;
			}

			if( strcmp( key , "gen_proto_challenge") == 0)
			{
				ml_gen_proto_challenge() ;
			}
			else
			{
				(*vt100_parser->config_listener->set)(
					vt100_parser->config_listener->self , dev , key , val) ;
			}

			/* XXX */
			if( vt100_parser->config_listener == NULL)
			{
				/*
				 * pty changed.
				 */
				break ;
			}
		}

		start_vt100_cmd( vt100_parser) ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

/*
 * This function will destroy the content of pt.
 */
static int
config_protocol_save(
	ml_vt100_parser_t *  vt100_parser ,
	char *  pt
	)
{
	char *  file ;
	kik_conf_write_t *  conf ;
	char *  key ;
	char *  val ;

	/* XXX */
	if( ( file = kik_get_user_rc_path( "mlterm/main")) == NULL)
	{
		return  0 ;
	}

	conf = kik_conf_write_open( file) ;
	free( file) ;
	if( conf == NULL)
	{
		return  0 ;
	}

	/*
	 * accept multiple key=value pairs.
	 */
	while( pt)
	{
		if( ! ml_parse_proto( NULL , &key , &val , &pt , 0) || key == NULL)
		{
			break ;
		}

		/* XXX */
		if( strcmp( key , "encoding") == 0)
		{
			key = "ENCODING" ;
		}

		/* XXX */
		if( strcmp( key , "xim") != 0)
		{
			kik_conf_io_write( conf , key , val) ;
		}
	}

	kik_conf_write_close( conf) ;

	if( HAS_CONFIG_LISTENER(vt100_parser,saved))
	{
		(*vt100_parser->config_listener->saved)() ;
	}

	return  1 ;
}

/*
 * This function will destroy the content of pt.
 */
static int
config_protocol_get(
	ml_vt100_parser_t *  vt100_parser ,
	char *  pt ,
	int  to_menu
	)
{
	if( HAS_CONFIG_LISTENER(vt100_parser,get))
	{
		char *  dev ;
		char *  key ;
		int  ret ;
		
		stop_vt100_cmd( vt100_parser) ;

		ret = ml_parse_proto( &dev , &key , NULL , &pt , to_menu == 0) ;
		if( ret == -1)
		{
			/*
			 * do_challenge is failed.
			 * to_menu is necessarily 0, so it is pty that msg should be returned to.
			 */

			char  msg[] = "#forbidden\n" ;

			ml_write_to_pty( vt100_parser->pty , msg , sizeof( msg) - 1) ;
		}
		else if( ret > 0)
		{
			(*vt100_parser->config_listener->get)( vt100_parser->config_listener->self ,
				dev , key , to_menu) ;
		}
		else /* if( ret < 0) */
		{
			char  msg[] = "error" ;
			
			(*vt100_parser->config_listener->get)( vt100_parser->config_listener->self ,
				NULL , msg , to_menu) ;
		}

		start_vt100_cmd( vt100_parser) ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

/*
 * This function will destroy the content of pt.
 */
static int
config_protocol_set_font(
	ml_vt100_parser_t *  vt100_parser ,
	char *  pt ,
	int  save
	)
{
	if( HAS_CONFIG_LISTENER(vt100_parser,set_font))
	{
		char *  file ;
		char *  key ;
		char *  val ;

		stop_vt100_cmd( vt100_parser) ;

		if( ml_parse_proto2( &file , &key , &val , pt , 0) && key && val)
		{
			(*vt100_parser->config_listener->set_font)( file , key , val, save) ;
		}

		start_vt100_cmd( vt100_parser) ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

/*
 * This function will destroy the content of pt.
 */
static int
config_protocol_get_font(
	ml_vt100_parser_t *  vt100_parser ,
	char *  pt ,
	int  to_menu
	)
{
	if( HAS_CONFIG_LISTENER(vt100_parser,get_font))
	{
		char *  file ;
		char *  key ;
		char *  cs ;
		int  ret ;

		stop_vt100_cmd( vt100_parser) ;

		ret = ml_parse_proto2( &file , &key , NULL , pt , to_menu == 0) ;
		if( ret == -1)
		{
			/*
			 * do_challenge is failed.
			 * to_menu is necessarily 0, so it is pty that msg should be returned to.
			 */

			char  msg[] = "#forbidden\n" ;

			ml_write_to_pty( vt100_parser->pty , msg , sizeof( msg) - 1) ;
		}
		else if( ret > 0 && key && ( cs = kik_str_sep( &key , ",")) && key)
		{
			(*vt100_parser->config_listener->get_font)(
				vt100_parser->config_listener->self ,
				file ,
				key ,	/* font size */
				cs ,
				to_menu) ;
		}
		else
		{
			char  msg[] = "error" ;
			
			(*vt100_parser->config_listener->get_font)(
				vt100_parser->config_listener->self ,
				NULL , msg , msg , to_menu) ;
		}

		start_vt100_cmd( vt100_parser) ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

/*
 * This function will destroy the content of pt.
 */
static int
config_protocol_set_color(
	ml_vt100_parser_t *  vt100_parser ,
	char *  pt ,
	int  save
	)
{
	if( HAS_CONFIG_LISTENER(vt100_parser,set_color))
	{
		char *  file ;
		char *  key ;
		char *  val ;

		stop_vt100_cmd( vt100_parser) ;

		if( ml_parse_proto2( &file , &key , &val , pt , 0) && key && val)
		{
			(*vt100_parser->config_listener->set_color)( file , key , val, save) ;
		}

		start_vt100_cmd( vt100_parser) ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
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
		vt100_parser->is_underlined = 0 ;
		vt100_parser->is_reversed = 0 ;
	}
	else if( flag == 1)
	{
		/* Bold */
		vt100_parser->is_bold = 1 ;
	}
	else if( flag == 4)
	{
		/* Underscore */
		vt100_parser->is_underlined = 1 ;
	}
	else if( flag == 5)
	{
		/* Blink */
	}
	else if( flag == 7)
	{
		/* Inverse */

		vt100_parser->is_reversed = 1 ;
	}
	else if( flag == 22)
	{
		/* Bold */
		vt100_parser->is_bold = 0 ;
	}
	else if( flag == 24)
	{
		vt100_parser->is_underlined = 0 ;
	}
	else if( flag == 25)
	{
		/* blink */
	}
	else if( flag == 27)
	{
		vt100_parser->is_reversed = 0 ;
	}
	else if( flag == 30)
	{
		fg_color = ML_BLACK ;
	}
	else if( flag == 31)
	{
		fg_color = ML_RED ;
	}
	else if( flag == 32)
	{
		fg_color = ML_GREEN ;
	}
	else if( flag == 33)
	{
		fg_color = ML_YELLOW ;
	}
	else if( flag == 34)
	{
		fg_color = ML_BLUE ;
	}
	else if( flag == 35)
	{
		fg_color = ML_MAGENTA ;
	}
	else if( flag == 36)
	{
		fg_color = ML_CYAN ;
	}
	else if( flag == 37)
	{
		fg_color = ML_WHITE ;
	}
	else if( flag == 39)
	{
		/* default fg */
		fg_color = ML_FG_COLOR ;
	}
	else if( flag == 40)
	{
		bg_color = ML_BLACK ;
	}
	else if( flag == 41)
	{
		bg_color = ML_RED ;
	}
	else if( flag == 42)
	{
		bg_color = ML_GREEN ;
	}
	else if( flag == 43)
	{
		bg_color = ML_YELLOW ;
	}
	else if( flag == 44)
	{
		bg_color = ML_BLUE ;
	}
	else if( flag == 45)
	{
		bg_color = ML_MAGENTA ;
	}
	else if( flag == 46)
	{
		bg_color = ML_CYAN ;
	}
	else if( flag == 47)
	{
		bg_color = ML_WHITE ;
	}
	else if( flag == 49)
	{
		bg_color = ML_BG_COLOR ;
	}
#ifdef  DEBUG
	else
	{
		kik_warn_printf( KIK_DEBUG_TAG " unknown font attr flag(%d).\n" , flag) ;
	}
#endif

	if( fg_color != vt100_parser->fg_color)
	{
		vt100_parser->fg_color = fg_color ;
	#ifndef  IGNORE_SPACE_FG_COLOR
		ml_screen_set_bce_fg_color( vt100_parser->screen , fg_color) ;
	#endif
	}

	if( bg_color != vt100_parser->bg_color)
	{
		vt100_parser->bg_color = bg_color ;
		ml_screen_set_bce_bg_color( vt100_parser->screen , bg_color) ;
	}
}

/*
 * This function will destroy the content of pt.
 */
static int
change_color_rgb(
	ml_vt100_parser_t *  vt100_parser,
	u_char *  pt
	)
{
	char *  p ;
	
	if( ( p = strchr( pt, ';')) == NULL)
	{
		return  0 ;
	}
	*p = '=' ;

	config_protocol_set_color( vt100_parser, pt , 0) ;

	return  1 ;
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

	#ifdef  ESCSEQ_DEBUG
		if( **str_p < 0x20 || 0x7e < **str_p)
		{
			kik_msg_printf( " - 0x%x" , **str_p) ;
		}
		else
		{
			kik_msg_printf( " - %c" , **str_p) ;
		}
	#endif

		if( **str_p < 0x20 || 0x7e < **str_p)
		{
			/*
			 * cursor-control characters inside ESC sequences
			 */
			if( **str_p == CTLKEY_LF || **str_p == CTLKEY_VT)
			{
				ml_screen_line_feed( screen) ;
			}
			else if( **str_p == CTLKEY_CR)
			{
				ml_screen_goto_beg_of_line( screen) ;
			}
			else if( **str_p == CTLKEY_BS)
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
				kik_warn_printf( KIK_DEBUG_TAG " Ignored 0x%x inside escape sequences.\n" ,
					**str_p) ;
			#endif
			}
		}
		else
		{
			return  1 ;
		}
	}
}


static int
parse_vt100_escape_sequence(
	ml_vt100_parser_t *  vt100_parser
	)
{
	u_char *  str_p ;
	size_t  left ;

	if( vt100_parser->left == 0)
	{
		/* end of string */

		return  1 ;
	}

	str_p = CURRENT_STR_P(vt100_parser) ;
	left = vt100_parser->left ;

	while( 1)
	{
		if( *str_p == CTLKEY_ESC)
		{
		#ifdef  ESCSEQ_DEBUG
            		kik_msg_printf( "RECEIVED ESCAPE SEQUENCE(current left = %d: ESC", left) ;
		#endif

			if( inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0) == 0)
			{
				return  0 ;
			}

			if( *str_p == '#')
			{
				if( inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0) == 0)
				{
					return  0 ;
				}

				if( *str_p == '8')
				{
					ml_screen_fill_all_with_e( vt100_parser->screen) ;
				}
			}
			else if( *str_p == '7')
			{
				/* save cursor */

				save_cursor( vt100_parser) ;
			}
			else if( *str_p == '8')
			{
				/* restore cursor */

				restore_cursor( vt100_parser) ;
			}
			else if( *str_p == '=')
			{
				/* application keypad */

				if( HAS_XTERM_LISTENER(vt100_parser,set_app_keypad))
				{
					stop_vt100_cmd( vt100_parser) ;
					(*vt100_parser->xterm_listener->set_app_keypad)(
						vt100_parser->xterm_listener->self , 1) ;
					start_vt100_cmd( vt100_parser) ;
				}
			}
			else if( *str_p == '>')
			{
				/* normal keypad */

				if( HAS_XTERM_LISTENER(vt100_parser,set_app_keypad))
				{
					stop_vt100_cmd( vt100_parser) ;
					(*vt100_parser->xterm_listener->set_app_keypad)(
						vt100_parser->xterm_listener->self , 0) ;
					start_vt100_cmd( vt100_parser) ;
				}
			}
			else if( *str_p == 'D')
			{
				/* index(scroll up) */

				ml_screen_index( vt100_parser->screen) ;
			}
			else if( *str_p == 'E')
			{
				/* next line */

				ml_screen_line_feed( vt100_parser->screen) ;
				ml_screen_goto_beg_of_line( vt100_parser->screen) ;
			}
			else if( *str_p == 'F')
			{
				/* cursor to lower left corner of screen */

			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" cursor to lower left corner is not implemented.\n") ;
			#endif
			}
			else if( *str_p == 'H')
			{
				/* set tab */

				ml_screen_set_tab_stop( vt100_parser->screen) ;
			}
			else if( *str_p == 'M')
			{
				/* reverse index(scroll down) */

				ml_screen_reverse_index( vt100_parser->screen) ;
			}
			else if( *str_p == 'Z')
			{
				/* return terminal id */

			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" return terminal id is not implemented.\n") ;
			#endif
			}
			else if( *str_p == 'c')
			{
				/* full reset */

				clear_display_all( vt100_parser) ;

				/* XXX  is this necessary ? */
				change_char_attr( vt100_parser , 0) ;

				ml_init_encoding_parser( vt100_parser) ;
			}
			else if( *str_p == 'l')
			{
				/* memory lock */

			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " memory lock is not implemented.\n") ;
			#endif
			}
			else if( *str_p == 'm')
			{
				/* memory unlock */

			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " memory unlock is not implemented.\n") ;
			#endif
			}
			else if( *str_p == '[')
			{
				u_char  pre_ch ;
				int  ps[5] ;
				size_t  num ;

				if( inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0) == 0)
				{
					return  0 ;
				}

				if( *str_p == '?' || *str_p == '>')
				{
					pre_ch = *str_p ;

					if( inc_str_in_esc_seq( vt100_parser->screen ,
						&str_p , &left , 0) == 0)
					{
						return  0 ;
					}
				}
				else
				{
					pre_ch = '\0' ;
				}

				num = 0 ;
				while( num < 5)
				{
					while( '0' == *str_p)
					{
						if( inc_str_in_esc_seq( vt100_parser->screen ,
									&str_p , &left , 0) == 0)
						{
							return  0 ;
						}
					}
					if( '0' <= *str_p && *str_p <= '9')
					{
						char  digit[DIGIT_STR_LEN(int)] ;
						int  count ;

						digit[0] = *str_p ;

						for( count = 1 ; count < DIGIT_STR_LEN(int) -1 ; count ++)
						{
							if( inc_str_in_esc_seq( vt100_parser->screen ,
								&str_p , &left , 0) == 0)
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

						ps[num ++] = atoi( digit) ;

						if( *str_p != ';')
						{
							/*
							 * "ESC [ 0 n" is regarded as it is.
							 */
							break ;
						}
					}
					else if( *str_p == ';')
					{
						/*
						 * "ESC [ ; n " is regarded as "ESC [ 0 ; n"
						 */
						ps[num ++] = 0 ;
					}
					else
					{
						/*
						 * "ESC [ n" is regarded as "ESC [ 0 n"
						 * => this 0 is ignored after exiting this while block.
						 *
						 * "ESC [ 1 ; n" is regarded as "ESC [ 1 ; 0 n"
						 */
						ps[num ++] = 0 ;

						break ;
					}
					
					if( inc_str_in_esc_seq( vt100_parser->screen ,
						&str_p , &left , 0) == 0)
					{
						return  0 ;
					}
				}

				/*
				 * XXX
				 * 0 ps of something like ESC [ 0 n is ignored.
				 * if there are multiple ps , no ps is ignored.
				 * adhoc for vttest.
				 */
				if( num == 1 && ps[0] == 0)
				{
					num = 0 ;
				}

				if( pre_ch == '?')
				{
					if( *str_p == 'h')
					{
						/* DEC Private Mode Set */

						if( ps[0] == 1)
						{
							if( HAS_XTERM_LISTENER(vt100_parser,set_app_cursor_keys))
							{
								stop_vt100_cmd( vt100_parser) ;
								(*vt100_parser->xterm_listener->set_app_cursor_keys)(
									vt100_parser->xterm_listener->self , 1) ;
								start_vt100_cmd( vt100_parser) ;
							}
						}
					#if  0
						else if( ps[0] == 2)
						{
							/* reset charsets to USASCII */
						}
					#endif
						else if( ps[0] == 3)
						{
							clear_display_all( vt100_parser) ; /* XTERM compatibility [#1048321] */
							if( HAS_XTERM_LISTENER(vt100_parser,resize_columns))
							{
								stop_vt100_cmd( vt100_parser) ;
								(*vt100_parser->xterm_listener->resize_columns)(
									vt100_parser->xterm_listener->self , 132) ;
								start_vt100_cmd( vt100_parser) ;
							}
						}
					#if  0
						else if( ps[0] == 4)
						{
							/* smooth scrolling */
						}
					#endif
						else if( ps[0] == 5)
						{
							if( HAS_XTERM_LISTENER(vt100_parser,reverse_video))
							{
								stop_vt100_cmd( vt100_parser) ;
								(*vt100_parser->xterm_listener->reverse_video)(
									vt100_parser->xterm_listener->self , 1) ;
								start_vt100_cmd( vt100_parser) ;
							}
						}
						else if( ps[0] == 6)
						{
							/* relative origins */

							ml_screen_set_relative_origin(
								vt100_parser->screen) ;
						}
						else if( ps[0] == 7)
						{
							/* auto wrap */

							ml_screen_set_auto_wrap(
								vt100_parser->screen) ;
						}
					#if  0
						else if( ps[0] == 8)
						{
							/* auto repeat */
						}
					#endif
					#if  0
						else if( ps[0] == 9)
						{
							/* X10 mouse reporting */
						}
					#endif
						else if( ps[0] == 25)
						{
							ml_screen_cursor_visible(
								vt100_parser->screen) ;
						}
					#if  0
						else if( ps[0] == 35)
						{
							/* shift keys */
						}
					#endif
					#if  0
						else if( ps[0] == 40)
						{
							/* 80 <-> 132 */
						}
					#endif
						else if( ps[0] == 47)
						{
							/* Use Alternate Screen Buffer */

							ml_screen_use_alternative_edit(
								vt100_parser->screen) ;
						}
					#if  0
						else if( ps[0] == 66)
						{
							/* application key pad */
						}
					#endif
					#if  0
						else if( ps[0] == 67)
						{
							/* have back space */
						}
					#endif
						else if( ps[0] == 1000)
						{
							if( HAS_XTERM_LISTENER(vt100_parser,set_mouse_report))
							{
								stop_vt100_cmd( vt100_parser) ;
								(*vt100_parser->xterm_listener->set_mouse_report)(
									vt100_parser->xterm_listener->self , 1) ;
								start_vt100_cmd( vt100_parser) ;
							}
						}
					#if  0
						else if( ps[0] == 1001)
						{
							/* X11 mouse highlighting */
						}
					#endif
					#if  0
						else if( ps[0] == 1010)
						{
							/* scroll to bottom on tty output inhibit */
						}
					#endif
					#if  0
						else if( ps[0] == 1011)
						{
							/* scroll to bottom on key press */
						}
					#endif
						else if( ps[0] == 1047)
						{
							/* if( !titeInhibit)*/
							save_cursor( vt100_parser) ;
							ml_screen_use_alternative_edit(
								vt100_parser->screen) ;
							/* */
						}
						else if( ps[0] == 1048)
						{
							/* if( !titeInhibit)*/
							save_cursor( vt100_parser) ;
							/* */
						}
						else if( ps[0] == 1049)
						{
							/* if( !titeInhibit)*/
							save_cursor( vt100_parser) ;
							ml_screen_use_alternative_edit(
								vt100_parser->screen) ;
							clear_display_all( vt100_parser) ;
							/* */
						}
						else
						{
						#ifdef  DEBUG
							kik_warn_printf( KIK_DEBUG_TAG
								" ESC - [ ? %d h is not implemented.\n" ,
								ps[0]) ;
						#endif
						}
					}
					else if( *str_p == 'l')
					{
						/* DEC Private Mode Reset */

						if( ps[0] == 1)
						{
							if( HAS_XTERM_LISTENER(vt100_parser,set_app_cursor_keys))
							{
								stop_vt100_cmd( vt100_parser) ;
								(*vt100_parser->xterm_listener->set_app_cursor_keys)(
									vt100_parser->xterm_listener->self , 0) ;
								start_vt100_cmd( vt100_parser) ;
							}
						}
					#if  0
						else if( ps[0] == 2)
						{
							/* reset charsets to USASCII */
						}
					#endif
						else if( ps[0] == 3)
						{
							clear_display_all( vt100_parser) ; /* XTERM compatibility [#1048321] */
							if( HAS_XTERM_LISTENER(vt100_parser,resize_columns))
							{
								stop_vt100_cmd( vt100_parser) ;
								(*vt100_parser->xterm_listener->resize_columns)(
									vt100_parser->xterm_listener->self , 80) ;
								start_vt100_cmd( vt100_parser) ;
							}
						}
					#if  0
						else if( ps[0] == 4)
						{
							/* smooth scrolling */
						}
					#endif
						else if( ps[0] == 5)
						{
							if( HAS_XTERM_LISTENER(vt100_parser,reverse_video))
							{
								stop_vt100_cmd( vt100_parser) ;
								(*vt100_parser->xterm_listener->reverse_video)(
									vt100_parser->xterm_listener->self , 0) ;
								start_vt100_cmd( vt100_parser) ;
							}
						}
						else if( ps[0] == 6)
						{
							/* absolute origins */

							ml_screen_set_absolute_origin(
								vt100_parser->screen) ;
						}
						else if( ps[0] == 7)
						{
							/* auto wrap */
							
							ml_screen_unset_auto_wrap(
								vt100_parser->screen) ;
						}
					#if  0
						else if( ps[0] == 8)
						{
							/* auto repeat */
						}
					#endif
					#if  0
						else if( ps[0] == 9)
						{
							/* X10 mouse reporting */
						}
					#endif
						else if( ps[0] == 25)
						{
							ml_screen_cursor_invisible(
								vt100_parser->screen) ;
						}
					#if  0
						else if( ps[0] == 35)
						{
							/* shift keys */
						}
					#endif
					#if  0
						else if( ps[0] == 40)
						{
							/* 80 <-> 132 */
						}
					#endif
						else if( ps[0] == 47)
						{
							/* Use Normal Screen Buffer */
							ml_screen_use_normal_edit(
								vt100_parser->screen) ;
						}
					#if  0
						else if( ps[0] == 66)
						{
							/* application key pad */
						}
					#endif
					#if  0
						else if( ps[0] == 67)
						{
							/* have back space */
						}
					#endif
						else if( ps[0] == 1000)
						{
							if( HAS_XTERM_LISTENER(vt100_parser,set_mouse_report))
							{
								stop_vt100_cmd( vt100_parser) ;
								(*vt100_parser->xterm_listener->set_mouse_report)(
									vt100_parser->xterm_listener->self , 0) ;
								start_vt100_cmd( vt100_parser) ;
							}
						}
					#if  0
						else if( ps[0] == 1001)
						{
							/* X11 mouse highlighting */
						}
					#endif
					#if  0
						else if( ps[0] == 1010)
						{
							/* scroll to bottom on tty output inhibit */
						}
					#endif
					#if  0
						else if( ps[0] == 1011)
						{
							/* scroll to bottom on key press */
						}
					#endif
						else if( ps[0] == 1047)
						{
							/* if( !titeInhibit)*/
							clear_display_all( vt100_parser) ;
							ml_screen_use_normal_edit(
								vt100_parser->screen) ;
							restore_cursor( vt100_parser) ;
							/* */
						}
						else if( ps[0] == 1048)
						{
							/* if( !titeInhibit)*/
							restore_cursor( vt100_parser) ;
							/* */
						}
						else if( ps[0] == 1049)
						{
							/* if( !titeInhibit)*/
							ml_screen_use_normal_edit(
								vt100_parser->screen) ;
							restore_cursor( vt100_parser) ;
							/* */
						}
						else
						{
						#ifdef  DEBUG
							kik_warn_printf( KIK_DEBUG_TAG
								" ESC - [ ? %d l is not implemented.\n" ,
								ps[0]) ;
						#endif
						}
					}
					else if( *str_p == 'r')
					{
						/* Restore DEC Private Mode */
						
					#ifdef  DEBUG
						kik_warn_printf( KIK_DEBUG_TAG
							" ESC - [ ? %d r is not implemented.\n" ,
							ps[0]) ;
					#endif
					}
					else if( *str_p == 's')
					{
						/* Save DEC Private Mode */
						
					#ifdef  DEBUG
						kik_warn_printf( KIK_DEBUG_TAG
							" ESC - [ ? %d s is not implemented.\n" ,
							ps[0]) ;
					#endif
					}
				#ifdef  DEBUG
					else
					{
						kik_warn_printf( KIK_DEBUG_TAG
							" received unknown csi sequence ESC - [ - ? - %c.\n"
							, *str_p) ;
					}
				#endif
				}
				else if( pre_ch == '>')
				{
			#ifdef  DEBUG
					kik_warn_printf( KIK_DEBUG_TAG
						" received unknown csi sequence ESC - [ - > - %c.\n"
						, *str_p) ;
			#endif
				}
				else
				{
					if( *str_p == '@')
					{
						/* insert blank chars */

						if( num == 0)
						{
							ps[0] = 1 ;
						}

						/*
						 * inserting ps[0] blank characters.
						 */
						 
						ml_screen_insert_blank_chars( vt100_parser->screen ,
							ps[0]) ;
					}
					else if( *str_p == 'A' || *str_p == 'e')
					{
						if( num == 0)
						{
							ps[0] = 1 ;
						}

						ml_screen_go_upward( vt100_parser->screen , ps[0]) ;
					}
					else if( *str_p == 'B')
					{
						if( num == 0)
						{
							ps[0] = 1 ;
						}

						ml_screen_go_downward( vt100_parser->screen , ps[0]) ;
					}
					else if( *str_p == 'C' || *str_p == 'a')
					{
						if( num == 0)
						{
							ps[0] = 1 ;
						}

						ml_screen_go_forward( vt100_parser->screen , ps[0]) ;
					}
					else if( *str_p == 'D')
					{
						if( num == 0)
						{
							ps[0] = 1 ;
						}

						ml_screen_go_back( vt100_parser->screen , ps[0]) ;
					}
					else if( *str_p == 'E')
					{
						/* down and goto first column */

						if( num == 0)
						{
							ps[0] = 1 ;
						}

						ml_screen_go_downward( vt100_parser->screen , ps[0]) ;
						ml_screen_goto_beg_of_line( vt100_parser->screen) ;
					}
					else if( *str_p == 'F')
					{
						/* up and goto first column */

						if( num == 0)
						{
							ps[0] = 1 ;
						}

						ml_screen_go_upward( vt100_parser->screen , ps[0]) ;
						ml_screen_goto_beg_of_line( vt100_parser->screen) ;
					}
					else if( *str_p == 'G' || *str_p == '`')
					{
						/* cursor position absolute(CHA or HPA) */
						
						if( num == 0)
						{
							ps[0] = 1 ;
						}

						ml_screen_go_horizontally( vt100_parser->screen ,
							ps[0] - 1) ;
					}
					else if( *str_p == 'H' || *str_p == 'f')
					{
						if( num == 0)
						{
							ps[0] = 1 ;
							ps[1] = 1 ;
						}
						else
						{
							/*
							 * some applications e.g. vin sometimes use 0 :(
							 */
							if( ps[0] == 0)
							{
								ps[0] = 1 ;
							}

							if( ps[1] == 0)
							{
								ps[1] = 1 ;
							}
						}

						ml_screen_goto( vt100_parser->screen ,
							ps[1] - 1 , ps[0] - 1) ;
					}
					else if( *str_p == 'I')
					{
						/* cursor forward tabulation */
						
						if( num == 0)
						{
							ps[0] = 1 ;
						}
						
						ml_screen_vertical_forward_tabs(
							vt100_parser->screen , ps[0]) ;
					}
					else if( *str_p == 'J')
					{
						/* Erase in Display */

						if( num == 0 || ps[0] == 0)
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
						/* Erase in Line */

						if( num == 0 || ps[0] == 0)
						{
							ml_screen_clear_line_to_right(
								vt100_parser->screen) ;
						}
						else if( ps[0] == 1)
						{
							ml_screen_clear_line_to_left(
								vt100_parser->screen) ;
						}
						else if( ps[0] == 2)
						{
							clear_line_all( vt100_parser) ;
						}
					}
					else if( *str_p == 'L')
					{
						if( num == 0)
						{
							ps[0] = 1 ;
						}

						ml_screen_insert_new_lines(
							vt100_parser->screen , ps[0]) ;
					}
					else if( *str_p == 'M')
					{
						if( num == 0)
						{
							ps[0] = 1 ;
						}

						ml_screen_delete_lines(
							vt100_parser->screen , ps[0]) ;
					}
					else if( *str_p == 'P')
					{
						/* delete chars */

						if( num == 0)
						{
							ps[0] = 1 ;
						}

						ml_screen_delete_cols( vt100_parser->screen , ps[0]) ;
					}
					else if( *str_p == 'S')
					{
						/* scroll up */

						if( num == 0)
						{
							ps[0] = 1 ;
						}
						
						ml_screen_scroll_upward( vt100_parser->screen , ps[0]) ;
					}
					else if( *str_p == 'T')
					{
						/* scroll down */

						if( num == 0)
						{
							ps[0] = 1 ;
						}
						
						ml_screen_scroll_downward( vt100_parser->screen , ps[0]) ;
					}
					else if( *str_p == '^')
					{
						/* initiate hilite mouse tracking. */

					#ifdef  DEBUG
						kik_warn_printf( KIK_DEBUG_TAG
							" ESC - [ - ^ is not implemented.\n") ;
					#endif
					}
					else if( *str_p == 'X')
					{
						/* erase characters */

						if( num == 0)
						{
							ps[0] = 1 ;
						}

						ml_screen_clear_cols( vt100_parser->screen , ps[0]) ;
					}
					else if( *str_p == 'Z')
					{
						/* cursor backward tabulation */

						if( num == 0)
						{
							ps[0] = 1 ;
						}
						
						ml_screen_vertical_backward_tabs(
							vt100_parser->screen , ps[0]) ;
					}
					else if( *str_p == 'c')
					{
						/* send device attributes */
						
						/* vt100 answerback */
						char  seq[] = "\x1b[?1;2c" ;
						
						ml_write_to_pty( vt100_parser->pty , seq , strlen( seq)) ;
					}
					else if( *str_p == 'd')
					{
						/* line position absolute(VPA) */

						if( num == 0)
						{
							ps[0] = 1 ;
						}

						ml_screen_go_vertically( vt100_parser->screen ,
							ps[0] - 1) ;
					}
					else if( *str_p == 'g')
					{
						/* tab clear */

						if( num == 0)
						{
							ml_screen_clear_tab_stop(
								vt100_parser->screen) ;
						}
						else if( num == 1 && ps[0] == 3)
						{
							ml_screen_clear_all_tab_stops(
								vt100_parser->screen) ;
						}
					}
					else if( *str_p == 'l')
					{
						if( num == 1)
						{
							if( ps[0] == 4)
							{
								/* replace mode */

								vt100_parser->buffer.output_func =
									ml_screen_overwrite_chars ;
							}
						}
					}
					else if( *str_p == 'h')
					{
						if( num == 1)
						{
							if( ps[0] == 4)
							{
								/* insert mode */

								vt100_parser->buffer.output_func =
									ml_screen_insert_chars ;
							}
						}
					}
					else if( *str_p == 'm')
					{
						int  count ;
						
						if( num == 0)
						{
							ps[0] = 0 ;
							num = 1 ;
						}

						for( count = 0 ; count < num ; count ++)
						{
							if( ps[count] == 38 && num >= 3
								&& ps[count + 1] == 5)
							{
								vt100_parser->fg_color =
									ps[count + 2] ;
								count += 2 ;
							}
							else if( ps[count] == 48 && num >= 3
								&& ps[count + 1] == 5)
							{
								vt100_parser->bg_color =
									ps[count + 2] ;
								count += 2 ;
							}
							else
							{
								change_char_attr( vt100_parser ,
									ps[count]) ;
							}
						}
					}
					else if( *str_p == 'n')
					{
						/* device status report */

						if( num == 1)
						{
							if( ps[0] == 5)
							{
								ml_write_to_pty( vt100_parser->pty ,
									"\x1b[0n" , 4) ;
							}
							else if( ps[0] == 6)
							{
								char  seq[4 + DIGIT_STR_LEN(u_int) + 1] ;

								sprintf( seq , "\x1b[%d;%dR" ,
									ml_screen_cursor_row( vt100_parser->screen) + 1 ,
									ml_screen_cursor_col( vt100_parser->screen) + 1) ;

								ml_write_to_pty( vt100_parser->pty ,
									seq , strlen( seq)) ;
							}
						}
					}
					else if( *str_p == 'r')
					{
						/* set scroll region */
						
						if( num == 2)
						{
							ml_screen_set_scroll_region(
								vt100_parser->screen ,
								ps[0] - 1 , ps[1] - 1) ;
						}
					}
					else if( *str_p == 's')
					{
						save_cursor( vt100_parser) ;
					}
					else if( *str_p == 'u')
					{
						restore_cursor( vt100_parser) ;
					}
					else if( *str_p == 'x')
					{
						/* request terminal parameters */

						/* XXX the same as rxvt */

						if( num == 0)
						{
							ps[0] = 0 ;
						}
						
						if( ps[0] == 0 || ps[0] == 1)
						{
							char seq[] = "\x1b[X;1;1;112;112;1;0x" ;

							/* '+ 0x30' lets int to char */
							seq[2] = ps[0] + 2 + 0x30 ;
							
							ml_write_to_pty( vt100_parser->pty ,
								seq , sizeof( seq)) ;
						}
					}
				#ifdef  DEBUG
					else
					{
						kik_warn_printf( KIK_DEBUG_TAG
							" received unknown csi sequence ESC - [ - 0x%x.\n" ,
							*str_p) ;
					}
				#endif
				}
			}
			else if( *str_p == ']')
			{
				char  digit[10] ;
				int  count ;
				int  ps ;
				u_char *  pt ;

				if( inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0) == 0)
				{
					return  0 ;
				}

				count = 0 ;
				while( '0' <= *str_p && *str_p <= '9')
				{
					digit[count++] = *str_p ;

					if( inc_str_in_esc_seq( vt100_parser->screen ,
						&str_p , &left , 0) == 0)
					{
						return  0 ;
					}
				}

				digit[count] = '\0' ;

				/* if digit is illegal , ps is set 0. */
				ps = atoi( digit) ;

				if( *str_p == ';')
				{
					u_char  prev_ch ;
					
					if( inc_str_in_esc_seq( vt100_parser->screen ,
						&str_p , &left , 1) == 0)
					{
						return  0 ;
					}
					
					pt = str_p ;
					prev_ch = 0 ;
					/* OSC is terminated by BEL or ST(ESC \) */
					while( *str_p != CTLKEY_BEL &&
						( prev_ch != CTLKEY_ESC || *str_p != '\\'))
					{
						prev_ch = *str_p ;
						
						if( *str_p == CTLKEY_LF)
						{
							/* stop to parse as escape seq. */
							return  1 ;
						}
						
						if( inc_str_in_esc_seq( vt100_parser->screen ,
							&str_p , &left , 1) == 0)
						{
							return  0 ;
						}
					}

					if( *str_p == '\\')
					{
						*(str_p - 1) = '\0' ;
					}
					else
					{
						*str_p = '\0' ;
					}

					if( ps == 0)
					{
						/* change icon name and window title */
						
						if( HAS_XTERM_LISTENER(vt100_parser,set_window_name))
						{
							stop_vt100_cmd( vt100_parser) ;
							(*vt100_parser->xterm_listener->set_window_name)(
								vt100_parser->xterm_listener->self , pt) ;
							start_vt100_cmd( vt100_parser) ;
						}
						
						if( HAS_XTERM_LISTENER(vt100_parser,set_icon_name))
						{
							stop_vt100_cmd( vt100_parser) ;
							(*vt100_parser->xterm_listener->set_icon_name)(
								vt100_parser->xterm_listener->self , pt) ;
							start_vt100_cmd( vt100_parser) ;
						}
					}
					else if( ps == 1)
					{
						/* change icon name */
						
						if( HAS_XTERM_LISTENER(vt100_parser,set_icon_name))
						{
							stop_vt100_cmd( vt100_parser) ;
							(*vt100_parser->xterm_listener->set_icon_name)(
								vt100_parser->xterm_listener->self , pt) ;
							start_vt100_cmd( vt100_parser) ;
						}
					}
					else if( ps == 2)
					{
						/* change window title */
						
						if( HAS_XTERM_LISTENER(vt100_parser,set_window_name))
						{
							stop_vt100_cmd( vt100_parser) ;
							(*vt100_parser->xterm_listener->set_window_name)(
								vt100_parser->xterm_listener->self , pt) ;
							start_vt100_cmd( vt100_parser) ;
						}
					}
					else if( ps == 4)
					{
						/* change 256 color */
						if( ! change_color_rgb( vt100_parser, pt))
						{
						#ifdef  DEBUG
							kik_debug_printf( KIK_DEBUG_TAG
							" change_color_rgb failed.\n") ;
						#endif
						}
					}
					else if( ps == 20)
					{
						if( HAS_CONFIG_LISTENER(vt100_parser,set))
						{
							/* edit commands */
							char *  p ;

							/* XXX discard all adjust./op. settings.*/
							/* XXX may break multi-byte character string. */
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
								 * Do not change current edit but alter
								 * diaplay setting.
								 * XXX nothing can be done for now.
								 */

								return  0 ;
							}

							stop_vt100_cmd( vt100_parser) ;
							(*vt100_parser->config_listener->set)(
								vt100_parser->config_listener->self ,
								NULL , "wall_picture" , pt) ;
							start_vt100_cmd( vt100_parser) ;
						}
					}
					else if( ps == 39)
					{
						if( HAS_CONFIG_LISTENER(vt100_parser,set))
						{
							stop_vt100_cmd( vt100_parser) ;
							(*vt100_parser->config_listener->set)(
								vt100_parser->config_listener->self ,
								NULL , "fg_color" , pt) ;
							start_vt100_cmd( vt100_parser) ;
						}
					}
					else if( ps == 46)
					{
						/* change log file */
					}
					else if( ps == 49)
					{
						if( HAS_CONFIG_LISTENER(vt100_parser,set))
						{
							stop_vt100_cmd( vt100_parser) ;
							(*vt100_parser->config_listener->set)(
								vt100_parser->config_listener->self ,
								NULL , "bg_color" , pt) ;
							start_vt100_cmd( vt100_parser) ;
						}
					}
					else if( ps == 50)
					{
						/* set font */
					}
					else if( ps == 5379)
					{
						/* set */
						
						config_protocol_set( vt100_parser , pt) ;
					}
					else if( ps == 5380)
					{
						/* get */

						config_protocol_get( vt100_parser , pt , 0) ;
					}
					else if( ps == 5381)
					{
						/* get(menu) */

						config_protocol_get( vt100_parser , pt , 1) ;
					}
					else if( ps == 5382)
					{
						/* save */
						
						config_protocol_save( vt100_parser , pt) ;
					}
					else if( ps == 5383)
					{
						/* set&save */
						
						char *  p ;

						p = kik_str_alloca_dup( pt) ;
						
						config_protocol_set( vt100_parser , pt) ;

						if( p)
						{
							config_protocol_save( vt100_parser , p) ;
						}
					}
					else if( ps == 5384)
					{
						/* set font */

						config_protocol_set_font( vt100_parser, pt, 0) ;
					}
					else if( ps == 5385)
					{
						/* set font(pty) */

						config_protocol_get_font( vt100_parser, pt, 0) ;
					}
					else if( ps == 5386)
					{
						/* set font(GUI menu) */

						config_protocol_get_font( vt100_parser, pt, 1) ;
					}
					else if( ps == 5387)
					{
						/* set&save font */
						config_protocol_set_font( vt100_parser, pt, 1) ;
					}
					else if( ps == 5388)
					{
						/* set font */

						config_protocol_set_color( vt100_parser, pt, 0) ;
					}
					else if( ps == 5391)
					{
						/* set&save font */
						config_protocol_set_color( vt100_parser, pt, 1) ;
					}
				#ifdef  DEBUG
					else
					{
						kik_warn_printf( KIK_DEBUG_TAG
							" unknown sequence ESC - ] - %d - ; - %s  is received.\n" ,
							ps , pt) ;
					}
				#endif
				}
			#ifdef  DEBUG
				else
				{
					kik_warn_printf( KIK_DEBUG_TAG
						" unknown sequence ESC - ] - %c is received.\n" , *str_p) ;
				}
			#endif
			}
			else if( *str_p == '(')
			{
				if( IS_ENCODING_BASED_ON_ISO2022(vt100_parser->encoding))
				{
					/* not VT100 control sequence */

					return  1 ;
				}

				if( inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0) == 0)
				{
					return  0 ;
				}

				if( *str_p == '0')
				{
					vt100_parser->is_dec_special_in_g0 = 1 ;
				}
				else if( *str_p == 'B')
				{
					vt100_parser->is_dec_special_in_g0 = 0 ;
				}
				else
				{
					/* not VT100 control sequence */

					return  1 ;
				}

				if( ! vt100_parser->is_so)
				{
					vt100_parser->is_dec_special_in_gl =
						vt100_parser->is_dec_special_in_g0 ;
				}
			}
			else if( *str_p == ')')
			{
				if( IS_ENCODING_BASED_ON_ISO2022(vt100_parser->encoding))
				{
					/* not VT100 control sequence */

					return  1 ;
				}

				/*
				 * ignored.
				 */
				 
				if( inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0) == 0)
				{
					return  0 ;
				}
			
				if( *str_p == '0')
				{
					vt100_parser->is_dec_special_in_g1 = 1 ;
				}
				else if( *str_p == 'B')
				{
					vt100_parser->is_dec_special_in_g1 = 0 ;
				}
				else
				{
					/* not VT100 control sequence */

					return  1 ;
				}
				
				if( vt100_parser->is_so)
				{
					vt100_parser->is_dec_special_in_gl =
						vt100_parser->is_dec_special_in_g1 ;
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
		else if( *str_p == CTLKEY_SI)
		{
			if( IS_ENCODING_BASED_ON_ISO2022(vt100_parser->encoding))
			{
				/* not VT100 control sequence */
				
				return  1 ;
			}
			
		#ifdef  ESCSEQ_DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " receiving SI\n") ;
		#endif

			vt100_parser->is_dec_special_in_gl = vt100_parser->is_dec_special_in_g0 ;
			vt100_parser->is_so = 0 ;
		}
		else if( *str_p == CTLKEY_SO)
		{
			if( IS_ENCODING_BASED_ON_ISO2022(vt100_parser->encoding))
			{
				/* not VT100 control sequence */

				return  1 ;
			}
			
		#ifdef  ESCSEQ_DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " receiving SO\n") ;
		#endif

			vt100_parser->is_dec_special_in_gl = vt100_parser->is_dec_special_in_g1 ;
			vt100_parser->is_so = 1 ;
		}
		else if( *str_p == CTLKEY_LF || *str_p == CTLKEY_VT)
		{
		#ifdef  ESCSEQ_DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " receiving LF\n") ;
		#endif

			ml_screen_line_feed( vt100_parser->screen) ;
		}
		else if( *str_p == CTLKEY_CR)
		{
		#ifdef  ESCSEQ_DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " receiving CR\n") ;
		#endif

			ml_screen_goto_beg_of_line( vt100_parser->screen) ;
		}
		else if( *str_p == CTLKEY_TAB)
		{
		#ifdef  ESCSEQ_DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " receiving TAB\n") ;
		#endif

			ml_screen_vertical_forward_tabs( vt100_parser->screen , 1) ;
		}
		else if( *str_p == CTLKEY_BS)
		{
		#ifdef  ESCSEQ_DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " receiving BS\n") ;
		#endif

			ml_screen_go_back( vt100_parser->screen , 1) ;
		}
		else if( *str_p == CTLKEY_BEL)
		{
		#ifdef  ESCSEQ_DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " receiving BEL\n") ;
		#endif

			if( HAS_XTERM_LISTENER(vt100_parser,bel))
			{
				stop_vt100_cmd( vt100_parser) ;
				(*vt100_parser->xterm_listener->bel)( vt100_parser->xterm_listener->self) ;
				start_vt100_cmd( vt100_parser) ;
			}
		}
		else
		{
			/* not VT100 control sequence */
			
			return  1 ;
		}

	#ifdef  EDIT_DEBUG
		ml_edit_dump( vt100_parser->screen->edit) ;
	#endif

		left -- ;
		str_p ++ ;
		
		if( ( vt100_parser->left = left) == 0)
		{
			return  1 ;
		}
	}
}

 
/* --- global functions --- */

ml_vt100_parser_t *
ml_vt100_parser_new(
	ml_screen_t *  screen ,
	ml_char_encoding_t  encoding ,
	ml_unicode_font_policy_t  policy ,
	u_int  col_size_a ,
	int  use_char_combining ,
	int  use_multi_col_char
	)
{
	ml_vt100_parser_t *  vt100_parser ;

	if( ( vt100_parser = malloc( sizeof( ml_vt100_parser_t))) == NULL)
	{
		return  NULL ;
	}

	vt100_parser->left = 0 ;
	vt100_parser->len = 0 ;

	ml_str_init( vt100_parser->buffer.chars , PTYMSG_BUFFER_SIZE) ;	
	vt100_parser->buffer.len = 0 ;
	vt100_parser->buffer.output_func = ml_screen_overwrite_chars ;

	vt100_parser->screen = screen ;
	vt100_parser->pty = NULL ;

	vt100_parser->xterm_listener = NULL ;
	vt100_parser->config_listener = NULL ;

	vt100_parser->log_file = -1 ;
	
	vt100_parser->cs = UNKNOWN_CS ;
	vt100_parser->fg_color = ML_FG_COLOR ;
	vt100_parser->bg_color = ML_BG_COLOR ;
	vt100_parser->is_bold = 0 ;
	vt100_parser->is_underlined = 0 ;
	vt100_parser->is_reversed = 0 ;
	vt100_parser->use_char_combining = use_char_combining ;
	vt100_parser->use_multi_col_char = use_multi_col_char ;
	vt100_parser->logging_vt_seq = 0 ;

	vt100_parser->unicode_font_policy = policy ;

	if( ( vt100_parser->cc_conv = ml_conv_new( encoding)) == NULL)
	{
		goto  error ;
	}

	if( ( vt100_parser->cc_parser = ml_parser_new( encoding)) == NULL)
	{
		(*vt100_parser->cc_conv->delete)( vt100_parser->cc_conv) ;

		goto  error ;
	}

	vt100_parser->encoding = encoding ;

	vt100_parser->is_dec_special_in_gl = 0 ;
	vt100_parser->is_so = 0 ;
	vt100_parser->is_dec_special_in_g0 = 0 ;
	vt100_parser->is_dec_special_in_g1 = 1 ;

	if( col_size_a == 1 || col_size_a == 2)
	{
		vt100_parser->col_size_of_east_asian_width_a = col_size_a ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " col size should be 1 or 2. default value 1 is used.\n") ;
	#endif
	
		vt100_parser->col_size_of_east_asian_width_a = 1 ;
	}

	vt100_parser->saved_normal.is_saved = 0 ;
        vt100_parser->saved_alternate.is_saved = 0 ;
	return  vt100_parser ;

error:
	free( vt100_parser) ;

	return  NULL ;
}

int
ml_vt100_parser_delete(
	ml_vt100_parser_t *  vt100_parser
	)
{
	ml_str_final( vt100_parser->buffer.chars , PTYMSG_BUFFER_SIZE) ;
	(*vt100_parser->cc_parser->delete)( vt100_parser->cc_parser) ;
	(*vt100_parser->cc_conv->delete)( vt100_parser->cc_conv) ;

	if( vt100_parser->log_file != -1)
	{
		close( vt100_parser->log_file) ;
	}

	free( vt100_parser) ;
	
	return  1 ;
}

int
ml_vt100_parser_set_pty(
	ml_vt100_parser_t *  vt100_parser ,
	ml_pty_ptr_t  pty
	)
{
	vt100_parser->pty = pty ;
	
	return  1 ;
}

int
ml_vt100_parser_set_xterm_listener(
	ml_vt100_parser_t *  vt100_parser ,
	ml_xterm_event_listener_t *  xterm_listener
	)
{
	vt100_parser->xterm_listener = xterm_listener ;

	return  1 ;
}

int
ml_vt100_parser_set_config_listener(
	ml_vt100_parser_t *  vt100_parser ,
	ml_config_event_listener_t *  config_listener
	)
{
	vt100_parser->config_listener = config_listener ;

	return  1 ;
}

int
ml_vt100_parser_set_unicode_font_policy(
	ml_vt100_parser_t *  vt100_parser ,
	ml_unicode_font_policy_t  policy
	)
{
	vt100_parser->unicode_font_policy = policy ;

	return  1 ;
}

int
ml_parse_vt100_sequence(
	ml_vt100_parser_t *  vt100_parser
	)
{
	mkf_char_t  ch ;
	size_t  prev_left ;

	if( receive_bytes( vt100_parser) == 0)
        {
            	return  0 ;
        }
  
	start_vt100_cmd( vt100_parser) ;

	/*
	 * bidi and visual-indian is always stopped from here.
	 */
		 
	while( 1)
	{
		prev_left = vt100_parser->left ;

		/*
		 * parsing character encoding.
		 */
		(*vt100_parser->cc_parser->set_str)( vt100_parser->cc_parser ,
			CURRENT_STR_P(vt100_parser) , vt100_parser->left) ;
		while( (*vt100_parser->cc_parser->next_char)( vt100_parser->cc_parser , &ch))
		{
			/*
			 * UCS <-> OTHER CS
			 */
			if( ch.cs == ISO10646_UCS4_1)
			{
				if( ch.ch[0] == 0x00 && ch.ch[1] == 0x00 &&
					ch.ch[2] == 0x00 && ch.ch[3] <= 0x7f
					)
				{
					/* this is always done */
					ch.ch[0] = ch.ch[3] ;
					ch.size = 1 ;
					ch.cs = US_ASCII ;
				}
				else if( vt100_parser->unicode_font_policy == NOT_USE_UNICODE_FONT)
				{
					/* convert ucs4 to appropriate charset */

					mkf_char_t  non_ucs ;
                                
					if( mkf_map_locale_ucs4_to( &non_ucs , &ch) == 0)
					{
					#ifdef  DEBUG
						kik_warn_printf( KIK_DEBUG_TAG
						" failed to convert ucs4 to other cs.\n") ;
					#endif
						continue ;
					}
					else
					{
						ch = non_ucs ;
					}
				}
			}
			else if( ( (vt100_parser->unicode_font_policy == ONLY_USE_UNICODE_FONT)
				&& ch.cs != US_ASCII)
			#if  0
				/* GB18030_2000 2-byte chars(==GBK) are converted to UCS */
				|| ( vt100_parser->encoding == ML_GB18030 && ch.cs == GBK)
			#endif
				/*
				 * XXX
				 * converting japanese gaiji to ucs.
				 */
				|| ch.cs == JISC6226_1978_NEC_EXT
				|| ch.cs == JISC6226_1978_NECIBM_EXT
				|| ch.cs == JISX0208_1983_MAC_EXT
				|| ch.cs == SJIS_IBM_EXT
				)
			{
				mkf_char_t  ucs ;

				if( mkf_map_to_ucs4( &ucs , &ch))
				{
					ucs.property = mkf_get_ucs_property(
							mkf_bytes_to_int( ucs.ch , ucs.size)) ;
					
					ch = ucs ;
				}
			#ifdef  DEBUG
				else
				{
					kik_warn_printf( KIK_DEBUG_TAG
						" mkf_convert_to_ucs4_char() failed.\n") ;
				}
			#endif
			}

			/*
			 * NON UCS <-> NON UCS
			 */
			
			{
				/*
				 * XXX hack
				 * how to deal with johab 10-4-4(8-4-4) font ?
				 * is there any uhc font ?
				 */
				 
				mkf_char_t  uhc ;

				if( ch.cs == JOHAB)
				{
					if( mkf_map_johab_to_uhc( &uhc , &ch) == 0)
					{
						continue ;
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
					if( mkf_map_uhc_to_ksc5601_1987( &ch , &uhc) == 0)
					{
						continue ;
					}
				}
			}

			if( ch.size == 1 && ch.ch[0] == 0x0)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" 0x0 sequence is received , ignored...\n") ;
			#endif
			}
			else if( ch.size == 1 && 0x1 <= ch.ch[0] && ch.ch[0] <= 0x1f)
			{
				/*
				 * this is a control sequence.
				 * reparsing this char in vt100_escape_sequence() ...
				 */

				vt100_parser->cc_parser->left ++ ;
				vt100_parser->cc_parser->is_eos = 0 ;

				break ;
			}
			else
			{
				vt100_parser->left = vt100_parser->cc_parser->left ;

				if( ml_is_msb_set( ch.cs))
				{
					SET_MSB( ch.ch[0]) ;
				}
				if( ( ch.cs == US_ASCII && vt100_parser->is_dec_special_in_gl) ||
					ch.cs == DEC_SPECIAL)
				{
					if( ch.ch[0] == 0x5f)
					{
						ch.ch[0] = 0x7f ;
					}
					else if( 0x5f < ch.ch[0] && ch.ch[0] < 0x7f)
					{
						ch.ch[0] -= 0x5f ;
					}

					ch.cs = DEC_SPECIAL ;
					ch.property = 0 ;
				}

				put_char( vt100_parser , ch.ch , ch.size , ch.cs ,
					ch.property) ;
			}
		}

		vt100_parser->left = vt100_parser->cc_parser->left ;

		flush_buffer( vt100_parser) ;

		if( vt100_parser->cc_parser->is_eos)
		{
			break ;
		}

		/*
		 * parsing other vt100 sequences.
		 */

		if( ! parse_vt100_escape_sequence( vt100_parser))
		{
			/* shortage of chars */
			if(vt100_parser->left >= PTYMSG_BUFFER_SIZE){
				/* the sequence seems to be	 longer than PTY buffer, or
				 * broken/unsupported.
				 * try to recover from error by dropping bytes... */
#ifdef DEBUG
				kik_debug_printf( KIK_DEBUG_TAG
					  "escape sequence too long. dropped\n") ;
#endif
				vt100_parser->left--;
			}else{
				/* expect more input */
				break ;
			}
		}

	#ifdef  EDIT_ROUGH_DEBUG
		ml_edit_dump( vt100_parser->screen->edit) ;
	#endif

		if( vt100_parser->left == prev_left)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				" unrecognized sequence[%.2x] is received , ignored...\n" ,
				*CURRENT_STR_P(vt100_parser)) ;
		#endif

			vt100_parser->left -- ;
		}

		if( vt100_parser->left == 0)
		{
			break ;
		}
	}
		
	stop_vt100_cmd( vt100_parser) ;

	return  1 ;
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
	vt100_parser->is_dec_special_in_gl = 0 ;
	vt100_parser->is_so = 0 ;
	vt100_parser->is_dec_special_in_g0 = 0 ;
	vt100_parser->is_dec_special_in_g1 = 1 ;
	
	return  1 ;
}

ml_char_encoding_t
ml_vt100_parser_get_encoding(
	ml_vt100_parser_t *  vt100_parser
	)
{
	return  vt100_parser->encoding ;
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
	vt100_parser->is_dec_special_in_gl = 0 ;
	vt100_parser->is_so = 0 ;
	vt100_parser->is_dec_special_in_g0 = 0 ;
	vt100_parser->is_dec_special_in_g1 = 1 ;

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
ml_vt100_parser_set_logging_vt_seq(
        ml_vt100_parser_t *  vt100_parser ,
        int  flag
	)
{
	vt100_parser->logging_vt_seq = flag ;

	return  1 ;
}
