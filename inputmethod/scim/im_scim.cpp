//
// im_scim.cpp - SCIM plugin for mlterm (c++ part)
//
// Copyright (C) 2005 Seiichi SATO <ssato@sh.rim.or.jp>
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. The name of any author may not be used to endorse or promote
//    products derived from this software without their specific prior
//    written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//
//
//	$Id$
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
	int  instance ;

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
kind_of_key( KeySym  ksym , unsigned int mask)
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
	int  instance
	)
{
	im_scim_context_private_t *  context = NULL ;
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

static bool
find_factory(
	String &  factory ,
	char *  factory_name ,
	char *  locale
	)
{
	String  lang ;
	size_t  i ;

	if( factory_name)
	{
		for( i = 0 ; i < factories.size() ; i++)
		{
			if( String( factory_name) ==
					utf8_wcstombs( be->get_factory_name( factories[i])))
			{
				factory = factories[i] ;
				return  true ;
			}
		}

		kik_error_printf( "Could not found %s\n" , factory_name) ;
		return  false ;
	}

	lang = scim_get_locale_language( String( locale)) ;

	for( i = 0 , factory = factories[0] ; i < factories.size() ; i++)
	{
		if( be->get_factory_language( factories[i]) == lang)
		{
			factory = factories[i] ;
			break ;
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

	return  true ;
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
	transaction_init( instance) ;
	sock.put_command( SCIM_TRANS_CMD_PANEL_SHOW_HELP);
	sock.put_data( String( "not implemented yet.\n")) ;
	sock.write_to_socket(panel , SCIM_TRANS_MAGIC);
}

static void
change_factory(
	int  instance ,
	const String &  factory
	)
{
	im_scim_context_private_t *  context ;

	if( ! ( context = instance_to_context( instance)))
	{
		return ;
	}

	if( ! context->on)
	{
		return ;
	}

	(context->cb->im_changed)( context->self,
				   C_STR( be->get_factory_name( factory))) ;
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
	u_int  num_of_candiate ;
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
im_scim_initialize(void)
{
	SocketAddress  sock_addr;
	SocketClient  sock_client;
	uint32  key ;
	std::vector<String>  imengines ;
	std::vector<String>  config_modules ;
	String  config_mod_name ;

	sock_addr.set_address( scim_get_default_socket_frontend_address());
	if( ! sock_client.connect(sock_addr) &&
	    ! scim_socket_trans_open_connection( key ,
						 String( "ConnectionTester") ,
						 String( "SocketFrontEnd") ,
						 sock_client ,
						 TIMEOUT))
	{
		kik_error_printf( "Unable to connect to the socket frontend.\n") ;
		return  0 ;
	}

	if( scim_get_imengine_module_list( imengines) == 0)
	{
		kik_error_printf( "Could not find any IMEngines.\n") ;
		return  0 ;
	}

	if( std::find( imengines.begin() , imengines.end() , "socket")
							== imengines.end())
	{
		kik_error_printf( "Could not find socket module.\n");
		return  0 ;
	}

	imengines.clear() ;
	imengines.push_back( "socket") ;

	if( scim_get_config_module_list( config_modules) == 0)
	{
		kik_error_printf( "Could not find any config modules.\n") ;
		return  0 ;
	}

	config_mod_name = scim_global_config_read(
				SCIM_GLOBAL_CONFIG_DEFAULT_CONFIG_MODULE ,
				String( "simple")); //String( "socket"));

	if( std::find( config_modules.begin() ,
		       config_modules.end() ,
		       config_mod_name) == config_modules.end())
	{
		// failback
		config_mod_name = config_modules[0] ;
	}

	if( ! (config_module = new ConfigModule( config_mod_name)))
	{
		kik_error_printf( "ConfigModule failed. (%s)\n" ,
				  config_mod_name.c_str());
		return  0 ;
	}

	config = config_module->create_config( "scim") ;

	if( config.null())
	{
		// TODO failback to DummyConfig
		kik_error_printf( "create_config failed.\n") ;
		return  0 ;
	}

	be = new CommonBackEnd( config , imengines) ;
	if( be.null())
	{
		kik_error_printf( "CommonBackEnd failed.\n") ;
		return  0 ;
	}

	if( be->number_of_factories() == 0)
	{
		kik_error_printf( "No factory\n");
		return  0 ;
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
	char *  factory_name ,
	char *  locale ,
	void *  self ,
	im_scim_callbacks_t *  callbacks)
{
	im_scim_context_private_t *  context = NULL ;
	String  factory ;
	String  lang ;
	int  i ;

	context = new im_scim_context_private_t ;

	context->instance = -1 ;

	if( ! find_factory( factory , factory_name , locale))
	{
		return  NULL ;
	}

	if( ( context->instance = be->new_instance( factory , "UTF-8")) < 0)
	{
		kik_error_printf( "Could not create new instance.\n") ;
		return  NULL ;
	}
	if( context->instance > 0xffffffff)	// FIXME
	{
		kik_error_printf( "An instance %d is too big.\n") ;
		return  NULL ;
	}

	context_table.push_back( context) ;

	context->on = 0 ;
	context->focused = 0 ;
	context->self = self ;
	context->cb = callbacks ;

	// workaround for switching factory via panel
	transaction_init( context->instance) ;
	sock.put_command( SCIM_TRANS_CMD_FOCUS_IN) ;
	sock.put_command( SCIM_TRANS_CMD_PANEL_TURN_OFF) ;
	sock.put_command( SCIM_TRANS_CMD_FOCUS_OUT) ;
	sock.write_to_socket(panel , SCIM_TRANS_MAGIC);
	be->focus_out( context->instance) ;

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

	switch( kind_of_key( ksym , event->state))
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

		if( index < start || end < index)
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
		change_factory( instance , string) ;
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


// --- functions for external tools ---

u_int
im_scim_get_number_of_factory(void)
{
	return  (u_int) factories.size() ;
}

char *
im_scim_get_default_factory_name(
	char *  locale
	)
{
	String  factory ;

	if( find_factory( factory , NULL , locale))
	{
		return  C_STR(be->get_factory_name( factory)) ;
	}

	return "unknown" ;
}

char *
im_scim_get_factory_name(
	int  index
	)
{
	return  C_STR( be->get_factory_name( factories[index])) ;
}

char *
im_scim_get_language(
	int  index
	)
{
	return  (char*) be->get_factory_language( factories[index]).c_str() ;
}

