//
// im_scim_1.2.cpp - SCIM plugin for mlterm (c++ part)
//
// Copyright (C) 2005 Seiichi SATO <ssato@sh.rim.or.jp>
//
//	$Id$
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
#define  Uses_SCIM_TRANSACTION
#define  Uses_SCIM_HOTKEY
#include  <scim.h>

#define  SCIM_TRANS_MAGIC 0x4d494353

#define  TIMEOUT  5000	// msec

const int  KEY_TRIGGER = 1 ;
const int  KEY_MENU = 2 ;
const int  KEY_NEXT_FACTORY = 3 ;
const int  KEY_PRE_FACTORY = 4 ;

using namespace scim ;

typedef struct  im_scim_context_private
{
	IMEngineFactoryPointer  factory ;
	IMEngineInstancePointer  instance ;
	int  id ;

	int  on ;
	int  focused ;

	WideString  preedit_str ;
	AttributeList  preedit_attr ;
	int  preedit_caret ;

	void *  self ;
	im_scim_callbacks_t *  cb ;

}  im_scim_context_private_t ;

#define transaction_init(id)				\
do {							\
	int  cmd ;					\
	uint32  data ;					\
	sock.clear() ;					\
	sock.put_command( SCIM_TRANS_CMD_REQUEST) ;	\
	sock.put_data( panel_key) ;			\
	sock.put_data( (uint32)(id)) ;			\
	sock.get_command( cmd) ;			\
	sock.get_data( data) ;				\
	sock.get_data( data) ;				\
}  while(0)

#define C_STR(s) (char*) ( utf8_wcstombs((s)).c_str())

// --- static variables ---

static std::vector <im_scim_context_private_t*>  context_table ;

static String  lang ;

static ConfigModule *  config_module = NULL ;
static ConfigPointer  config = NULL ;

static BackEndPointer  be = NULL ;

static HotkeyMatcher  keymatcher_frontend ;
static IMEngineHotkeyMatcher  keymatcher_imengine ;

static SocketClient  panel ;
static uint32  panel_key ;

static Transaction  sock ;

static bool  is_vertical_lookup ;

static int  id = 0 ; /* XXX */

// --- static functions ---

static void
load_config( const ConfigPointer &  config)
{
	static KeyEventList  key ;

	scim_string_to_key_list( key , config->read( String( SCIM_CONFIG_HOTKEYS_FRONTEND_TRIGGER) , String( "Control+space"))) ;
	keymatcher_frontend.add_hotkeys( key , KEY_TRIGGER) ;

	scim_string_to_key_list( key , config->read( String( SCIM_CONFIG_HOTKEYS_FRONTEND_SHOW_FACTORY_MENU) , String( "Control+Alt+l,Control+Alt+m,Control+Alt+s,Control+Alt+Right"))) ;
	keymatcher_frontend.add_hotkeys( key , KEY_MENU) ;

	scim_string_to_key_list( key , config->read( String( SCIM_CONFIG_HOTKEYS_FRONTEND_NEXT_FACTORY) , String( "Control+Alt+Down,Control+Shift_R,Control+Shift_L"))) ;
	keymatcher_frontend.add_hotkeys( key , KEY_NEXT_FACTORY) ;

	scim_string_to_key_list( key , config->read( String( SCIM_CONFIG_HOTKEYS_FRONTEND_PREVIOUS_FACTORY) , String( "Control+Alt+Up,Shift+Control_R,Shift+Control_L"))) ;
	keymatcher_frontend.add_hotkeys( key , KEY_PRE_FACTORY) ;

	// hack
	is_vertical_lookup = config->read( String( "/Panel/Gtk/LookupTableVertical") , false) ;
}

static im_scim_context_private_t *
id_to_context(
	int  id
	)
{
	im_scim_context_private_t *  context = NULL ;
	size_t i ;

	for( i = 0 ; i < context_table.size() ; i++)
	{
		if( context_table[i]->id == id)
		{
			return  context_table[i] ;
		}
	}

	return  NULL ;
}


static void
send_factory_menu_item(
	im_scim_context_private_t *  context
	)
{
	std::vector<IMEngineFactoryPointer>  factories ;
	size_t  i ;

	be->get_factories_for_encoding( factories , "UTF-8") ;

	transaction_init( context->id) ;
	sock.put_command( SCIM_TRANS_CMD_PANEL_SHOW_FACTORY_MENU) ;
	for( i = 0 ; i < factories.size() ; i++)
	{
		sock.put_data( factories[i]->get_uuid()) ;
		sock.put_data( utf8_wcstombs( factories[i]->get_name()));
		sock.put_data( factories[i]->get_language());
		sock.put_data( factories[i]->get_icon_file());
	}
	sock.write_to_socket(panel , SCIM_TRANS_MAGIC);
}

static void
send_help_description(
	im_scim_context_private_t *  context
	)
{
	String  desc ;
	String  str ;

	desc += utf8_wcstombs( context->factory->get_name()) +
		String( ":\n\n");
	desc += utf8_wcstombs( context->factory->get_authors()) +
		String( "\n\n");
	desc += String( "  Help:\n    ") +
		utf8_wcstombs( context->factory->get_help()) +
		String( "\n\n");
	desc += utf8_wcstombs( context->factory->get_credits()) +
		String( "\n\n");

	transaction_init( context->id) ;
	sock.put_command( SCIM_TRANS_CMD_PANEL_SHOW_HELP);
	sock.put_data( desc) ;
	sock.write_to_socket(panel , SCIM_TRANS_MAGIC);
}


//
// callbacks from backend
//

static void
cb_commit(
	IMEngineInstanceBase *  instance ,
	const WideString &  wstr
	)
{
	im_scim_context_private_t *  context ;

	context = static_cast <im_scim_context_private_t *> (instance->get_frontend_data()) ;

	if( ! context)
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
	IMEngineInstanceBase *  instance ,
	const  WideString &  wstr ,
	const AttributeList &  attr
	)
{
	im_scim_context_private_t *  context ;

	context = static_cast <im_scim_context_private_t *> (instance->get_frontend_data()) ;

	if( ! context)
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
	IMEngineInstanceBase *  instance
	)
{
	im_scim_context_private_t *  context ;

	context = static_cast <im_scim_context_private_t *> (instance->get_frontend_data()) ;

	if( ! context)
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
	IMEngineInstanceBase *  instance ,
	int  caret
	)
{
	im_scim_context_private_t *  context ;

	context = static_cast <im_scim_context_private_t *> (instance->get_frontend_data()) ;

	if( ! context)
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
	IMEngineInstanceBase *  instance ,
	const  LookupTable &  table
	)
{
	im_scim_context_private_t *  context ;
	u_int  num_of_candiate ;
	int  index ;
	char **  str ;
	int  i ;

	context = static_cast <im_scim_context_private_t *> (instance->get_frontend_data()) ;

	if( ! context)
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
	IMEngineInstanceBase *  instance
	)
{
	im_scim_context_private_t *  context ;

	context = static_cast <im_scim_context_private_t *> (instance->get_frontend_data()) ;

	if( ! context)
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
	IMEngineInstanceBase *  instance
	)
{
	im_scim_context_private_t *  context ;

	context = static_cast <im_scim_context_private_t *> (instance->get_frontend_data()) ;

	if( ! context)
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
	IMEngineInstanceBase *  instance ,
	const PropertyList &  props
	)
{
	im_scim_context_private_t *  context ;

	context = static_cast <im_scim_context_private_t *> (instance->get_frontend_data()) ;

	if( ! context)
	{
		return ;
	}

	if( ! panel.is_connected())
	{
		return ;
	}

	transaction_init( context->id) ;
	sock.put_command( SCIM_TRANS_CMD_REGISTER_PROPERTIES) ;
	sock.put_data( props) ;
	sock.write_to_socket( panel , SCIM_TRANS_MAGIC);
}

static void
cb_prop_update(
	IMEngineInstanceBase *  instance ,
	const Property &  prop
	)
{
	im_scim_context_private_t *  context ;

	context = static_cast <im_scim_context_private_t *> (instance->get_frontend_data()) ;

	if( ! context)
	{
		return ;
	}

	if( ! panel.is_connected())
	{
		return ;
	}

	transaction_init( context->id) ;
	sock.put_command( SCIM_TRANS_CMD_UPDATE_PROPERTY) ;
	sock.put_data( prop) ;
	sock.write_to_socket( panel , SCIM_TRANS_MAGIC);
}

static void
set_callbacks(
	im_scim_context_private_t *  context
	)
{
	context->instance->signal_connect_commit_string( slot( cb_commit)) ;
	context->instance->signal_connect_update_preedit_string( slot( cb_preedit_update)) ;
	context->instance->signal_connect_hide_preedit_string( slot( cb_preedit_hide)) ;
	context->instance->signal_connect_update_preedit_caret( slot( cb_preedit_caret)) ;
	context->instance->signal_connect_update_lookup_table( slot( cb_lookup_update)) ;
	context->instance->signal_connect_show_lookup_table( slot( cb_lookup_show)) ;
	context->instance->signal_connect_hide_lookup_table( slot( cb_lookup_hide)) ;
	context->instance->signal_connect_register_properties( slot( cb_prop_register)) ;
	context->instance->signal_connect_update_property( slot( cb_prop_update)) ;

	context->instance->set_frontend_data( static_cast <void*> (context)) ;
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
	    ! scim_socket_open_connection( key ,
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
		// fallback
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
		// TODO fallback DummyConfig
		kik_error_printf( "create_config failed.\n") ;
		return  0 ;
	}

	be = new CommonBackEnd( config , imengines) ;
	if( be.null())
	{
		kik_error_printf( "CommonBackEnd failed.\n") ;
		return  0 ;
	}

	sock_addr.set_address( scim_get_default_panel_socket_address( getenv( "DISPLAY"))) ; // FIXME
	if( panel.connect( sock_addr))
	{
		if( ! scim_socket_open_connection( panel_key ,
						   String( "FrontEnd") ,
						   String( "Panel") ,
						   panel ,
						   TIMEOUT))
		{
			panel.close() ;
		}
	}

	load_config( config) ;
	config->signal_connect_reload( slot(load_config)) ;

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
	size_t  i ;

	context = new im_scim_context_private_t ;

	context->factory = be->get_default_factory( lang , String( "UTF-8")) ;

	if( ! ( context->instance = context->factory->create_instance( String( "UTF-8") ,
							      id)))
	{
		kik_error_printf( "Could not create new instance.\n") ;
		return  NULL ;
	}

	context_table.push_back( context) ;

	context->id = id ;
	context->on = 0 ;
	context->focused = 0 ;
	context->self = self ;
	context->cb = callbacks ;

	set_callbacks( context) ;

	id ++ ;

	return  (im_scim_context_t) context ;
}

int
im_scim_destroy_context(
	im_scim_context_t  _context
	)
{
	im_scim_context_private_t *  context ;

	context = (im_scim_context_private_t *) _context ;

	context->instance.reset() ;

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
		transaction_init( context->id) ;
		sock.put_command( SCIM_TRANS_CMD_FOCUS_IN) ;
		sock.put_data( context->instance->get_factory_uuid()) ;
		if( context->on)
		{
			sock.put_command( SCIM_TRANS_CMD_PANEL_UPDATE_FACTORY_INFO);
			sock.put_data( context->factory->get_uuid()) ;
			sock.put_data( utf8_wcstombs( context->factory->get_name()));
			sock.put_data( context->factory->get_language()) ;
			sock.put_data( context->factory->get_icon_file()) ;
			sock.put_command( SCIM_TRANS_CMD_PANEL_TURN_ON) ;
		}
		else
		{
			sock.put_command( SCIM_TRANS_CMD_PANEL_TURN_OFF) ;
		}
		sock.write_to_socket(panel , SCIM_TRANS_MAGIC);
	}

	context->instance->focus_in() ;

	(*context->cb->candidate_show)( context->self) ;

	context->focused = 1 ;

	return 1 ;
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
		transaction_init( context->id) ;
		sock.put_command( SCIM_TRANS_CMD_FOCUS_IN) ;
		sock.put_command( SCIM_TRANS_CMD_PANEL_TURN_OFF);
		sock.put_command( SCIM_TRANS_CMD_FOCUS_OUT) ;
		sock.write_to_socket(panel , SCIM_TRANS_MAGIC);
	}

	context->instance->focus_out() ;

	(*context->cb->candidate_hide)( context->self) ;

	context->focused = 0 ;

	return 1 ;
}

int
im_scim_key_event(
	im_scim_context_t  _context ,
	KeySym  ksym ,
	XKeyEvent *  event
	)
{
	im_scim_context_private_t *  context ;
	int  key_type ;
	KeyEvent  scim_key ;

	context = (im_scim_context_private_t *) _context ;

	scim_key.mask = event->state & ~SCIM_KEY_ReleaseMask;
	scim_key.code = ksym ;

	keymatcher_frontend.push_key_event( scim_key) ;
	keymatcher_imengine.push_key_event( scim_key) ;

	key_type = keymatcher_frontend.get_match_result() ;

	switch( key_type)
	{
	case KEY_TRIGGER:
		transaction_init( context->id) ;
		sock.put_command( SCIM_TRANS_CMD_FOCUS_IN) ;
		sock.put_data( context->instance->get_factory_uuid()) ;
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

			context->instance->focus_out() ;

			context->on = 0 ;
		}
		else
		{
			if( panel.is_connected())
			{
				sock.put_command( SCIM_TRANS_CMD_PANEL_UPDATE_FACTORY_INFO);
				sock.put_data( context->factory->get_uuid()) ;
				sock.put_data( utf8_wcstombs( context->factory->get_name())) ;
				sock.put_data( context->factory->get_language()) ;
				sock.put_data( context->factory->get_icon_file()) ;
				sock.put_command( SCIM_TRANS_CMD_PANEL_TURN_ON) ;
				sock.put_command( SCIM_TRANS_CMD_FOCUS_IN) ;
				sock.write_to_socket(panel , SCIM_TRANS_MAGIC);
			}

			(*context->cb->preedit_update)(
						context->self ,
						C_STR( context->preedit_str) ,
						context->preedit_caret) ;
			(*context->cb->candidate_show)( context->self) ;

			context->instance->focus_in() ;

			context->on = 1 ;
		}
		return  0 ;
	case KEY_MENU:
	case KEY_NEXT_FACTORY:
	case KEY_PRE_FACTORY:
		// not implemented yet
		return  0 ;
	default:
		break ;
	}

	if( ! context->on)
	{
		return  1 ;
	}

	if( context->instance->process_key_event( scim_key))
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
	im_scim_context_private_t *  context ;
	int  command ;
	TransactionDataType  type ;
	String  string ;
	uint32  data_uint32 ;
	int  id ;

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
	id = (int) data_uint32 ;

	context = id_to_context( id) ;

	sock.get_command( command) ;
	switch( command)
	{
	case SCIM_TRANS_CMD_PANEL_REQUEST_FACTORY_MENU:
		send_factory_menu_item( context) ;
		break ;
	case SCIM_TRANS_CMD_PANEL_CHANGE_FACTORY:
		sock.get_data( string) ;
		transaction_init( context->id) ;
		sock.put_command( SCIM_TRANS_CMD_FOCUS_IN) ;
		sock.put_data( context->instance->get_factory_uuid()) ;
		if( string.length() == 0)
		{
			sock.put_command( SCIM_TRANS_CMD_PANEL_TURN_OFF) ;
			sock.put_command( SCIM_TRANS_CMD_FOCUS_OUT) ;
			sock.write_to_socket(panel , SCIM_TRANS_MAGIC);
			context->on = 0 ;
			return  1 ;
		}
		context->factory = be->get_factory( string) ;
		context->instance->focus_out() ;
		be->set_default_factory( lang , context->factory->get_uuid()) ;
		context->instance = context->factory->create_instance( String( "UTF-8") , context->id) ;
		set_callbacks( context) ;
		sock.put_command( SCIM_TRANS_CMD_PANEL_UPDATE_FACTORY_INFO);
		sock.put_data( context->factory->get_uuid());
		sock.put_data( utf8_wcstombs(context->factory->get_name()));
		sock.put_data( context->factory->get_language()) ;
		sock.put_data( context->factory->get_icon_file()) ;
		sock.write_to_socket(panel , SCIM_TRANS_MAGIC);
		context->instance->focus_in() ;

		break ;
	case SCIM_TRANS_CMD_PANEL_REQUEST_HELP:
		send_help_description( context) ;
		break ;
	case SCIM_TRANS_CMD_TRIGGER_PROPERTY:
		sock.get_data( string) ;
		context->instance->trigger_property( string) ;
		break;
	default:
		break ;
	}

	return  1 ;
}

