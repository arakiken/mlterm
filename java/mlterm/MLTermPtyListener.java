/*
 *	$Id$
 */

package  mlterm ;


public interface MLTermPtyListener
{
	public void  executeCommand( String  cmd) ;

	public void  lineScrolledOut() ;

	public void  windowScrolled() ;
}
