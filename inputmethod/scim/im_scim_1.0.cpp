//
// im_scim_1.0.cpp - SCIM plugin for mlterm (c++ part)
//
// Copyright (C) 2005 Seiichi SATO <ssato@sh.rim.or.jp>
//
//	$Id$
//

//
// This file is partially based on gtkimcontextscim.cpp of SCIM
//
// Copyright (C) 2002-2005 James Su <suzhe@tsinghua.org.cn>
//
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this program; if not, write to the
// Free Software Foundation, Inc., 59 Temple Place, Suite 330,
// Boston, MA  02111-1307  USA
//


#include  <iostream>
#include  <stdio.h>
#include  <X11/Xlib.h>  // KeySym, XKeyEvent

#include  "im_scim.h"

#define  Uses_SCIM_CONFIG_PATH
#define  Uses_SCIM_IMENGINE_MODULE
#define  Uses_SCIM_BACKEND
#define  Uses_SCIM_PANEL
#define  Uses_SCIM_SOCKET_TRANSACTION
#include  <scim.h>

#define  SCIM_TRANS_MAGIC 0x4d494353

#define  TIMEOUT  5000	// msec

using namespace scim ;

typedef struct  im_scim_context_private
{
	String  factory ;
	u_int  instance ;

	int  on ;
	int  focused ;

	WideString  preedit_str ;
	AttributeList  preedit_attr ;
	int  preedit_caret ;

	void *  self ;
	im_scim_callbacks_t *  cb ;

}  im_scim_context_private_t ;

#define transaction_init(instance)			\
do {							\
	int  cmd ;					\
	uint32  data ;					\
	sock.clear() ;					\
	sock.put_command( SCIM_TRANS_CMD_REQUEST) ;	\
	sock.put_data( panel_key) ;			\
	sock.put_data( (uint32)(instance)) ;		\
	sock.get_command( cmd) ;			\
	sock.get_data( data) ;				\
	sock.get_data( data) ;				\
}  while(0)

#define C_STR(s) (char*) ( utf8_wcstombs((s)).c_str())

#define  KEY_OTHER		0
#define  KEY_TRIGER		1
#define  KEY_FACTORY_MENU	2
#define  KEY_FACTORY_NEXT	3
#define  KEY_FACTORY_PREV	4

// --- static variables ---

static std::vector <im_scim_context_private_t*>  context_table ;

static String  lang ;

static ConfigModule *  config_module = NULL ;
static ConfigPointer  config = NULL ;

static BackEndPointer be = NULL ;
static std::vector<String>  factories ;

static SocketClient  panel ;
static uint32  panel_key ;

static SocketTransaction  sock ;

static KeyEventList  key_list_trigger ;
static KeyEventList  key_list_factory_menu ;
static KeyEventList  key_list_factory_menu_next ;
static KeyEventList  key_list_factory_menu_prev ;

static bool  is_vertical_lookup ;

// --- static functions ---

static int
kind_of_key(
	int  ksym ,
	int  mask
	)
{
	std::vector <KeyEvent>::const_iterator  key ;

	for( key = key_list_trigger.begin() ;
	     key != key_list_trigger.end() ;
	     key ++)
	{
		if( ksym == key->code && mask == key->mask)
		{
			return KEY_TRIGER ;
		}
	}

	for( key = key_list_factory_menu.begin() ;
	     key != key_list_factory_menu.end() ;
	     key ++)
	{
		if( ksym == key->code && mask == key->mask)
		{
			return KEY_FACTORY_MENU ;
		}
	}

	for( key = key_list_factory_menu_next.begin() ;
	     key != key_list_factory_menu_next.end() ;
	     key ++)
	{
		if( ksym == key->code && mask == key->mask)
		{
			return KEY_FACTORY_NEXT ;
		}
	}

	for( key = key_list_factory_menu_prev.begin() ;
	     key != key_list_factory_menu_prev.end() ;
	     key ++)
	{
		if( ksym == key->code && mask == key->mask)
		{
			return KEY_FACTORY_PREV ;
		}
	}

	return  KEY_OTHER ;
}

static void
load_config( const ConfigPointer &  config)
{
	scim_string_to_key_list( key_list_trigger , config->read( String( SCIM_CONFIG_FRONTEND_KEYS_TRIGGER) , String( "Control+space"))) ;

	scim_string_to_key_list( key_list_factory_menu , config->read( String( SCIM_CONFIG_FRONTEND_KEYS_SHOW_FACTORY_MENU) , String( "Control+Alt+l,Control+Alt+m,Control+Alt+s,Control+Alt+Right"))) ;

	scim_string_to_key_list( key_list_factory_menu_next , config->read( String( SCIM_CONFIG_FRONTEND_KEYS_NEXT_FACTORY) , String( "Control+Alt+Down,Control+Shift_R,Control+Shift_L"))) ;

	scim_string_to_key_list( key_list_factory_menu_prev , config->read( String( SCIM_CONFIG_FRONTEND_KEYS_PREVIOUS_FACTORY) , String( "Control+Alt+Up,Shift+Control_R,Shift+Control_L"))) ;

	// hack
	is_vertical_lookup = config->read( String( "/Panel/Gtk/LookupTableVertical") , false) ;
}

static im_scim_context_private_t *
instance_to_context(
	u_int  instance
	)
{
	size_t i ;

	for( i = 0 ; i < context_table.size() ; i++)
	{
		if( context_table[i]->instance == instance)
		{
			return  context_table[i] ;
		}
	}

	return  NULL ;
}


static void
send_factory_menu_item(
	int instance
	)
{
	size_t  i ;

	transaction_init( instance) ;
	sock.put_command( SCIM_TRANS_CMD_PANEL_SHOW_FACTORY_MENU) ;
	for( i = 0 ; i < factories.size() ; i++)
	{
		sock.put_data( factories[i]) ;
		sock.put_data( utf8_wcstombs( be->get_factory_name( factories[i])));
		sock.put_data( be->get_factory_language( factories[i]));
		sock.put_data( be->get_factory_icon_file( factories[i]));
	}
	sock.write_to_socket(panel , SCIM_TRANS_MAGIC);
}

static void
send_help_description(
	int  instance
	)
{
	String  desc ;
	String  str ;

	desc = String( "Hot keys\n\n") ;

	scim_key_list_to_string( str , key_list_trigger) ;
	desc += String( "  ") + str +
		String( ":\n    open/close the input method.\n\n") ;
	scim_key_list_to_string( str , key_list_factory_menu) ;
	desc += String( "  ") + str +
		String( ":\n    show the factory menu.\n\n") ;
	scim_key_list_to_string( str , key_list_factory_menu_next) ;
	desc += String( "  ") + str +
		String( ":\n    switch to the next input method.\n\n") ;
	scim_key_list_to_string( str , key_list_factory_menu_prev) ;
	desc += String( "  ") + str +
		String( ":\n    switch to the previous input method.\n\n\n") ;

	desc += utf8_wcstombs( be->get_instance_name( instance)) +
		String( "\n\n");
	desc += String( "  Authors:\n    ") +
		utf8_wcstombs( be->get_instance_authors( instance)) +
		String( "\n");
	desc += String( "  Help:\n    ") +
		utf8_wcstombs( be->get_instance_help( instance)) +
		String( "\n");
	desc += String( "  Credits:\n    ") +
		utf8_wcstombs( be->get_instance_credits( instance)) +
		String( "\n\n");

	transaction_init( instance) ;
	sock.put_command( SCIM_TRANS_CMD_PANEL_SHOW_HELP);
	sock.put_data( desc) ;
	sock.write_to_socket(panel , SCIM_TRANS_MAGIC);
}


//
// callbacks from backend
//

static void
cb_commit(
	int instance ,
	const WideString &  wstr
	)
{
	im_scim_context_private_t *  context = NULL ;

	if( ! ( context = instance_to_context( instance)))
	{
		return ;
	}

	if( ! context->on)
	{
		return ;
	}

	context->preedit_attr.clear() ;

	(*context->cb->commit)( context->self , C_STR( wstr)) ;
	(*context->cb->candidate_hide)( context->self) ;
}

static void
cb_preedit_update(
	int  instance ,
	const  WideString &  wstr ,
	const AttributeList &  attr
	)
{
	im_scim_context_private_t *  context = NULL ;

	if( ! ( context = instance_to_context( instance)))
	{
		return ;
	}

	if( ! context->on)
	{
		return ;
	}

	context->preedit_str = wstr ;
	context->preedit_attr = attr ;
}

static void
cb_preedit_hide(
	int  instance
	)
{
	im_scim_context_private_t *  context = NULL ;

	if( ! ( context = instance_to_context( instance)))
	{
		return ;
	}

	if( ! context->on)
	{
		return ;
	}

	context->preedit_str = WideString() ;
	context->preedit_attr.clear() ;

	(*context->cb->preedit_update)( context->self , NULL , 0) ;
}

static void
cb_preedit_caret(
	int  instance ,
	int  caret
	)
{
	im_scim_context_private_t *  context = NULL ;

	if( ! ( context = instance_to_context( instance)))
	{
		return ;
	}

	if( ! context->on)
	{
		return ;
	}

	context->preedit_caret = caret ;

	(*context->cb->preedit_update)( context->self ,
					C_STR( context->preedit_str) ,
					caret) ;
}

static void
cb_lookup_update(
	int  instance ,
	const  LookupTable &  table
	)
{
	im_scim_context_private_t *  context = NULL ;
	int  num_of_candiate ;
	int  index ;
	char **  str ;
	int  i ;

	if( ! ( context = instance_to_context( instance)))
	{
		return ;
	}

	if( ! context->on)
	{
		return ;
	}

	num_of_candiate = table.get_current_page_size() ;
	index = table.get_cursor_pos_in_current_page() ;

	str = new char*[num_of_candiate] ;

	for( i = 0 ; i < num_of_candiate ; i++)
	{
		str[i] = strdup( C_STR( table.get_candidate_in_current_page( i))) ;
	}

	(*context->cb->candidate_update)( context->self ,
					  is_vertical_lookup ? 1 : 0 ,
					  num_of_candiate ,
					  str , index) ;

	for( i = 0 ; i < num_of_candiate ; i++)
	{
		free( str[i]) ;
	}

	delete [] str ;
}

static void
cb_lookup_show(
	int  instance
	)
{
	im_scim_context_private_t *  context = NULL ;

	if( ! ( context = instance_to_context( instance)))
	{
		return ;
	}

	if( ! context->on)
	{
		return ;
	}

	(*context->cb->candidate_show)( context->self) ;
}

static void
cb_lookup_hide(
	int  instance
	)
{
	im_scim_context_private_t *  context = NULL ;

	if( ! ( context = instance_to_context( instance)))
	{
		return ;
	}

	if( ! context->on)
	{
		return ;
	}

	(*context->cb->candidate_hide)( context->self) ;
}

static void
cb_prop_register(
	int  instance ,
	const PropertyList &  props
	)
{
	if( ! panel.is_connected())
	{
		return ;
	}

	transaction_init( instance) ;
	sock.put_command( SCIM_TRANS_CMD_REGISTER_PROPERTIES) ;
	sock.put_data( props) ;
	sock.write_to_socket( panel , SCIM_TRANS_MAGIC);
}

static void
cb_prop_update(
	int  instance ,
	const Property &  prop
	)
{
	if( ! panel.is_connected())
	{
		return ;
	}

	transaction_init( instance) ;
	sock.put_command( SCIM_TRANS_CMD_UPDATE_PROPERTY) ;
	sock.put_data( prop) ;
	sock.write_to_socket( panel , SCIM_TRANS_MAGIC);
}


// --- global functions ---

int
im_scim_initialize( char *  locale)
{
	SocketAddress  sock_addr;
	SocketClient  sock_client;
	uint32  key ;
	std::vector<String>  imengines ;
	std::vector<String>  config_modules ;
	String  config_mod_name ;

	lang = scim_get_locale_language( String( locale)) ;

	sock_addr.set_address( scim_get_default_socket_frontend_address());
	if( ! sock_client.connect(sock_addr) &&
	    ! scim_socket_trans_open_connection( key ,
						 String( "ConnectionTester") ,
						 String( "SocketFrontEnd") ,
						 sock_client ,
						 TIMEOUT))
	{
		kik_error_printf( "Unable to connect to the socket frontend.\n") ;
		goto  error ;
	}

	if( scim_get_imengine_module_list( imengines) == 0)
	{
		kik_error_printf( "Could not find any IMEngines.\n") ;
		return  error ;
	}

	if( std::find( imengines.begin() , imengines.end() , "socket")
							== imengines.end())
	{
		kik_error_printf( "Could not find socket module.\n");
		return  error ;
	}

	imengines.clear() ;
	imengines.push_back( "socket") ;

	if( scim_get_config_module_list( config_modules) == 0)
	{
		kik_error_printf( "Could not find any config modules.\n") ;
		return  error ;
	}

	config_mod_name = scim_global_config_read(
				SCIM_GLOBAL_CONFIG_DEFAULT_CONFIG_MODULE ,
				String( "simple")); //String( "socket"));

	if( std::find( config_modules.begin() ,
		       config_modules.end() ,
		       config_mod_name) == config_modules.end())
	{
		// fallback
		config_mod_name = config_modules[0] ;
	}

	if( ! (config_module = new ConfigModule( config_mod_name)))
	{
		kik_error_printf( "ConfigModule failed. (%s)\n" ,
				  config_mod_name.c_str());
		return  error ;
	}

	config = config_module->create_config( "scim") ;

	if( config.null())
	{
		// TODO fallback DummyConfig
		kik_error_printf( "create_config failed.\n") ;
		return  error ;
	}

	be = new CommonBackEnd( config , imengines) ;
	if( be.null())
	{
		kik_error_printf( "CommonBackEnd failed.\n") ;
		return  error ;
	}

	if( be->number_of_factories() == 0)
	{
		kik_error_printf( "No factory\n");
		return  error ;
	}

	be->get_factory_list( factories) ;

	sock_addr.set_address( scim_get_default_panel_socket_address()) ;
	if( panel.connect( sock_addr))
	{
		if( ! scim_socket_trans_open_connection( panel_key ,
							 String( "FrontEnd") ,
							 String( "Panel") ,
							 panel ,
							 TIMEOUT))
		{
			panel.close() ;
		}
	}

	load_config( config) ;
	config->signal_connect_reload( slot( load_config)) ;

	be->signal_connect_commit_string( slot( cb_commit)) ;
	be->signal_connect_update_preedit_string( slot( cb_preedit_update)) ;
	be->signal_connect_hide_preedit_string( slot( cb_preedit_hide)) ;
	be->signal_connect_update_preedit_caret( slot( cb_preedit_caret)) ;
	be->signal_connect_update_lookup_table( slot( cb_lookup_update)) ;
	be->signal_connect_show_lookup_table( slot( cb_lookup_show)) ;
	be->signal_connect_hide_lookup_table( slot( cb_lookup_hide)) ;
	be->signal_connect_register_properties( slot( cb_prop_register)) ;
	be->signal_connect_update_property( slot( cb_prop_update)) ;

	context_table.clear() ;

	return  1 ;

error:

	if( ! config.null())
	{
		config.reset() ;
	}

	if( ! be.null())
	{
		be.reset() ;
	}

	if( panel.is_connected())
	{
		panel.close() ;
	}

	return  0 ;
}

int
im_scim_finalize( void)
{
	if( panel.is_connected())
	{
		panel.close() ;
	}

	if( ! be.null())
	{
		be->delete_all_instances() ;
		be.reset() ;
	}

	if( ! config.null())
	{
		config.reset() ;
	}

	if( config_module)
	{
		delete  config_module ;
		config_module = NULL ;
	}

	return  1 ;
}

im_scim_context_t
im_scim_create_context(
	void *  self ,
	im_scim_callbacks_t *  callbacks)
{
	im_scim_context_private_t *  context = NULL ;
	String  factory ;
	size_t  i ;

	context = new im_scim_context_private_t ;

	context->instance = 0xffffffffU ;

	for( i = 0 , factory = factories[0] ; i < factories.size() ; i++)
	{
		if( be->get_factory_language( factories[i]) == lang)
		{
			factory = factories[i] ;
		}
	}

	factory = scim_global_config_read(
			String( SCIM_GLOBAL_CONFIG_DEFAULT_IMENGINE_FACTORY) +
				String( "/") + lang ,
			factory) ;

	if( std::find( factories.begin() , factories.end() , factory)
							== factories.end())
	{
		factory = factories[0] ;
	}

	if( ( context->instance = be->new_instance( factory , "UTF-8")) < 0)
	{
		kik_error_printf( "Could not create new instance.\n") ;
		return  NULL ;
	}
	if( context->instance >= 0xffffffff)	// FIXME
	{
		kik_error_printf( "An instance %d is too big.\n", context->instance) ;
		return  NULL ;
	}

	context_table.push_back( context) ;

	context->on = 0 ;
	context->focused = 0 ;
	context->self = self ;
	context->cb = callbacks ;

	return  (im_scim_context_t) context ;
}

int
im_scim_destroy_context(
	im_scim_context_t  _context
	)
{
	im_scim_context_private_t *  context ;

	context = (im_scim_context_private_t *) _context ;

	be->delete_instance( context->instance) ;

	context_table.erase( std::find( context_table.begin() ,
					context_table.end() ,
					context)) ;

	delete  context ;

	return  1 ;
}

int
im_scim_focused(
	im_scim_context_t  _context
	)
{
	im_scim_context_private_t *  context ;

	context = (im_scim_context_private_t *) _context ;

	if( panel.is_connected())
	{
		transaction_init( context->instance) ;
		sock.put_command( SCIM_TRANS_CMD_FOCUS_IN) ;
		if( context->on)
		{
			sock.put_command( SCIM_TRANS_CMD_PANEL_UPDATE_FACTORY_INFO);
			sock.put_data( utf8_wcstombs( be->get_instance_name( context->instance))) ;
			sock.put_data( be->get_instance_icon_file( context->instance));
			sock.put_command( SCIM_TRANS_CMD_PANEL_TURN_ON) ;
		}
		else
		{
			sock.put_command( SCIM_TRANS_CMD_PANEL_TURN_OFF) ;
		}
		sock.write_to_socket(panel , SCIM_TRANS_MAGIC);
	}

	be->focus_in( context->instance) ;

	(*context->cb->candidate_show)( context->self) ;

	context->focused = 1 ;

	return  1 ;
}

int
im_scim_unfocused(
	im_scim_context_t  _context
	)
{
	im_scim_context_private_t *  context ;

	context = (im_scim_context_private_t *) _context ;

	if( panel.is_connected())
	{
		transaction_init( context->instance) ;
		sock.put_command( SCIM_TRANS_CMD_FOCUS_IN) ;
		sock.put_command( SCIM_TRANS_CMD_PANEL_TURN_OFF);
		sock.put_command( SCIM_TRANS_CMD_FOCUS_OUT) ;
		sock.write_to_socket(panel , SCIM_TRANS_MAGIC);
	}

	be->focus_out( context->instance) ;

	(*context->cb->candidate_hide)( context->self) ;

	context->focused = 0 ;

	return  1 ;
}

int
im_scim_key_event(
	im_scim_context_t  _context ,
	KeySym  ksym ,
	XKeyEvent *  event
	)
{
	im_scim_context_private_t *  context ;
	KeyEvent  scim_key ;

	context = (im_scim_context_private_t *) _context ;

	switch( kind_of_key( (int)ksym , (int)event->state))
	{
	case KEY_TRIGER:
		transaction_init( context->instance) ;
		sock.put_command( SCIM_TRANS_CMD_FOCUS_IN) ;
		if( context->on)
		{
			if( panel.is_connected())
			{
				sock.put_command( SCIM_TRANS_CMD_PANEL_TURN_OFF) ;
				sock.put_command( SCIM_TRANS_CMD_FOCUS_OUT) ;
				sock.write_to_socket(panel , SCIM_TRANS_MAGIC);
			}

			(*context->cb->preedit_update)( context->self ,
							NULL , 0) ;
			(*context->cb->candidate_hide)( context->self) ;

			be->focus_out( context->instance) ;

			context->on = 0 ;
		}
		else
		{
			if( panel.is_connected())
			{
				sock.put_command( SCIM_TRANS_CMD_PANEL_UPDATE_FACTORY_INFO);
				sock.put_data( utf8_wcstombs( be->get_instance_name( context->instance))) ;
				sock.put_data( be->get_instance_icon_file( context->instance));
				sock.put_command( SCIM_TRANS_CMD_PANEL_TURN_ON) ;
				sock.put_command( SCIM_TRANS_CMD_FOCUS_IN) ;
				sock.write_to_socket(panel , SCIM_TRANS_MAGIC);
			}

			(*context->cb->preedit_update)(
						context->self ,
						C_STR( context->preedit_str) ,
						context->preedit_caret) ;
			(*context->cb->candidate_show)( context->self) ;

			be->focus_in( context->instance) ;

			context->on = 1 ;
		}
		return  0 ;
	case KEY_FACTORY_MENU:
	case KEY_FACTORY_NEXT:
	case KEY_FACTORY_PREV:
		// not implemented yet
		return  0 ;
	case KEY_OTHER:
	default:
		break ;
	}

	if( ! context->on)
	{
		return  1 ;
	}

	scim_key.mask = event->state & ~SCIM_KEY_ReleaseMask;
	scim_key.code = ksym ;

	if( be->process_key_event( context->instance , scim_key))
	{
		sock.write_to_socket( panel , SCIM_TRANS_MAGIC);
		return  0 ;
	}

	return  1 ;
}

unsigned int
im_scim_preedit_char_attr(
	im_scim_context_t  _context ,
	unsigned int  index)
{
	im_scim_context_private_t *  context ;
	AttributeList::const_iterator  attr;
	unsigned int  result = CHAR_ATTR_UNDERLINE ;

	context = (im_scim_context_private_t *) _context ;

	for( attr = context->preedit_attr.begin() ;
	     attr != context->preedit_attr.end() ;
	     attr ++)
	{
		unsigned int  start ;
		unsigned int  end ;

		start = attr->get_start() ;
		end = attr->get_end() ;

	#if 0 // XXX
		if( index < start || end < index)
	#else
		if( index < start || end <= index)
	#endif
		{
			continue ;
		}

		if( attr->get_type() != SCIM_ATTR_DECORATE)
		{
			// SCIM_ATTR_FOREGROUND and SCIM_ATTR_BACKGROUND
			continue ;
		}

		switch( attr->get_value())
		{
		#if 0
		case  SCIM_ATTR_DECORATE_UNDERLINE:
			result |= CHAR_ATTR_UNDERLINE ;
			break ;
		#endif
		case  SCIM_ATTR_DECORATE_REVERSE:
			result &= ~CHAR_ATTR_UNDERLINE ;
			result |= CHAR_ATTR_REVERSE ;
			break ;
		case  SCIM_ATTR_DECORATE_HIGHLIGHT:
			result |= CHAR_ATTR_BOLD ;
			break ;
		default:
			break ;
		}
	}

	return  result ;
}

int
im_scim_get_panel_fd( void)
{
	if( panel.is_connected())
	{
		return  panel.get_id() ;
	}

	return  -1 ;
}

int
im_scim_receive_panel_event( void)
{
	int  command ;
	SocketTransactionDataType  type ;
	String  string ;
	uint32  data_uint32 ;
	int  instance ;

	if( ! sock.read_from_socket( panel , TIMEOUT))
	{
		// XXX
		return  0 ;
	}

	sock.get_command( command) ;
	if( command != SCIM_TRANS_CMD_REPLY)
	{
		return  1 ;
	}

	type = sock.get_data_type() ;
	if( type == SCIM_TRANS_DATA_COMMAND)
	{
		while( sock.get_command(command))
		{
			if( command == SCIM_TRANS_CMD_RELOAD_CONFIG)
			{
				config->reload() ;
				load_config( config) ;
			}
			else if( command == SCIM_TRANS_CMD_EXIT)
			{
				// not implemented yet
			}
		}

		return  1 ;
	}

	sock.get_data( data_uint32) ;
	instance = (int) data_uint32 ;

	sock.get_command( command) ;
	switch( command)
	{
	case SCIM_TRANS_CMD_PANEL_REQUEST_FACTORY_MENU:
		send_factory_menu_item( instance) ;
		break ;
	case SCIM_TRANS_CMD_PANEL_CHANGE_FACTORY:
		sock.get_data( string) ;
		be->replace_instance( instance , string) ;
		transaction_init( instance) ;
		sock.put_command( SCIM_TRANS_CMD_FOCUS_IN) ;
		sock.put_command( SCIM_TRANS_CMD_PANEL_UPDATE_FACTORY_INFO);
		sock.put_data( utf8_wcstombs( be->get_instance_name( instance))) ;
		sock.put_data( be->get_instance_icon_file( instance));
		//sock.put_command( SCIM_TRANS_CMD_PANEL_TURN_ON) ;
		sock.write_to_socket(panel , SCIM_TRANS_MAGIC);
		be->focus_in( instance) ;
		scim_global_config_write(
			String( SCIM_GLOBAL_CONFIG_DEFAULT_IMENGINE_FACTORY) +
				String( "/") + lang ,
			string) ;
		break ;
	case SCIM_TRANS_CMD_PANEL_REQUEST_HELP:
		send_help_description( instance) ;
		break ;
	case SCIM_TRANS_CMD_TRIGGER_PROPERTY:
		sock.get_data( string) ;
		be->trigger_property( instance , string) ;
		break;
	default:
		break ;
	}

	return  1 ;
}

