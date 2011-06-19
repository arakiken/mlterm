//
// im_scim_1.4.cpp - SCIM plugin for mlterm (c++ part)
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
#include  <string.h>

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

#define C_STR(s) (char*) ( utf8_wcstombs((s)).c_str())

// --- static variables ---

static std::vector <im_scim_context_private_t*>  context_table ;

static String  lang ;

static ConfigModule *  config_module = NULL ;
static ConfigPointer  config = NULL ;

static BackEndPointer  be = NULL ;

static FrontEndHotkeyMatcher  keymatcher_frontend ;
static IMEngineHotkeyMatcher  keymatcher_imengine ;

static PanelClient  panel_client ;

static bool  is_vertical_lookup ;
static int  valid_key_mask = 0 ;

static int  id = 0 ; /* XXX */


// --- static functions ---

static im_scim_context_private_t *
id_to_context(
	int  id
	)
{
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


//
// callback for config modules
//

static void
cb_config_load( const ConfigPointer &  config)
{
	KeyEvent  key ;

	keymatcher_frontend.load_hotkeys( config);
	keymatcher_imengine.load_hotkeys( config);

	scim_string_to_key( key, config->read( String( SCIM_CONFIG_HOTKEYS_FRONTEND_VALID_KEY_MASK) , String( "Shift+Control+Alt+Lock")));

	valid_key_mask = key.mask > 0 ? key.mask : 0xffff ;
	valid_key_mask |= SCIM_KEY_ReleaseMask ;

	scim_global_config_flush();

	// hack
	is_vertical_lookup = config->read( String( "/Panel/Gtk/LookupTableVertical") , false) ;
}


//
// callbacks for backend
//

static void
cb_commit(
	IMEngineInstanceBase *  instance ,
	const WideString &  wstr
	)
{
	im_scim_context_private_t *  context ;

	context = static_cast <im_scim_context_private_t *> (instance->get_frontend_data()) ;

	if( context && context->on)
	{
		context->preedit_attr.clear() ;

		(*context->cb->commit)( context->self , C_STR( wstr)) ;
		(*context->cb->candidate_hide)( context->self) ;
	}
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

	if( context && context->on)
	{
		context->preedit_str = wstr ;
		context->preedit_attr = attr ;
	}
}

static void
cb_preedit_hide(
	IMEngineInstanceBase *  instance
	)
{
	im_scim_context_private_t *  context ;

	context = static_cast <im_scim_context_private_t *> (instance->get_frontend_data()) ;

	if( context && context->on)
	{
		context->preedit_str = WideString() ;
		context->preedit_attr.clear() ;

		(*context->cb->preedit_update)( context->self , NULL , 0) ;
	}
}

static void
cb_preedit_caret(
	IMEngineInstanceBase *  instance ,
	int  caret
	)
{
	im_scim_context_private_t *  context ;

	context = static_cast <im_scim_context_private_t *> (instance->get_frontend_data()) ;

	if( context && context->on)
	{
		context->preedit_caret = caret ;

		(*context->cb->preedit_update)( context->self ,
						C_STR( context->preedit_str) ,
						caret) ;
	}
}

static void
cb_lookup_update(
	IMEngineInstanceBase *  instance ,
	const  LookupTable &  table
	)
{
	im_scim_context_private_t *  context ;
	int  num_of_candiate ;
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

	if( context && context->on)
	{
		(*context->cb->candidate_show)( context->self) ;
	}
}

static void
cb_lookup_hide(
	IMEngineInstanceBase *  instance
	)
{
	im_scim_context_private_t *  context ;

	context = static_cast <im_scim_context_private_t *> (instance->get_frontend_data()) ;

	if( context && context->on)
	{
		(*context->cb->candidate_hide)( context->self) ;
	}
}

static void
cb_prop_register(
	IMEngineInstanceBase *  instance ,
	const PropertyList &  props
	)
{
	im_scim_context_private_t *  context ;

	context = static_cast <im_scim_context_private_t *> (instance->get_frontend_data()) ;

	if( context && panel_client.is_connected())
	{
		panel_client.prepare( context->id) ;
		panel_client.register_properties( context->id , props) ;
		panel_client.send() ;
	}
}

static void
cb_prop_update(
	IMEngineInstanceBase *  instance ,
	const Property &  prop
	)
{
	im_scim_context_private_t *  context ;

	context = static_cast <im_scim_context_private_t *> (instance->get_frontend_data()) ;

	if( context && panel_client.is_connected())
	{
		panel_client.prepare( context->id) ;
		panel_client.update_property( context->id , prop) ;
		panel_client.send() ;
	}
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


//
// callbacks for panel clients
//

static void
cb_panel_request_factory_menu(
	int  id
	)
{
	im_scim_context_private_t *  context ;
	std::vector<IMEngineFactoryPointer>  factories ;
	std::vector<PanelFactoryInfo>  menu;
	size_t  i ;

	context = id_to_context( id) ;

	be->get_factories_for_encoding( factories , "UTF-8") ;
	for( i = 0 ; i < factories.size() ; i++)
	{
		menu.push_back(PanelFactoryInfo(
				factories[i]->get_uuid() ,
				utf8_wcstombs( factories[i]->get_name()) ,
				factories[i]->get_language() ,
				factories[i]->get_icon_file()));
	}

	panel_client.prepare( id) ;
	panel_client.show_factory_menu( id , menu) ;
	panel_client.send() ;
}

static void
cb_panel_request_help(
	int  id
	)
{
	im_scim_context_private_t *  context ;
	String  desc ;
	String  str ;

	context = id_to_context( id) ;

	desc += utf8_wcstombs( context->factory->get_name()) +
		String( ":\n\n");
	desc += utf8_wcstombs( context->factory->get_authors()) +
		String( "\n\n");
	desc += String( "  Help:\n    ") +
		utf8_wcstombs( context->factory->get_help()) +
		String( "\n\n");
	desc += utf8_wcstombs( context->factory->get_credits()) +
		String( "\n\n");

	panel_client.prepare( id) ;
	panel_client.show_help( id , desc) ;
	panel_client.send() ;
}

static void
cb_panel_change_factory(
	int  id ,
	const String &  uuid
	)
{
	im_scim_context_private_t *  context ;
	IMEngineFactoryPointer  factory ;
	PanelFactoryInfo  info ;

	context = id_to_context( id) ;

	factory = be->get_factory( uuid) ;
	if( factory.null())
	{
		return ;
	}

	panel_client.prepare( id) ;

	if( uuid.length() == 0)
	{
		panel_client.turn_off( id) ;
		panel_client.focus_out( id) ;
		panel_client.send() ;
		context->on = 0 ;
		return ;
	}

	context->factory = factory ;
	context->instance->focus_out() ;
	be->set_default_factory( lang , context->factory->get_uuid()) ;
	context->instance = context->factory->create_instance( String( "UTF-8") , context->id) ;
	set_callbacks( context) ;
	info = PanelFactoryInfo(context->factory->get_uuid() ,
				utf8_wcstombs( context->factory->get_name()) ,
				context->factory->get_language() ,
				context->factory->get_icon_file()) ;
	panel_client.update_factory_info( id, info) ;
	panel_client.send() ;
	context->instance->focus_in() ;
}

static void
cb_panel_trigger_property(
	int  id ,
	const String &  prop
	)
{
	im_scim_context_private_t *  context ;

	context = id_to_context( id) ;

	panel_client.prepare( id) ;
	context->instance->trigger_property( prop);
	panel_client.send() ;
}

static int
hotkey(
	im_scim_context_t  _context ,
	const KeyEvent & scim_key
	)
{
	im_scim_context_private_t *  context ;
	FrontEndHotkeyAction  hotkey_action ;
	PanelFactoryInfo  info ;

	context = (im_scim_context_private_t *) _context ;

	keymatcher_frontend.push_key_event( scim_key) ;
	keymatcher_imengine.push_key_event( scim_key) ;

	hotkey_action = keymatcher_frontend.get_match_result() ;

	if( hotkey_action == SCIM_FRONTEND_HOTKEY_OFF && ! context->on)
	{
		return  0 ;
	}

	if( hotkey_action == SCIM_FRONTEND_HOTKEY_ON && context->on)
	{
		return  0 ;
	}

	if( hotkey_action == SCIM_FRONTEND_HOTKEY_TRIGGER)
	{
		if( context->on)
		{
			hotkey_action = SCIM_FRONTEND_HOTKEY_OFF ;
		}
		else
		{
			hotkey_action = SCIM_FRONTEND_HOTKEY_ON ;
		}
	}

	info = PanelFactoryInfo( context->factory->get_uuid() ,
				 utf8_wcstombs( context->factory->get_name()) ,
				 context->factory->get_language() ,
				 context->factory->get_icon_file()) ;

	switch( hotkey_action)
	{
	case SCIM_FRONTEND_HOTKEY_ON:
		if( panel_client.is_connected())
		{
			panel_client.prepare( context->id) ;
			panel_client.update_factory_info( context->id , info) ;
			panel_client.turn_on( context->id) ;
			panel_client.focus_in( context->id , context->instance->get_factory_uuid ()) ;
			panel_client.send() ;
		}

		(*context->cb->preedit_update)( context->self ,
						C_STR( context->preedit_str) ,
						context->preedit_caret) ;
		(*context->cb->candidate_show)( context->self) ;

		context->instance->focus_in() ;

		context->on = 1 ;

		return  0 ;
	case SCIM_FRONTEND_HOTKEY_OFF:
		if( panel_client.is_connected())
		{
			panel_client.prepare( context->id) ;
			panel_client.turn_off( context->id) ;
			panel_client.focus_out( context->id) ;
			panel_client.send() ;
		}

		(*context->cb->preedit_update)( context->self ,
						NULL , 0) ;
		(*context->cb->candidate_hide)( context->self) ;

		context->instance->focus_out() ;

		context->on = 0 ;

		return  0 ;
	case SCIM_FRONTEND_HOTKEY_NEXT_FACTORY:
	case SCIM_FRONTEND_HOTKEY_PREVIOUS_FACTORY:
	case SCIM_FRONTEND_HOTKEY_SHOW_FACTORY_MENU:
		// not implemented yet
		return  0 ;
	default:
		return  1 ;
	}
}


// --- global functions ---

int
im_scim_initialize( char *  locale)
{
	SocketAddress  address;
	SocketClient  client;
	uint32  magic ;
	std::vector<String>  imengines ;
	std::vector<String>  config_modules ;
	String  config_mod_name ;

	lang = scim_get_locale_language( String( locale)) ;

	address.set_address( scim_get_default_socket_frontend_address());
	if( ! client.connect(address) &&
	    ! scim_socket_open_connection( magic ,
					   String( "ConnectionTester") ,
					   String( "SocketFrontEnd") ,
					   client ,
					   TIMEOUT))
	{
		kik_error_printf( "Unable to connect to the socket frontend.\n") ;
		goto  error ;
	}

	if( scim_get_imengine_module_list( imengines) == 0)
	{
		kik_error_printf( "Could not find any IMEngines.\n") ;
		goto  error ;
	}

	if( std::find( imengines.begin() , imengines.end() , "socket")
							== imengines.end())
	{
		kik_error_printf( "Could not find socket module.\n");
		goto  error ;
	}

	imengines.clear() ;
	imengines.push_back( "socket") ;

	if( scim_get_config_module_list( config_modules) == 0)
	{
		kik_error_printf( "Could not find any config modules.\n") ;
		goto  error ;
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
		goto  error ;
	}

	config = config_module->create_config() ;

	if( config.null())
	{
		// TODO fallback DummyConfig
		kik_error_printf( "create_config failed.\n") ;
		goto  error ;
	}

	be = new CommonBackEnd( config , imengines) ;
	if( be.null())
	{
		kik_error_printf( "CommonBackEnd failed.\n") ;
		goto  error ;
	}

	cb_config_load( config) ;
	config->signal_connect_reload( slot(cb_config_load)) ;

	panel_client.signal_connect_request_factory_menu( slot (cb_panel_request_factory_menu));
	panel_client.signal_connect_request_help( slot (cb_panel_request_help)) ;
	panel_client.signal_connect_change_factory( slot (cb_panel_change_factory)) ;

	panel_client.signal_connect_trigger_property( slot (cb_panel_trigger_property)) ;

	if (panel_client.open_connection( config->get_name() , getenv( "DISPLAY")) == false)
	{
		goto  error;
	}

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

	if( panel_client.is_connected())
	{
		panel_client.close_connection() ;
	}

	return  0 ;
}

int
im_scim_finalize( void)
{
	if( panel_client.is_connected())
	{
		panel_client.close_connection() ;
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
	im_scim_callbacks_t *  callbacks
	)
{
	im_scim_context_private_t *  context = NULL ;

	context = new im_scim_context_private_t ;

	context->factory = be->get_default_factory( lang , String( "UTF-8")) ;

	if( ! ( context->instance = context->factory->create_instance( String( "UTF-8") , id)))
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

	if( panel_client.is_connected())
	{
		panel_client.prepare( context->id) ;
		panel_client.focus_in( context->id ,
				       context->instance->get_factory_uuid()) ;
		if( context->on)
		{
			PanelFactoryInfo  info ;
			info = PanelFactoryInfo(
				context->factory->get_uuid() ,
				utf8_wcstombs( context->factory->get_name()) ,
				context->factory->get_language() ,
				context->factory->get_icon_file()) ;
			panel_client.update_factory_info( context->id, info) ;
			panel_client.turn_on( context->id) ;
		}
		else
		{
			panel_client.turn_off( context->id) ;
		}
		panel_client.send() ;
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

	if( panel_client.is_connected())
	{
		panel_client.prepare( context->id) ;
		panel_client.turn_off( context->id) ;
		panel_client.focus_in( context->id , context->instance->get_factory_uuid ()) ;
		panel_client.send() ;
	}

	context->instance->focus_out() ;

	(*context->cb->candidate_hide)( context->self) ;

	context->focused = 0 ;

	return 1 ;
}

int
im_scim_is_on(
	im_scim_context_t  context
	)
{
	if( ((im_scim_context_private_t*) context)->on)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
im_scim_switch_mode(
	im_scim_context_t  context
	)
{
	KeyEventList  keys ;
	size_t  size ;

	if( ( size = keymatcher_frontend.find_hotkeys( SCIM_FRONTEND_HOTKEY_TRIGGER , keys)) > 0)
	{
		return  (hotkey( context , keys[0]) == 0) ;
	}
	else
	{
		return  0 ;
	}
}

int
im_scim_key_event(
	im_scim_context_t  context ,
	KeySym  ksym ,
	XKeyEvent *  event
	)
{
	KeyEvent  scim_key ;

	scim_key.mask = event->state & valid_key_mask;
	scim_key.code = ksym ;

	if( hotkey( context , scim_key) == 0)
	{
		return  0 ;
	}

	if( ! ((im_scim_context_private_t *)context)->on)
	{
		return  1 ;
	}

	if( ((im_scim_context_private_t *)context)->instance->process_key_event( scim_key))
	{
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
	if( panel_client.is_connected())
	{
		return  panel_client.get_connection_number() ;
	}

	return  -1 ;
}

int
im_scim_receive_panel_event( void)
{
	panel_client.filter_event() ;

	return  1 ;
}

