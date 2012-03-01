/*
 *	$Id$
 */

package  mlterm ;


public class  MLTermPty
{
	static
	{
		System.loadLibrary( "MLTermPty") ;
	}

	private long  nativeObj ;

	private native long nativeOpen( String  host , String  pass , int  cols , int  rows) ;

	public boolean open( String  host , String  pass , int  cols , int  rows)
	{
		nativeObj = nativeOpen( host , pass , cols , rows) ;
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
