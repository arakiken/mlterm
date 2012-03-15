/*
 *	$Id$
 */

package  mlterm ;


import  java.io.* ;
import  java.util.jar.* ;
import  java.net.* ;
import  java.util.* ;


public class  MLTermPty
{
	private native static void  setAltLibDir( String  dir) ;

	private static void  loadLibraryFromJar()
	{
		Manifest  mf = null ;

		try
		{
			Enumeration  urls = Thread.currentThread().getContextClassLoader().
									getResources( "META-INF/MANIFEST.MF") ;

			while( urls.hasMoreElements())
			{
				URL  url = (URL)urls.nextElement() ;
				if( url.getPath().indexOf( "mlterm.jar") != -1)
				{
					InputStream  is = url.openStream() ;

					try
					{
						mf = new Manifest(is) ;
					}
					catch( IOException  e1)
					{
					}

					is.close() ;
					break ;
				}
			}
		}
		catch( IOException  e2)
		{
			return ;
		}

		if( mf == null)
		{
			return ;
		}

		String[]  files = mf.getMainAttributes().getValue( "Bundle-NativeCode").split( ";") ;
		String  tmpdir = System.getProperty( "java.io.tmpdir") + "/" ;
		byte[]  buf = new byte[4096] ;
		for( int  count = 0 ; count < files.length ; count++)
		{
			if( true)
			{
				System.out.println( "Writing " + tmpdir + files[count]) ;
			}

			InputStream  is = Thread.currentThread().getContextClassLoader().
									getResourceAsStream( files[count]) ;
			try
			{
				OutputStream  os = new FileOutputStream( tmpdir + files[count] , false) ;
				try
				{
					while( true)
					{
						int  size = is.read( buf) ;
						if( size < 0)
						{
							break ;
						}
						os.write( buf , 0 , size) ;
					}
				}
				catch( IOException  e3)
				{
				}
				os.close() ;
			}
			catch( IOException  e4)
			{
			}

			try
			{
				is.close() ;
			}
			catch( IOException  e5)
			{
			}
		}

		boolean[]  results = new boolean[files.length] ;	/* init value is false. */
		boolean  errorHappened = false ;

		for( int  trial = 0 ; trial < files.length ; trial++)
		{
			errorHappened = false ;

			for( int  count = 0 ; count < files.length ; count++)
			{
				if( ! results[count] &&
					files[count].indexOf( '_') == -1)	/* libmkf_xxx is not loaded. */
				{
					try
					{
						if( true)
						{
							System.out.print( "Loading " + tmpdir + files[count]) ;
						}

						System.load( tmpdir + files[count]) ;
						results[count] = true ;

						if( true)
						{
							System.out.println( " ... done.") ;
						}
					}
					catch( UnsatisfiedLinkError  e)
					{
						if( true)
						{
							System.out.println( " ... failed.") ;
						}

						errorHappened = true ;
					}
				}
			}

			if( ! errorHappened)
			{
				break ;
			}
		}

		if( errorHappened)
		{
			throw new UnsatisfiedLinkError() ;
		}

		setAltLibDir( tmpdir) ;
	}

	static
	{
		try
		{
			if( false)
			{
				System.out.println( System.getProperty( "java.library.path")) ;
			}

			System.loadLibrary( "MLTermPty") ;
		}
		catch( UnsatisfiedLinkError  e)
		{
			if( false)
			{
				e.printStackTrace() ;
			}

			loadLibraryFromJar() ;
		}
	}

	private long  nativeObj = 0 ;

	private MLTermPtyListener  listener = null ;

	private void  lineScrolledOut()
	{
		if( listener != null)
		{
			listener.lineScrolledOut() ;
		}
	}

	public void  setListener( MLTermPtyListener  lsn)
	{
		listener = lsn ;
	}

	private native long nativeOpen( String  host , String  pass , int  cols , int  rows ,
							String  encoding , String[]  argv) ;

	public boolean open( String  host , String  pass , int  cols , int  rows ,
						String  encoding , String[]  argv)
	{
		nativeObj = nativeOpen( host , pass , cols , rows , encoding , argv) ;
		if( nativeObj == 0)
		{
			return  false ;
		}
		else
		{
			return  true ;
		}
	}

	private native void nativeClose( long  obj) ;

	public void close()
	{
		nativeClose( nativeObj) ;
		nativeObj = 0 ;
	}

	private native boolean nativeIsActive( long  obj) ;

	public boolean isActive()
	{
		return  nativeIsActive( nativeObj) ;
	}

	private native boolean nativeRead( long  obj) ;

	public boolean read()
	{
		return  nativeRead( nativeObj) ;
	}

	private native boolean nativeWrite( long  obj , String  str) ;

	public boolean write( String  str)
	{
		return  nativeWrite( nativeObj , str) ;
	}

	private native boolean  nativeResize( long  obj , int  cols , int  rows) ;

	public void resize( int  cols , int  rows)
	{
		nativeResize( nativeObj , cols , rows) ;
	}

	private native boolean nativeGetRedrawString( long  obj , int  row , RedrawRegion  region) ;

	public boolean getRedrawString( int row , RedrawRegion  region)
	{
		return  nativeGetRedrawString( nativeObj , row , region) ;
	}

	private native int  nativeGetCaretRow( long  obj) ;

	public int  getCaretRow()
	{
		return  nativeGetCaretRow( nativeObj) ;
	}

	private native int  nativeGetCaretCol( long  obj) ;

	public int  getCaretCol()
	{
		return  nativeGetCaretCol( nativeObj) ;
	}
}
