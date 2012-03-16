/*
 *	$Id$
 */

package  mlterm ;


public interface MLTermPtyListener
{
	public void  executeCommand( String  cmd) ;

	public void  lineScrolledOut() ;

//	public void  scrollUpwardRegion( int  begRow , int  endRow , int  size) ;

//	public void  scrollDownwardRegion( int  begRow , int  endRow , int  size) ;
}
