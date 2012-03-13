/*
 *	$Id$
 */

package  mlterm ;


import  java.awt.* ;
import  java.applet.* ;
import  org.eclipse.swt.awt.* ;
import  org.eclipse.swt.* ;
import  org.eclipse.swt.widgets.* ;
import  org.eclipse.swt.layout.* ;


/* applet class */
public class  MLTermApplet extends Applet
{
	private Thread  kick = null ;

	public void init()
	{
		if( kick != null)
		{
			return ;
		}

		kick = new Thread(
				new Runnable()
				{
					public void  run()
					{
						setLayout( new java.awt.GridLayout( 1 , 1)) ;
						java.awt.Canvas canvas = new java.awt.Canvas() ;
						add( canvas) ;

						Display display = new Display() ;
						Shell shell = SWT_AWT.new_Shell( display , canvas) ;
						shell.setText( "mlterm") ;
						shell.setLayout( new FillLayout()) ;

						String  host = null ;
						String  pass = null ;
						String  encoding = null ;
						ConnectDialog  dialog = new ConnectDialog( shell) ;
						String[]  array = dialog.open( host) ;
						if( array != null)
						{
							host = array[0] ;
							pass = array[1] ;
							if( array[2] != null)
							{
								encoding = array[2] ;
							}
						}
						dialog = null ;

						MLTerm mlterm = new MLTerm( shell , SWT.BORDER|SWT.V_SCROLL ,
												host , pass , 80 , 24 , encoding , null) ;

						String  fontFamily ;
						if( System.getProperty( "os.name").indexOf( "Windows") >= 0)
						{
							fontFamily = "Terminal" ;
						}
						else
						{
							fontFamily = "monospace" ;
						}

						mlterm.setFont( new org.eclipse.swt.graphics.Font(
											display , fontFamily , 10 , SWT.NORMAL)) ;

						mlterm.resetSize() ;

						Dimension  d = getSize() ;
						mlterm.resizePty( d.width , d.height) ;

						org.eclipse.swt.graphics.Point  p = mlterm.getSize() ;
						setSize( p.x , p.y) ;
						validate() ;

						while( ! shell.isDisposed())
						{
							while( display.readAndDispatch()) ;

							if( ! mlterm.updatePty())
							{
								break ;
							}

							if( ! display.readAndDispatch())
							{
								display.sleep() ;
							}
						}

						display.dispose() ;
						remove( canvas) ;
					}
				}) ;

		kick.start() ;
	}

	public void stop()
	{
		if( kick != null)
		{
			kick.interrupt() ;
			kick = null ;
		}
	}
}
