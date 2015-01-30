/*
 *	$Id$
 */

package  mlterm.native_activity ;

import  android.app.NativeActivity ;
import  android.view.KeyEvent ;
import  android.view.WindowManager ;
import  android.view.WindowManager.LayoutParams ;
import  android.view.inputmethod.InputMethodManager ;
import  android.view.inputmethod.InputConnection ;
import  android.view.inputmethod.BaseInputConnection ;
import  android.view.inputmethod.EditorInfo ;
import  android.view.View ;
import  android.view.ViewGroup ;
import  android.text.InputType ;
import  android.content.Context ;
import  android.os.Bundle ;
import  android.graphics.Rect ;
import  android.graphics.BitmapFactory ;
import  android.graphics.Bitmap ;
import  android.util.AttributeSet ;
import  java.net.URL ;
import  java.io.* ;


public class MLActivity extends NativeActivity
{
	static
	{
		System.loadLibrary( "mlterm") ;
	}

	private native void  visibleFrameChanged( int  yoffset , int  width , int  height) ;
	private native void  commitText( String  str) ;
	private native void  preeditText( String  str) ;
	private native int  hashPath( String  path) ;
	private native void  splitAnimationGif( String  path) ;

	private boolean  isPreediting ;
	private String  keyString ;
	private View  contentView ;

	private class TextInputConnection extends BaseInputConnection
	{
		public TextInputConnection( View  v , boolean  fulledit)
		{
			super( v , fulledit) ;
		}

		@Override
		public boolean commitText( CharSequence  text , int  newCursorPosition)
		{
			super.commitText( text , newCursorPosition) ;

			isPreediting = false ;
			commitText( text.toString()) ;

			return  true ;
		}

		@Override
		public boolean setComposingText( CharSequence  text , int  newCursorPosition)
		{
			super.setComposingText( text , newCursorPosition) ;

			isPreediting = true ;
			preeditText( text.toString()) ;

			return  true ;
		}

		@Override
		public boolean finishComposingText()
		{
			super.finishComposingText() ;

			isPreediting = false ;

			return  true ;
		}
	}

	private class NativeContentView extends View
	{
		public NativeContentView( Context  context)
		{
			super( context) ;
		}

		public NativeContentView( Context  context , AttributeSet  attrs)
		{
			super( context , attrs) ;
		}

		@Override
		public boolean onCheckIsTextEditor()
		{
			return  true ;
		}

		@Override
		public InputConnection onCreateInputConnection( EditorInfo  outAttrs)
		{
			outAttrs.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE ;
			outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_ENTER_ACTION |
			                      EditorInfo.IME_ACTION_DONE ;

			return  new TextInputConnection( this , true) ;
		}
	}

	private void showSoftInput()
	{
		((InputMethodManager)getSystemService( Context.INPUT_METHOD_SERVICE)).showSoftInput(
			contentView , InputMethodManager.SHOW_FORCED) ;
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
		super.onCreate( state) ;

		getWindow().setSoftInputMode( WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE) ;

		if( false)
		{
			/* setContentView() is called in NativeActivity.onCreate(). */
			contentView = ((ViewGroup)findViewById(android.R.id.content)).getChildAt(0) ;
		}
		else
		{
			contentView = new NativeContentView( this) ;
			setContentView( contentView) ;
			contentView.getViewTreeObserver().addOnGlobalLayoutListener( this) ;
		}

		contentView.setFocusable( true) ;
		contentView.setFocusableInTouchMode( true) ;
		contentView.requestFocus() ;

		/* android-11 or later */
		getWindow().getDecorView().addOnLayoutChangeListener(
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
	}

	@Override
	protected void onPause()
	{
		super.onPause() ;

		((InputMethodManager)getSystemService(
			Context.INPUT_METHOD_SERVICE)).hideSoftInputFromWindow(
				contentView.getWindowToken() , 0) ;
	}

	@Override
	protected void onRestart()
	{
		super.onRestart() ;

		showSoftInput() ;
	}

	private String saveDefaultFont()
	{
		File  file = getFileStreamPath( "unifont.pcf") ;
		if( ! file.exists() ||
		    /* If unifont.pcf is older than apk, rewrite unifont.pcf. */
		    (new File( getApplicationInfo().publicSourceDir)).lastModified() > file.lastModified())
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

	private int[]  getBitmap( String  path , int  width , int  height)
	{
		try
		{
			InputStream  is ;

			if( path.indexOf( "://") != -1)
			{
				URL  url = new URL( path) ;
				is = url.openConnection().getInputStream() ;

				if( path.indexOf( ".gif") != -1)
				{
					String  tmp = "/sdcard/.mlterm/anim" +
									Integer.toString( hashPath( path)) + ".gif" ;
					OutputStream  os = new FileOutputStream( tmp) ;

					int  len ;
					byte[]  buf = new byte[10240] ;

					while( ( len = is.read( buf)) >= 0)
					{
						os.write( buf , 0 , len) ;
					}

					os.close() ;
					is.close() ;

					splitAnimationGif( path) ;
					is = new FileInputStream( tmp) ;
				}
			}
			else
			{
				if( path.indexOf( ".gif") != -1)
				{
					splitAnimationGif( path) ;
				}

				is = new FileInputStream( path) ;
			}

			Bitmap  bmp = BitmapFactory.decodeStream( is) ;

			/* decodeStream() can return null. */
			if( bmp != null)
			{
				if( width != 0 && height != 0)
				{
					bmp = Bitmap.createScaledBitmap( bmp , width , height , false) ;
				}

				width = bmp.getWidth() ;
				height = bmp.getHeight() ;
				int[]  pixels = new int[width * height + 2] ;

				bmp.getPixels( pixels , 0 , width , 0 , 0 , width , height) ;
				pixels[pixels.length - 2] = width ;
				pixels[pixels.length - 1] = height ;

				return  pixels ;
			}
		}
		catch( Exception  e)
		{
			System.err.println( e) ;
		}

		return  null ;
	}
}
