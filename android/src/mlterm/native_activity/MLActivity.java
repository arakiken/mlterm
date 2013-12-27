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
import  android.view.View.OnLayoutChangeListener ;
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
		super.onCreate( state) ;

		InputMethodManager  m = (InputMethodManager)getSystemService(
									Context.INPUT_METHOD_SERVICE) ;
		m.showSoftInput( getWindow().getDecorView() , 0) ;
		getWindow().setSoftInputMode( WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE) ;

		/* android-11 or later */
		getWindow().getDecorView().addOnLayoutChangeListener(
			new OnLayoutChangeListener()
			{
				@Override
				public void onLayoutChange( View  v , int  left , int  top ,
						int  right , int  bottom , int  oldLeft , int  oldTop ,
						int  oldRight , int  oldBottom)
				{
					Rect  r = new Rect() ;
					getWindow().getDecorView().getWindowVisibleDisplayFrame(r) ;
					visibleFrameChanged( r.top , r.right , r.bottom - r.top) ;
				}
			}) ;
	}

	public void saveUnifont()
	{
		File  file = new File("/sdcard/.mlterm/unifont.pcf") ;
		if( file.exists())
		{
			return ;
		}

		try
		{
			File  dir = new File( "/sdcard/.mlterm") ;
			if( ! dir.isDirectory())
			{
				dir.mkdir() ;
			}

			InputStream  is = getResources().getAssets().open( "unifont.pcf") ;
			OutputStream  os = new FileOutputStream( "/sdcard/.mlterm/unifont.pcf") ;
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
}
