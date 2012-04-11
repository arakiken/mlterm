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
	final private static boolean  DEBUG = false ;

	private Object  auxData = null ;
	
	private native static void  setLibDir( String  dir) ;

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
				int  pos = url.getPath().indexOf( "mlterm") ;
				if( pos != -1 &&
				    /* is end element of path. */
				    url.getPath().indexOf( System.getProperty( "file.separator") , pos) == -1 &&
				    /* is jar file. */
				    url.getPath().indexOf( ".jar" , pos) != -1)
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

		String  dir = getConfigDirectory() + "java" + System.getProperty( "file.separator") ;
		File  d = new File( dir) ;
		d.mkdir() ;
		d.setWritable( true , true) ;
		d.setReadable( true , true) ;

		String[]  files = mf.getMainAttributes().getValue( "Bundle-NativeCode").split( ";") ;

		byte[]  buf = new byte[4096] ;
		for( int  count = 0 ; count < files.length ; count++)
		{
			if( true)
			{
				System.out.println( "Writing " + dir + files[count]) ;
			}

			InputStream  is = Thread.currentThread().getContextClassLoader().
									getResourceAsStream( files[count]) ;
			try
			{
				OutputStream  os = new FileOutputStream( dir + files[count] , false) ;
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
					files[count].indexOf( '_') == -1 &&	/* libmkf_xxx is not loaded. */
					files[count].indexOf( ".exe") == -1)/* plink.exe is not loaded. */
				{
					try
					{
						if( true)
						{
							System.out.print( "Loading " + dir + files[count]) ;
						}

						System.load( dir + files[count]) ;
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

		setLibDir( dir) ;
	}

	static
	{
		try
		{
			if( DEBUG)
			{
				System.out.println( System.getProperty( "java.library.path")) ;
			}

			System.loadLibrary( "MLTermPty") ;
		}
		catch( UnsatisfiedLinkError  e)
		{
			if( DEBUG)
			{
				e.printStackTrace() ;
			}

			loadLibraryFromJar() ;
		}
	}

	public static String  getConfigDirectory()
	{
		StringBuilder  strb = new StringBuilder() ;
		strb.append( System.getProperty( "user.home")) ;
		strb.append( System.getProperty( "file.separator")) ;
		if( System.getProperty( "os.name").indexOf( "Windows") >= 0)
		{
			strb.append( "mlterm") ;
		}
		else
		{
			strb.append( ".mlterm") ;
		}
		strb.append( System.getProperty( "file.separator")) ;

		return  strb.toString() ;
	}

	public void  setAuxData( Object  data)
	{
		auxData = data ;
	}

	public Object  getAuxData()
	{
		return  auxData ;
	}

	private long  nativeObj = 0 ;

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

	private native void  nativeSetListener( long  obj , Object  listener) ;
	public void  setListener( MLTermPtyListener  listener)
	{
		nativeSetListener( nativeObj , listener) ;
	}

	public native static boolean  waitForReading() ;

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

	private native int  nativeGetRows( long  obj) ;
	public int  getRows()
	{
		return  nativeGetRows( nativeObj) ;
	}

	private native int  nativeGetCols( long  obj) ;
	public int  getCols()
	{
		return  nativeGetCols( nativeObj) ;
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

	private native boolean  nativeIsAppCursorKeys( long  obj) ;
	public boolean  isAppCursorKeys()
	{
		return  nativeIsAppCursorKeys( nativeObj) ;
	}

	private native void  nativeReportMouseTracking( long  obj ,
							int  char_index , int  row , int  button ,
							int  state , boolean  isMotion , boolean  isReleased) ;
	public void  reportMouseTracking( int  char_index , int  row , int  button ,
							int  state , boolean  isMotion , boolean  isReleased)
	{
		nativeReportMouseTracking( nativeObj , char_index , row ,
						button , state , isMotion , isReleased) ;
	}

	public native static long  getColorRGB( String  color) ;
}
