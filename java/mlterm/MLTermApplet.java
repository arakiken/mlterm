/*
 *	$Id$
 */

package  mlterm ;


import  java.awt.Dimension ;
import  java.awt.Canvas ;
import  java.applet.Applet ;
import  org.eclipse.swt.awt.* ;
import  org.eclipse.swt.* ;
import  org.eclipse.swt.widgets.* ;
import  org.eclipse.swt.layout.* ;
import  org.eclipse.swt.graphics.* ;


public class  MLTermApplet extends Applet
{
	private Thread  kick = null ;

	private void resetSize( Shell  shell , MLTerm  mlterm)
	{
		Dimension  d = getSize() ;
		mlterm.resizePty( d.width , d.height) ;
		mlterm.resetSize() ;

		Point  p = mlterm.getSize() ;
		shell.setSize( p) ;
		setSize( p.x , p.y) ;

		validate() ;
	}

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

						final Shell shell = SWT_AWT.new_Shell( display , canvas) ;
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

						final MLTerm mlterm = new MLTerm( shell ,
												SWT.NO_BACKGROUND|SWT.BORDER|SWT.V_SCROLL ,
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

						mlterm.setFont( new Font( display , fontFamily , 10 , SWT.NORMAL)) ;

						mlterm.setListener(
							new MLTermListener()
							{
								public void lineHeightChanged()
								{
									resetSize( shell , mlterm) ;
								}

								public void ptyClosed()
								{
								}
							}) ;

						resetSize( shell , mlterm) ;

						MLTerm.startPtyWatcher( display) ;

						while( ! shell.isDisposed())
						{
							while( display.readAndDispatch()) ;

							if( ! mlterm.updatePty())
							{
								break ;
							}

							if( ! display.readAndDispatch())
							{
								synchronized(display)
								{
									display.notifyAll() ;
								}
								display.sleep() ;
							}
						}

						synchronized(display)
						{
							display.notifyAll() ;
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
