/*
 *	$Id$
 */

package  mlterm.native_activity ;

import  android.app.NativeActivity ;
import  android.view.KeyEvent ;
import  android.view.WindowManager ;
import  android.view.WindowManager.LayoutParams ;
import  android.view.inputmethod.InputMethodManager ;
import  android.view.View ;
import  android.content.Context ;
import  android.os.Bundle ;
import  android.graphics.Rect ;
import  java.io.* ;


public class MLActivity extends NativeActivity
{
	static
	{
		System.loadLibrary( "mlterm") ;
	}

	private native void  visibleFrameChanged( int  yoffset , int  width , int  height) ;

	public String  keyString ;

	public void showSoftInput()
	{
		((InputMethodManager)getSystemService( Context.INPUT_METHOD_SERVICE)).showSoftInput(
			getWindow().getDecorView() , InputMethodManager.SHOW_FORCED) ;
	}

	@Override
	public boolean dispatchKeyEvent( KeyEvent  event)
	{
		if( event.getAction() == KeyEvent.ACTION_MULTIPLE)
		{
			keyString = event.getCharacters() ;
		}

		return  super.dispatchKeyEvent( event) ;
	}

	@Override
	protected void onCreate( Bundle  state)
	{
		View  view ;

		super.onCreate( state) ;

		getWindow().setSoftInputMode( WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE) ;

		view = getWindow().getDecorView() ;

		/* android-11 or later */
		view.addOnLayoutChangeListener(
			new View.OnLayoutChangeListener()
			{
				@Override
				public void onLayoutChange( View  v , int  left , int  top ,
						int  right , int  bottom , int  oldLeft , int  oldTop ,
						int  oldRight , int  oldBottom)
				{
					Rect  r = new Rect() ;
					v.getWindowVisibleDisplayFrame(r) ;
					visibleFrameChanged( r.top , r.right , r.bottom - r.top) ;
				}
			}) ;

		if( true)
		{
			view.setOnFocusChangeListener(
				new View.OnFocusChangeListener()
				{
					@Override
					public void onFocusChange( View  v , boolean  hasFocus)
					{
						InputMethodManager  m = (InputMethodManager)getSystemService(
													Context.INPUT_METHOD_SERVICE) ;
						if( hasFocus)
						{
							m.showSoftInput( v , InputMethodManager.SHOW_FORCED) ;
						}
						else
						{
							m.hideSoftInputFromWindow( v.getWindowToken() , 0) ;
						}
					}
				}) ;

			view.setFocusable( true) ;
			view.setFocusableInTouchMode( true) ;
			view.requestFocus() ;
		}
		else
		{
			((InputMethodManager)getSystemService( Context.INPUT_METHOD_SERVICE)).showSoftInput(
						view , InputMethodManager.SHOW_FORCED) ;
		}
	}

	public String saveUnifont()
	{
		File  file = getFileStreamPath( "unifont.pcf") ;
		if( ! file.exists())
		{
			try
			{
				InputStream  is = getResources().getAssets().open( "unifont.pcf") ;
				OutputStream  os = openFileOutput( "unifont.pcf" , 0) ;
				int  len ;
				byte[]  buf = new byte[102400] ;

				while( ( len = is.read( buf)) >= 0)
				{
					os.write( buf , 0 , len) ;
				}

				os.close() ;
				is.close() ;

				System.err.println( "unifont.pcf written.\n") ;
			}
			catch(Exception  e)
			{
			}
		}

		return  file.getPath() ;
	}
}
