/*
 *	$Id$
 */

package  mlterm ;


import  org.eclipse.swt.SWT ;
import  org.eclipse.swt.custom.StyleRange ;
import  org.eclipse.swt.custom.StyledText ;
import  org.eclipse.swt.events.VerifyEvent ;
import  org.eclipse.swt.custom.VerifyKeyListener ;
import  org.eclipse.swt.custom.CaretEvent ;
import  org.eclipse.swt.custom.CaretListener ;
import  org.eclipse.swt.graphics.Color ;
import  org.eclipse.swt.graphics.Font ;
import  org.eclipse.swt.graphics.FontMetrics ;
import  org.eclipse.swt.graphics.GC ;
import  org.eclipse.swt.graphics.Rectangle ;
import  org.eclipse.swt.graphics.Point ;
import  org.eclipse.swt.layout.FillLayout ;
import  org.eclipse.swt.widgets.Canvas ;
import  org.eclipse.swt.widgets.Composite ;
import  org.eclipse.swt.widgets.Display ;
import  org.eclipse.swt.widgets.Shell ;


public class MLTerm extends StyledText
{
	private MLTermPty  pty ;
	private int  ptyCols ;
	private int  ptyRows ;
	private Color[]  colors ;
	private RedrawRegion  region ;

	private  void moveCaret()
	{
		int  row = pty.getCaretRow() ;
		int  lineCount = getLineCount() ;
		int  offset ;
		if( row >= lineCount)
		{
			for( ; lineCount <= row ; lineCount++)
			{
				append( String.valueOf( '\n')) ;
			}

			offset = getCharCount() -  1 ;
		}
		else
		{
			offset = getOffsetAtLine( row) + pty.getCaretCol() ;
		}

		setCaretOffset( offset) ;
	}

	public MLTerm( Composite  parent , int  style , String  host , String  pass ,
				int  cols , int  rows)
	{
		super( parent , style) ;

		pty = new MLTermPty() ;
		if( ! pty.open( host , pass , cols , rows))
		{
			return ;
		}

		ptyCols = cols ;
		ptyRows = rows ;

		addVerifyKeyListener(
			new VerifyKeyListener()
			{
				public void verifyKey( VerifyEvent	event)
				{
					String  str ;
					if( event.keyCode == 0x1000001)
					{
						str = "\u001b[A" ;
					}
					else if( event.keyCode == 0x1000002)
					{
						str = "\u001b[B" ;
					}
					else if( event.keyCode == 0x1000003)
					{
						str = "\u001b[D" ;
					}
					else if( event.keyCode == 0x1000004)
					{
						str = "\u001b[C" ;
					}
					else
					{
						str = String.valueOf( event.character) ;
					}

					pty.write( str) ;
					event.doit = false ;
				}
			}) ;
		addCaretListener(
			new CaretListener()
			{
				public void caretMoved( CaretEvent  event)
				{
					moveCaret() ;
				}
			}) ;

		colors = new Color[258] ;
		colors[0x100] = getForeground() ;
		colors[0x101] = getBackground() ;

		region = new RedrawRegion() ;
	}

	public void  closePty()
	{
		pty.close() ;
	}

	public boolean  updatePty()
	{
		if( ! pty.isActive())
		{
			return  false ;
		}
		else if( pty.read())
		{
			for( int  row = 0 ; row < ptyRows ; row ++)
			{
				if( ! pty.getRedrawString( row , region))
				{
					continue ;
				}

				if( row == ptyRows - 1)
				{
					/* Remove '\n' which region.str always contains. */
					region.str = region.str.substring( 0 , region.str.length() - 1) ;
				}

				int  lineCount ;
				for( lineCount = getLineCount() ; lineCount <= row ; lineCount++)
				{
					append( String.valueOf( '\n')) ;
				}

				int	 offset ;
				int	 nextRowOffset ;
				if( row >= lineCount)
				{
					nextRowOffset = offset = getCharCount() ;
				}
				else
				{
					offset = getOffsetAtLine( row) + region.start ;

					if( row + 1 >= lineCount)
					{
						nextRowOffset = getCharCount() ;
					}
					else
					{
						nextRowOffset = getOffsetAtLine( row + 1) ;
					}

					/* XXX */
					if( offset > nextRowOffset)
					{
						System.out.printf( "%s %d %d %d %d%n" , region.str ,
							row , lineCount , offset , nextRowOffset) ;
						offset = nextRowOffset ;
					}
				}

				replaceTextRange( offset , nextRowOffset - offset , region.str) ;

				if( region.styles != null)
				{
					StyleRange[]  styles = new StyleRange[region.styles.length] ;

					for( int  count = 0 ; count < region.styles.length ; count++)
					{
						if( colors[ region.styles[count].fg_color] == null)
						{
							colors[ region.styles[count].fg_color] =
								new Color( getDisplay() ,
										(region.styles[count].fg_pixel >> 16) & 0xff ,
										(region.styles[count].fg_pixel >> 8) & 0xff ,
										region.styles[count].fg_pixel & 0xff) ;
						}

						if( colors[ region.styles[count].bg_color] == null)
						{
							colors[ region.styles[count].bg_color] =
								new Color( getDisplay() ,
										(region.styles[count].bg_pixel >> 16) & 0xff ,
										(region.styles[count].bg_pixel >> 8) & 0xff ,
										region.styles[count].bg_pixel & 0xff) ;
						}

						styles[count] = new StyleRange(
											region.styles[count].start + offset ,
											region.styles[count].length ,
											colors[ region.styles[count].fg_color] ,
											colors[ region.styles[count].bg_color]) ;
						styles[count].underline = region.styles[count].underline ;
						styles[count].fontStyle = region.styles[count].bold ?
													SWT.BOLD : SWT.NORMAL ;
					}

					replaceStyleRanges( offset , region.str.length() , styles) ;
				}
			}

			if( getLineCount() > ptyRows)
			{
				setTopIndex( getLineCount() - ptyRows) ;
			}

			moveCaret() ;
		}

		return  true ;
	}

	public static void main (String [] args)
	{
		String  host = System.getenv( "DISPLAY") ;
		String  pass = null ;

		for( int  count = 0 ; count < args.length ; count ++)
		{
			if( args[count].equals( "-h"))
			{
				System.out.println( "usage: java -jar mlterm.jar [options]") ;
				System.out.println( "options: -serv ssh://user@host") ;
				System.out.println( "         -pass password") ;

				return ;
			}
			else if( count + 1 < args.length)
			{
				if( args[count].equals( "-serv"))
				{
					host = args[++count] ;
				}
				else if( args[count].equals( "-pass"))
				{
					pass = args[++count] ;
				}
			}
		}

		if( host == null)
		{
			host = ":0.0" ;
		}

		int  cols = 80 ;
		int  rows = 24 ;

		Display display = new Display() ;

		Shell shell = new Shell(display) ;
		shell.setText( "mlterm") ;
		shell.setLayout( new FillLayout()) ;

		MLTerm  mlterm = new MLTerm( shell , SWT.BORDER|SWT.V_SCROLL , host , pass , cols , rows) ;
		mlterm.setLeftMargin( 1) ;
		mlterm.setRightMargin( 1) ;
		mlterm.setTopMargin( 1) ;
		mlterm.setBottomMargin( 1) ;
		if( System.getProperty( "os.name").indexOf( "Windows") >= 0)
		{
			mlterm.setFont( new Font( display , "MS Gothic" , 10 , SWT.NORMAL)) ;
		}
		else
		{
			mlterm.setFont( new Font( display , "Kochi Gothic" , 10 , SWT.NORMAL)) ;
		}

		GC  gc = new GC( mlterm) ;
		FontMetrics  fm = gc.getFontMetrics() ;
		gc.dispose() ;

		int  width = cols * fm.getAverageCharWidth() + 6 ;
		int  height = rows * fm.getHeight() + 6 ;
		shell.setSize( width , height) ;

		Rectangle  r = shell.getClientArea() ;
		shell.setSize( width * 2 - r.width , height * 2 - r.height) ;

		shell.open() ;

		if( false)
		{
			System.out.printf( "CHW %d CHH %d LH %d LS %d%n" ,
				fm.getAverageCharWidth() , fm.getHeight() ,
				mlterm.getLineHeight() , mlterm.getLineSpacing()) ;
		}

		while( ! shell.isDisposed())
		{
			if( ! display.readAndDispatch())
			{
				display.sleep() ;
			}

			if( ! mlterm.updatePty())
			{
				break ;
			}
		}

		display.dispose() ;
		mlterm.closePty() ;
	}
}
