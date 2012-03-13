/*
 *	$Id$
 */

package  mlterm ;


import  org.eclipse.swt.* ;
import  org.eclipse.swt.dnd.* ;
import  org.eclipse.swt.custom.* ;
import  org.eclipse.swt.events.* ;
import  org.eclipse.swt.graphics.* ;
import  org.eclipse.swt.layout.* ;
import  org.eclipse.swt.widgets.* ;
import  org.eclipse.swt.browser.* ;


public class MLTerm extends StyledText
{
	private MLTermListener  listener = null ;
	private MLTermPty  pty = null ;
	private int  ptyCols = 80 ;
	private int  ptyRows = 24 ;
	private Color[]  colors = null ;
	private RedrawRegion  region = null ;
	private boolean  mouseMoving = false;
	private Clipboard  clipboard = null ;	/* StyledText.clipboard is not accessible. */
	private int  lineHeight = 0 ;


	/* --- private methods --- */

	private void moveCaret()
	{
		int  row = pty.getCaretRow() + pty.numOfScrolledOutLines ;
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

		if( ! mouseMoving) ;
		{
			setCaretOffset( offset) ;
		}
	}

	private void startBrowser( final MLTerm  mlterm , String  uri)
	{
		final Composite  composite = new Composite( mlterm.getParent() , SWT.NONE) ;
		composite.setLayout( new GridLayout( 2 , false)) ;
		composite.setLayoutData( mlterm.getLayoutData()) ;

		Button  button = new Button( composite , SWT.NONE) ;
		button.setText( "Exit") ;
		button.addSelectionListener(
			new  SelectionAdapter()
			{
				public void widgetSelected( SelectionEvent  e)
				{
					mlterm.setParent( composite.getParent()) ;
					composite.dispose() ;
					mlterm.getParent().layout() ;
				}
			}) ;

		final Text  urlinput = new Text( composite , SWT.BORDER) ;
		urlinput.setLayoutData( new GridData( GridData.FILL_HORIZONTAL)) ;

		final Browser  browser = new Browser( composite , SWT.NONE) ;
		browser.setLayoutData( new GridData( SWT.FILL , SWT.FILL , true , true , 2 , 1)) ;
		browser.addLocationListener(
			new LocationAdapter()
			{
				public void changed( LocationEvent  e)
				{
					urlinput.setText( e.location) ;
				}
			}) ;

		urlinput.addKeyListener(
			new KeyAdapter()
			{
				public void keyPressed( KeyEvent  e)
				{
					if( e.keyCode == 13)
					{
						browser.setUrl( urlinput.getText()) ;
					}
				}
			}) ;

		mlterm.setParent( browser) ;
		browser.setUrl( uri) ;

		composite.getParent().layout() ;
	}

	private void  closePty()
	{
		if( pty != null)
		{
			pty.close() ;
			pty = null ;

			if( listener != null)
			{
				listener.ptyClosed() ;
			}
		}
	}

	private void  redrawPty()
	{
		setRedraw( false) ;

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
			for( lineCount = getLineCount() - pty.numOfScrolledOutLines ;
				 lineCount <= row ; lineCount++)
			{
				append( String.valueOf( '\n')) ;
			}

			/*
			 *   $: new line
			 *
			 *   0123 -> insert "e$" at 7 -> 012345678
			 *   abc$                        abc    e$
			 *
			 *   1) offset: 7, offsetNextRow: 4
			 *   2) insert 4(offset - offsetNextRow + 1) spaces at offsetNextRow - 1.
			 *   3) offsetNextRow: 4 -> Modified to 8.
			 *   4) replace 1(offsetNextRow - offset) at offset.
			 *
			 *   012 -> insert "e$" at 7 -> 012345678
			 *   abc                        abc    e$
			 *
			 *   1) offset: 7, offsetNextRow: 3
			 *   2) insert 4(offset - offsetNextRow) spaces at offsetNextRow.
			 *   3) offsetNextRow: 3 -> Modified to 7.
			 *   4) replace 0(offsetNextRow - offset) at offset.
			 */

			int  offset = getOffsetAtLine( row + pty.numOfScrolledOutLines) + region.start ;

			int  offsetNextRow ;
			boolean  hasNewLine = true ;
			if( row + 1 >= lineCount)
			{
				offsetNextRow = getCharCount() ;

				if( offsetNextRow == offset - region.start ||
				    ! getTextRange( offsetNextRow - 1 , 1).equals( "\n"))
				{
					hasNewLine = false ;
				}
			}
			else
			{
				offsetNextRow = getOffsetAtLine( row + pty.numOfScrolledOutLines + 1) ;
			}

			if( offset >= offsetNextRow)
			{
				int  padding = offset - offsetNextRow ;
				if( hasNewLine)
				{
					padding ++ ;
				}

				if( padding > 0)
				{
					char[]  spaces = new char[padding] ;
					for( int  count = 0 ; count < padding ; count++)
					{
						spaces[count] = ' ' ;
					}

					getContent().replaceTextRange( offset - padding , 0 , new String(spaces)) ;

					offsetNextRow += padding ;
				}

				if( false)
				{
					System.out.printf( "%s row %d lineCount %d offset %d offsetNextRow %d" ,
						region.str , row , lineCount , offset , offsetNextRow) ;
				}
			}

			getContent().replaceTextRange( offset , offsetNextRow - offset , region.str) ;

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

		if( lineHeight != getLineHeight())
		{
			if( false)
			{
				System.out.printf( "Line height %d is changed to %d\n" ,
					lineHeight , getLineHeight()) ;
			}

			if( listener != null)
			{
				listener.lineHeightChanged() ;
			}
		}

		if( pty.numOfScrolledOutLines > 0)
		{
			if( pty.numOfScrolledOutLines > getLineCount() - ptyRows)
			{
				pty.numOfScrolledOutLines = getLineCount() - ptyRows ;
			}

			setTopIndex( pty.numOfScrolledOutLines) ;
		}

		moveCaret() ;

		setRedraw( true) ;
		redraw() ;
	}


	/* public methods */

	public MLTerm( Composite  parent , int  style , String  host , String  pass ,
				int  cols , int  rows , String  encoding , String[]  argv)
	{
		super( parent , style) ;

		setLeftMargin( 1) ;
		setRightMargin( 1) ;
		setTopMargin( 1) ;
		setBottomMargin( 1) ;

		getCaret().setVisible( false) ;

		pty = new MLTermPty() ;
		pty.setListener(
			new  MLTermPtyListener()
			{
				public void  executeCommand( String  cmd)
				{
					String[]  argv = cmd.split( " ") ;
					if( argv.length == 2 && argv[0].equals( "browser"))
					{
						startBrowser( MLTerm.this , argv[1]) ;
					}
				}

				public void  redraw()
				{
					Display  display ;

					redrawPty() ;

					display = getDisplay() ;
					while( display.readAndDispatch()) ;
				}
			}) ;

		if( ! pty.open( host , pass , cols , rows , encoding , argv))
		{
			pty = null ;

			return ;
		}

		ptyCols = cols ;
		ptyRows = rows ;

		clipboard = new Clipboard( getDisplay()) ;

		addListener( SWT.Dispose ,
			new Listener()
			{
				public void handleEvent( Event  e)
				{
					closePty() ;
				}
			}) ;

		addMouseListener(
			new  MouseListener()
			{
				public void mouseDown( MouseEvent  event)
				{
					mouseMoving = true ;
				}

				public void mouseUp( MouseEvent  event)
				{
					mouseMoving = false ;

					if( getSelectionCount() > 0)
					{
						copy() ;
					}
				}

				public void mouseDoubleClick( MouseEvent  event)
				{
				}
			}) ;

		addVerifyKeyListener(
			new VerifyKeyListener()
			{
				public void verifyKey( VerifyEvent	event)
				{
					String  str ;

					if( false)
					{
						System.out.printf( "MASK %x KCODE %x\n" ,
							event.stateMask , event.keyCode) ;
					}

					if( (event.keyCode & 0x70000) != 0)
					{
						/* Control, Shift, Alt */

						return ;
					}
					else if( event.stateMask == 0x40000 /* Control */ &&
					    event.keyCode == ' ' /* Space */)
					{
						/* Control + space is not converted to 0x00 automatically. */
						str = "" ;
					}
					else if( event.stateMask == 0x20000 /* SHIFT */ &&
					    event.keyCode == 0x1000009 /* INSERT */)
					{
						str = (String) clipboard.getContents(
										TextTransfer.getInstance() , DND.CLIPBOARD) ;
					}
					else if( event.keyCode == 0x1000001)
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
					else if( event.keyCode == 0x1000005)	/* PAGEUP */
					{
						str = "\u001b[5~" ;
					}
					else if( event.keyCode == 0x1000006)	/* PAGEDOWN */
					{
						str = "\u001b[6~" ;
					}
					else if( event.keyCode == 0x1000007)	/* HOME */
					{
						str = "\u001b[H" ;
					}
					else if( event.keyCode == 0x1000008)	/* END */
					{
						str = "\u001b[F" ;
					}
					else
					{
						str = String.valueOf( event.character) ;
					}

					if( str != null)
					{
						if( event.stateMask == 0x10000 /* Alt */)
						{
							pty.write( "\u001b") ;
						}

						System.out.printf( str) ;
						pty.write( str) ;
					}
					event.doit = false ;
				}
			}) ;

		colors = new Color[258] ;
		colors[0x100] = getForeground() ;
		colors[0x101] = getBackground() ;

		region = new RedrawRegion() ;
	}

	public void  setListener( MLTermListener  lsn)
	{
		listener = lsn ;
	}

	public void  resetSize()
	{
		if( pty == null)
		{
			return ;
		}

		lineHeight = getLineHeight() ;

		int  width = getColumnWidth() * ptyCols + getLeftMargin() +
						getRightMargin() + getBorderWidth() * 2 +
						getVerticalBar().getSize().x - 1 ;
		int  height = lineHeight * ptyRows + getTopMargin() +
						getBottomMargin() + getBorderWidth() * 2 - 1 ;

		super.setSize( width , height) ;
	}

	public void  resizePty( int  width , int  height)
	{
		if( pty == null)
		{
			return ;
		}

		ptyCols =  (width - getLeftMargin() - getRightMargin() - getBorderWidth() * 2 -
						getVerticalBar().getSize().x + 1) / getColumnWidth() ;
		ptyRows =  (height - getTopMargin() - getBottomMargin() - getBorderWidth() * 2 + 1)
						/ lineHeight ;

		pty.resize( ptyCols , ptyRows) ;
	}

	public boolean  updatePty()
	{
		if( pty == null)
		{
			return  false ;
		}
		else if( ! pty.isActive())
		{
			closePty() ;

			return  false ;
		}
		else if( pty.read())
		{
			redrawPty() ;
		}

		return  true ;
	}

	public int  getColumnWidth()
	{
		GC  gc = new GC( this) ;
		int  width = gc.stringExtent( "W").x ;
		gc.dispose() ;

		return  width ;
	}


	/* --- static functions --- */

	private static MLTerm[]  mlterms = new MLTerm[10] ;
	private static int  numMLTerms = 0 ;

	private static void resetWindowSize( Shell  shell , MLTerm  mlterm)
	{
		mlterm.resetSize() ;
		Point  p = mlterm.getSize() ;
		shell.setSize( p) ;

		Rectangle  r = shell.getClientArea() ;
		shell.setSize( p.x * 2 - r.width , p.y * 2 - r.height) ;
	}

	private static void startMLTerm( final Shell  shell , final String  host ,
								final String  pass , final int  cols , final int  rows ,
								final String  encoding , final String[]  argv ,
								final Font  font)
	{
		final MLTerm  mlterm = new MLTerm( shell , SWT.BORDER|SWT.V_SCROLL ,
									host , pass , cols , rows , encoding , argv) ;
		mlterm.setFont( font) ;
		mlterm.setListener(
			new  MLTermListener()
			{
				public void  lineHeightChanged()
				{
					resetWindowSize( shell , mlterm) ;
				}

				public void  ptyClosed()
				{
					for( int  count = 0 ; count < numMLTerms ; count++)
					{
						if( mlterms[count] == mlterm)
						{
							mlterms[count] = mlterms[--numMLTerms] ;
							break ;
						}
					}
				}
			}) ;
		mlterm.addVerifyKeyListener(
			new VerifyKeyListener()
			{
				public void verifyKey( VerifyEvent	event)
				{
					if( event.stateMask == 0x40000 /* Control */ &&
					    event.keyCode == 0x100000a /* F1 */ &&
						numMLTerms < mlterms.length)
					{
						Shell s = new Shell( shell.getDisplay()) ;
						s.setText( "mlterm") ;
						s.setLayout( new FillLayout()) ;

						startMLTerm( s , host , pass , cols , rows ,
									encoding , argv , font) ;
					}
				}
			}) ;

		resetWindowSize( shell , mlterm) ;

		shell.addListener( SWT.Resize ,
			new Listener()
			{
				private boolean  processing = false ;

				public void handleEvent( Event  e)
				{
					if( ! processing)
					{
						Rectangle  rect = shell.getClientArea() ;
						mlterm.resizePty( rect.width , rect.height) ;

						processing = true ;
						resetWindowSize( shell , mlterm) ;
						processing = false ;
					}
				}
			}) ;

		shell.open() ;

		mlterms[numMLTerms++] = mlterm ;
	}

	public static void main (String [] args)
	{
		String  host = null ;
		String  pass = null ;
		String  encoding = null ;
		String[]  argv = null ;
		boolean  openDialog = false ;
		int  cols = 80 ;
		int  rows = 24 ;
		int  fontSize = 10 ;
		String  fontFamily ;

		if( System.getProperty( "os.name").indexOf( "Windows") >= 0)
		{
			fontFamily = "Terminal" ;
		}
		else
		{
			fontFamily = "monospace" ;
		}

		for( int  count = 0 ; count < args.length ; count ++)
		{
			if( args[count].equals( "-h"))
			{
				System.out.println( "usage: java -jar mlterm.jar [options] -e ...") ;
				System.out.println( "options: ") ;
				System.out.println( " -dialog       : show dialog to specify a server.") ;
				System.out.println( " -fn <font>    : specify font family.") ;
				System.out.println( " -g <col>x<row>: specify size.") ;
				System.out.println( " -km <encoding>: specify character encoding.") ;
				System.out.println( " -w <point>    : specify font size.") ;
				System.out.println( " -serv proto://user@host: specify a server.") ;

				return ;
			}
			else if( args[count].equals( "-dialog"))
			{
				openDialog = true ;
			}
			else if( count + 1 < args.length)
			{
				if( args[count].equals( "-serv"))
				{
					host = args[++count] ;
					openDialog = true ;
				}
				else if( args[count].equals( "-g"))
				{
					String[]  geom = args[++count].split( "x") ;
					if( geom.length == 2)
					{
						try
						{
							int  c = Integer.parseInt( geom[0]) ;
							int  r = Integer.parseInt( geom[1]) ;

							cols = c ;
							rows = r ;
						}
						catch( NumberFormatException  e)
						{
						}
					}
				}
				else if( args[count].equals( "-km"))
				{
					encoding = args[++count] ;
				}
				else if( args[count].equals( "-w"))
				{
					try
					{
						fontSize = Integer.parseInt( args[++count]) ;
					}
					catch( NumberFormatException  e)
					{
					}
				}
				else if( args[count].equals( "-fn"))
				{
					fontFamily = args[++count] ;
				}
				else if( args[count].equals( "-e"))
				{
					argv = new String[args.length - (++count)] ;
					for( int  count2 = 0 ; count2 < argv.length ; count2++)
					{
						argv[count2] = args[count + count2] ;
					}
					break ;
				}
			}
		}

		Display display = new Display() ;

		Shell shell = new Shell(display) ;
		shell.setText( "mlterm") ;
		shell.setLayout( new FillLayout()) ;

		if( openDialog)
		{
			ConnectDialog  dialog = new ConnectDialog( shell) ;
			String[]  array = dialog.open( host) ;
			if( array == null)
			{
				shell.dispose() ;

				return ;
			}

			host = array[0] ;
			pass = array[1] ;
			if( array[2] != null)
			{
				encoding = array[2] ;
			}
		}

		startMLTerm( shell , host , pass , cols , rows , encoding ,
					argv , new Font( display , fontFamily , fontSize , SWT.NORMAL)) ;

		while( ! display.isDisposed() && numMLTerms > 0)
		{
			while( display.readAndDispatch()) ;

			for( int  count = 0 ; count < numMLTerms ; count++)
			{
				if( ! mlterms[count].updatePty())
				{
					mlterms[count].getParent().dispose() ;
				}
			}

			if( ! display.readAndDispatch())
			{
				display.sleep() ;
			}
		}

		display.dispose() ;
	}
}
