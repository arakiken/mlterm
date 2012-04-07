/*
 *	$Id$
 */

package  mlterm ;


import  java.util.Properties ;
import  java.io.* ;
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
	/* --- private --- */

	final private static boolean  DEBUG = false ;

	private MLTermListener  listener = null ;
	private MLTermPty  pty = null ;
	private int  ptyCols = 80 ;
	private int  ptyRows = 24 ;
	private boolean  isSelecting = false ;
	private Clipboard  clipboard = null ;	/* StyledText.clipboard is not accessible. */
	private int  lineHeight = 0 ;
	private int  columnWidth = 0 ;
	private int  numOfScrolledOutLines = 0 ;
	private int  scrolledOutCache = 0 ;

	static private Color[]  colors = null ;
	static private RedrawRegion  region = null ;
	static private Font  font = null ;

	private static MLTermPty[]  pooledPtys = new MLTermPty[32] ;
	private static int  numOfPooledPtys = 0 ;

	private boolean  pushPty( MLTermPty  p)
	{
		if( numOfPooledPtys == pooledPtys.length)
		{
			return  false ;
		}

		p.setListener( null) ;
		pooledPtys[numOfPooledPtys] = p ;
		numOfPooledPtys ++ ;

		return  true ;
	}

	private MLTermPty  popPty()
	{
		if( numOfPooledPtys == 0)
		{
			return  null ;
		}

		numOfPooledPtys -- ;
		MLTermPty  p = pooledPtys[numOfPooledPtys] ;
		pooledPtys[numOfPooledPtys] = null ;

		return  p ;
	}

	private MLTermPty  getNextPty()
	{
		if( numOfPooledPtys == 0)
		{
			return  null ;
		}

		/* 012 3 => 123 0 => 230 1 */

		MLTermPty  p = pooledPtys[0] ;
		if( numOfPooledPtys > 1)
		{
			System.arraycopy( pooledPtys , 1 , pooledPtys , 0 , numOfPooledPtys - 1) ;
		}
		numOfPooledPtys -- ;

		return  p ;
	}


	private void moveCaret()
	{
		int  row = pty.getCaretRow() + numOfScrolledOutLines ;
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

		if( ! isSelecting)
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

	private int  getColumnWidth()
	{
		GC  gc = new GC( this) ;
		int  width = gc.stringExtent( "W").x ;
		gc.dispose() ;

		return  width ;
	}

	private void checkCellSize( boolean  invokeEvent)
	{
		int  height = getLineHeight() ;
		int  width = getColumnWidth() ;

		if( lineHeight == height && columnWidth == width)
		{
			return ;
		}
		else if( DEBUG)
		{
			System.err.printf( "Line height %d is changed to %d\n" , lineHeight , height) ;
			System.err.printf( "Column width %d is changed to %d\n" , columnWidth , width) ;
		}

		lineHeight = height ;
		columnWidth = width ;

		if( invokeEvent && listener != null)
		{
			listener.sizeChanged() ;
		}
	}

	private void  resetScrollBar()
	{
		if( numOfScrolledOutLines > 0)
		{
			setTopIndex( numOfScrolledOutLines) ;
		}
	}

	private void  checkTextLimit( int  num)
	{
		if( getTextLimit() != -1 && getTextLimit() < getCharCount() + num)
		{
			/* 500 is for buffer. */
			getContent().replaceTextRange( 0 , getCharCount() + num + 500 - getTextLimit() , "") ;

			if( numOfScrolledOutLines > getLineCount() - ptyRows)
			{
				numOfScrolledOutLines = getLineCount() - ptyRows ;
			}
		}
	}

	private int  bufOffset = 0 ;
	private StringBuilder  bufStr = null ;
	private int  bufReplaceLen = 0 ;
	private Object[]  bufStylesArray = null ;
	private int  numOfBufStylesArray = 0 ;
	private int  numOfBufStyles = 0 ;
	private void  replaceTextBuffering( int  offset , int  replaceLen ,
					String  str , StyleRange[]  styles)
	{
		if( DEBUG)
		{
			/* Simple way (output each line) */

			if( str != null)
			{
				getContent().replaceTextRange( offset , replaceLen , str) ;
			}

			if( styles != null)
			{
				replaceStyleRanges( offset , str.length() , styles) ;
			}
		}
		else
		{
			if( styles != null && bufStr != null)
			{
				for( int  count = 0 ; count < styles.length ; count++)
				{
					styles[count].start += (bufStr.length() - bufReplaceLen) ;
				}
			}

			if( ( str == null || bufOffset + bufReplaceLen != offset) && bufStr != null)
			{
				getContent().replaceTextRange( bufOffset , bufReplaceLen , bufStr.toString()) ;

				if( DEBUG)
				{
					System.err.printf( "OUTPUT %d characters.%n" , bufStr.length()) ;
				}

				if( numOfBufStylesArray > 0)
				{
					StyleRange[]  bufStyles = new StyleRange[numOfBufStyles] ;
					int  destPos = 0 ;
					for( int  count = 0 ; count < numOfBufStylesArray ; count++)
					{
						System.arraycopy( (StyleRange[])bufStylesArray[count] , 0 ,
								bufStyles , destPos ,
								((StyleRange[])bufStylesArray[count]).length) ;
						destPos += ((StyleRange[])bufStylesArray[count]).length ;
					}

					if( destPos != numOfBufStyles)
					{
						System.err.printf( "Illegal styles, not applied.\n") ;
					}
					else
					{
						replaceStyleRanges( bufOffset , bufStr.length() , bufStyles) ;
					}

					numOfBufStylesArray = 0 ;
					numOfBufStyles = 0 ;
				}

				if( str == null)
				{
					bufStr = null ;
					bufOffset = 0 ;
					bufReplaceLen = 0 ;
				}
				else
				{
					bufOffset = offset + bufStr.length() - bufReplaceLen ;
					bufStr = new StringBuilder( str) ;
					bufReplaceLen = replaceLen ;
				}
			}
			else if( str != null)
			{
				if( bufStr == null)
				{
					bufStr = new StringBuilder( str) ;
					bufOffset = offset ;
				}
				else
				{
					bufStr.append( str) ;
				}

				bufReplaceLen += replaceLen ;
			}

			if( styles != null)
			{
				if( bufStylesArray == null || bufStylesArray.length < ptyRows)
				{
					bufStylesArray = new Object[ptyRows] ;
				}

				bufStylesArray[numOfBufStylesArray] = styles ;
				numOfBufStylesArray ++ ;
				numOfBufStyles += styles.length ;
			}
		}
	}

	private void  redrawPty()
	{
		if( scrolledOutCache > 0)
		{
			numOfScrolledOutLines += scrolledOutCache ;
			setRedraw( false) ;		/* Stop moving scrollbar */
		}

		for( int  row = 0 ; row < ptyRows ; row ++)
		{
			if( ! pty.getRedrawString( row , region))
			{
				continue ;
			}

			if( region.start > 0)
			{
				replaceTextBuffering( 0 , 0 , null , null) ;
			}

			if( row == ptyRows - 1)
			{
				/* Remove '\n' which region.str always contains. */
				region.str = region.str.substring( 0 , region.str.length() - 1) ;
			}

			/* If lineCount is 0, max num of '\n' appended right below is row. */
			checkTextLimit( region.str.length() + row) ;

			int  lineCount = getLineCount() - numOfScrolledOutLines ;
			if( lineCount <= row)
			{
				replaceTextBuffering( 0 , 0 , null , null) ;

				do
				{
					append( String.valueOf( '\n')) ;
					lineCount++ ;
				}
				while( lineCount <= row) ;
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

			int  offset = getOffsetAtLine( row + numOfScrolledOutLines) + region.start ;

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
				offsetNextRow = getOffsetAtLine( row + numOfScrolledOutLines + 1) ;
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

				if( DEBUG)
				{
					System.err.printf( "%s row %d lineCount %d offset %d offsetNextRow %d%n" ,
						region.str , row , lineCount , offset , offsetNextRow) ;
				}
			}

			StyleRange[]  styles = null ;
			if( region.styles != null)
			{
				styles = new StyleRange[region.styles.length] ;

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
			}

			replaceTextBuffering( offset , offsetNextRow - offset , region.str , styles) ;
		}

		replaceTextBuffering( 0 , 0 , null , null) ;

		checkCellSize(true) ;

		if( scrolledOutCache > 0)
		{
			scrolledOutCache = 0 ;
			resetScrollBar() ;
			setRedraw( true) ;
		}
	}

	private void  reportMouseTracking( MouseEvent  event ,
						boolean  isMotion , boolean  isReleased)
	{
		if( ( event.stateMask & (SWT.CONTROL|SWT.SHIFT)) != 0)
		{
			return ;
		}

		int  row ;
		int  charIndex ;
		if( isMotion || event.button > 3 /* isWheel */)
		{
			row = event.y / lineHeight ;
			charIndex = event.x / columnWidth ;
		}
		else
		{
			row = getLineAtOffset( getCaretOffset()) ;
			charIndex = getLine( row).codePointCount( 0 ,
										getCaretOffset() - getOffsetAtLine( row)) ;
		}

		pty.reportMouseTracking( charIndex , row - numOfScrolledOutLines ,
						event.button , event.stateMask , isMotion , isReleased) ;
	}

	private void attachPty( boolean  createContent)
	{
		ptyCols = pty.getCols() ;
		ptyRows = pty.getRows() ;

		StyledTextContent  content = (StyledTextContent)pty.getAuxData() ;
		if( content == null)
		{
			if( createContent)
			{
				/* New content */
				StyledText  text = new StyledText( MLTerm.this , 0) ;
				setContent( text.getContent()) ;
				text.dispose() ;
			}

			numOfScrolledOutLines = 0 ;
		}
		else
		{
			setContent( content) ;

			int  rows = content.getLineCount() ;
			if( rows > ptyRows)
			{
				numOfScrolledOutLines = rows - ptyRows ;
			}
			else
			{
				numOfScrolledOutLines = 0 ;
			}
		}

		/*
		 * XXX
		 * If scrolledOutCache > 0 , redrawPty() should be called, but
		 * such a case rarely seems to happen.
		 */
		scrolledOutCache = 0 ;

		pty.setListener(
			new MLTermPtyListener()
			{
				public void  executeCommand( String  cmd)
				{
					String[]  argv = cmd.split( " ") ;
					if( argv.length == 2 && argv[0].equals( "browser"))
					{
						startBrowser( MLTerm.this , argv[1]) ;
					}
				}

				public void  linesScrolledOut( int  size)
				{
					scrolledOutCache += size ;

					if( scrolledOutCache == ptyRows * 2 / 5)
					{
						redrawPty() ;

						Display  display = getDisplay() ;
						while( display.readAndDispatch()) ;
					}
				}

				public void  resize( int  width , int  height , int  cols , int  rows)
				{
					if( width > 0 && height > 0)
					{
						resizePty( width , height) ;
					}
					else if( cols > 0 && rows > 0)
					{
						ptyCols = cols ;
						ptyRows = rows ;
					}
					else
					{
						return ;
					}

					listener.sizeChanged() ;
				}

				public void  bell()
				{
					String  mode = getProperty( "bel_mode") ;
					if( mode == null || mode.equals( "sound"))
					{
						getDisplay().beep() ;
					}
				}
			}) ;

		if( content != null || createContent)
		{
			/* Content was changed */
			listener.sizeChanged() ;
		}
	}

	private void init( final String  host , final String  pass , final int  cols ,
					final int  rows , final String  encoding , final String[]  argv)
	{
		pty = new MLTermPty() ;

		if( ! pty.open( host , pass , cols , rows , encoding , argv))
		{
			pty = null ;

			return ;
		}

		attachPty( false) ;

		if( font == null)
		{
			String  fontFamily = getProperty( "font") ;
			if( fontFamily == null)
			{
				if( System.getProperty( "os.name").indexOf( "Windows") >= 0)
				{
					fontFamily = "Terminal" ;
				}
				else
				{
					fontFamily = "monospace" ;
				}
			}

			int  fontSize = 10 ;
			try
			{
				fontSize = Integer.parseInt( getProperty( "fontsize")) ;
			}
			catch( NumberFormatException  e)
			{
			}

			font = new Font( getDisplay() , fontFamily , fontSize , SWT.NORMAL) ;
		}

		setFont( font) ;
		checkCellSize( false) ;

		if( colors == null)
		{
			colors = new Color[258] ;

			String  color = getProperty( "fg_color") ;
			if( color != null)
			{
				long  pixel = MLTermPty.getColorRGB( color) ;
				if( pixel != -1)
				{
					colors[0x100] = new Color( getDisplay() , (int)((pixel >> 16) & 0xff) ,
										(int)((pixel >> 8) & 0xff) , (int)(pixel & 0xff)) ;
					setForeground( colors[0x100]) ;
				}
			}

			if( colors[0x100] == null)
			{
				colors[0x100] = getForeground() ;
			}

			color = getProperty( "bg_color") ;
			if( color != null)
			{
				long  pixel = MLTermPty.getColorRGB( color) ;
				if( pixel != -1)
				{
					colors[0x101] = new Color( getDisplay() , (int)((pixel >> 16) & 0xff) ,
										(int)((pixel >> 8)) & 0xff , (int)(pixel & 0xff)) ;
					setBackground( colors[0x101]) ;
				}
			}

			if( colors[0x101] == null)
			{
				colors[0x101] = getBackground() ;
			}


			/* font and color objects will be disposed when display is disposed. */
			getDisplay().addListener( SWT.Dispose ,
				new Listener()
				{
					public void handleEvent( Event  event)
					{
						font.dispose() ;
						font = null ;

						for( int  count = 0 ; count < colors.length ; count++)
						{
							if( colors[count] != null)
							{
								colors[count].dispose() ;
								colors[count] = null ;
							}
						}
						colors = null ;
					}
				}) ;
		}

		setTextLimit( 100000) ;
		setMargins( 1 , 1 , 1 , 1) ;

		clipboard = new Clipboard( getDisplay()) ;

		addListener( SWT.Dispose ,
			new Listener()
			{
				public void handleEvent( Event  event)
				{
					if( pty != null)
					{
						if( pushPty( pty))
						{
							pty.setAuxData( getContent()) ;
						}
						else
						{
							closePty() ;
						}
					}
				}
			}) ;

		addMouseListener(
			new  MouseListener()
			{
				public void mouseDown( MouseEvent  event)
				{
					reportMouseTracking( event , false , false) ;

					isSelecting = true ;
				}

				public void mouseUp( MouseEvent  event)
				{
					isSelecting = false ;

					if( getSelectionCount() > 0)
					{
						copy() ;
					}

					reportMouseTracking( event , false , true) ;
				}

				public void mouseDoubleClick( MouseEvent  event)
				{
				}
			}) ;

		addMouseMoveListener(
			new  MouseMoveListener()
			{
				public void mouseMove( MouseEvent  event)
				{
					reportMouseTracking( event , true , false) ;
				}
			}) ;

		addMouseWheelListener(
			new  MouseWheelListener()
			{
				public void mouseScrolled( MouseEvent  event)
				{
					if( event.count > 0)
					{
						event.button = 4 ;
					}
					else
					{
						event.button = 5 ;
					}

					reportMouseTracking( event , false , false) ;
				}
			}) ;

		addTraverseListener(
			new TraverseListener()
			{
				public void keyTraversed( TraverseEvent  event)
				{
					if( false)
					{
						System.out.printf( "MASK %x KCODE %x\n" ,
							event.stateMask , event.keyCode) ;
					}

					if( event.stateMask == SWT.SHIFT && event.keyCode == SWT.TAB)
					{
						pty.write( "\u001b[Z") ;
					}
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
					else if( event.stateMask == SWT.CONTROL && event.keyCode == SWT.F2)
					{
						if( pushPty( pty))
						{
							pty.setAuxData( getContent()) ;

							pty = new MLTermPty() ;

							if( pty.open( host , pass , cols , rows , encoding , argv))
							{
								attachPty( true) ;
							}
							else
							{
								/* restore */
								pty = popPty() ;
							}
						}

						return ;
					}
					else if( event.stateMask == SWT.CONTROL && event.keyCode == SWT.F3)
					{
						MLTermPty  nextPty = getNextPty() ;
						if( nextPty != null)
						{
							pushPty( pty) ;
							pty.setAuxData( getContent()) ;

							pty = nextPty ;
							attachPty( false) ;
						}

						return ;
					}
					else if( false && event.stateMask == SWT.SHIFT && event.keyCode == SWT.TAB)
					{
						/* XXX Shift+tab event is received only at TraverseListener. */
						return ;
					}
					else if( event.stateMask == SWT.CONTROL && event.keyCode == ' ')
					{
						/* Control + space is not converted to 0x00 automatically. */
						str = "" ;
					}
					else if( event.stateMask == SWT.SHIFT && event.keyCode == SWT.INSERT)
					{
						str = (String) clipboard.getContents(
										TextTransfer.getInstance() , DND.CLIPBOARD) ;
					}
					else if( event.keyCode == SWT.ARROW_UP)
					{
						if( pty.isAppCursorKeys())
						{
							str = "\u001bOA" ;
						}
						else
						{
							str = "\u001b[A" ;
						}
					}
					else if( event.keyCode == SWT.ARROW_DOWN)
					{
						if( pty.isAppCursorKeys())
						{
							str = "\u001bOB" ;
						}
						else
						{
							str = "\u001b[B" ;
						}
					}
					else if( event.keyCode == SWT.ARROW_LEFT)
					{
						if( pty.isAppCursorKeys())
						{
							str = "\u001bOD" ;
						}
						else
						{
							str = "\u001b[D" ;
						}
					}
					else if( event.keyCode == SWT.ARROW_RIGHT)
					{
						if( pty.isAppCursorKeys())
						{
							str = "\u001bOC" ;
						}
						else
						{
							str = "\u001b[C" ;
						}
					}
					else if( event.keyCode == SWT.PAGE_UP)
					{
						str = "\u001b[5~" ;
					}
					else if( event.keyCode == SWT.PAGE_DOWN)
					{
						str = "\u001b[6~" ;
					}
					else if( event.keyCode == SWT.HOME)
					{
						str = "\u001b[H" ;
					}
					else if( event.keyCode == SWT.END)
					{
						str = "\u001b[F" ;
					}
					else
					{
						str = String.valueOf( event.character) ;
					}

					if( str != null)
					{
						if( event.stateMask == SWT.ALT)
						{
							pty.write( "\u001b") ;
						}

						pty.write( str) ;
					}
					event.doit = false ;
				}
			}) ;

		setCaret( null) ;

		DropTarget  target = new DropTarget( this , DND.DROP_MOVE|DND.DROP_COPY|DND.DROP_LINK) ;
		target.setTransfer( new Transfer[] { FileTransfer.getInstance() }) ;
		target.addDropListener(
			new DropTargetAdapter()
			{
				public void  dragEnter( DropTargetEvent event)
				{
					if( event.detail == DND.DROP_NONE)
					{
						event.detail = DND.DROP_LINK ;
					}
				}

				public void  dragOperationChanged( DropTargetEvent event)
				{
					if( event.detail == DND.DROP_NONE)
					{
						event.detail = DND.DROP_LINK ;
					}
				}

				public void  drop( DropTargetEvent event)
				{
					if( event.data == null)
					{
						event.detail = DND.DROP_NONE ;

						return ;
					}

					String[]  files = (String[])event.data ;
					for( int  count = 0 ; count < files.length ; count++)
					{
						if( count > 0)
						{
							pty.write( "\n") ;
						}
						pty.write( files[count]) ;
					}
				}
			}) ;

		region = new RedrawRegion() ;
	}


	/* --- public --- */

	public MLTerm( Composite  parent , int  style , String  host , String  pass ,
				int  cols , int  rows , String  encoding , String[]  argv)
	{
		/* If SWT.READ_ONLY is specified, tab key doesn't work. */
		super( parent , style|SWT.NO_BACKGROUND) ;

		init( host , pass , cols , rows , encoding , argv) ;
	}

	public static void  startPtyWatcher( final Display  display)
	{
		(new Thread(
			new  Runnable()
			{
				public void run()
				{
					while( true)
					{
						synchronized(display)
						{
							try
							{
								/* blocking until display.notifyAll() is called. */
								display.wait() ;
							}
							catch( InterruptedException  e)
							{
							}
						}

						if( ! MLTermPty.waitForReading() /* block until pty is ready to be read. */
                            || display.isDisposed())
						{
							break ;
						}

						display.wake() ;
					}
				}
			})).start() ;
	}

	public boolean  isActive()
	{
		if( pty == null)
		{
			return  false ;
		}
		else if( ! pty.isActive())
		{
			MLTermPty  nextPty = popPty() ;
			if( nextPty != null)
			{
				pty.close() ;
				pty = nextPty ;
				attachPty( false) ;

				return  true ;
			}
			else
			{
				closePty() ;

				return  false ;
			}
		}
		else
		{
			return  true ;
		}
	}

	public void  setListener( MLTermListener  lsn)
	{
		listener = lsn ;
	}

	public void  resetSize()
	{
		int  width = columnWidth * ptyCols + getLeftMargin() +
						getRightMargin() + getBorderWidth() * 2 +
						getVerticalBar().getSize().x ;
		int  height = lineHeight * ptyRows + getTopMargin() +
						getBottomMargin() + getBorderWidth() * 2 ;

		setSize( width , height) ;
		resetScrollBar() ;
	}

	public void  resizePty( int  width , int  height)
	{
		int  cols =  (width - getLeftMargin() - getRightMargin() - getBorderWidth() * 2 -
						getVerticalBar().getSize().x) / columnWidth ;
		int  rows =  (height - getTopMargin() - getBottomMargin() - getBorderWidth() * 2)
						/ lineHeight ;

		if( cols == ptyCols && rows == ptyRows)
		{
			return ;
		}

		ptyCols = cols ;
		ptyRows = rows ;

		int  start = getOffsetAtLine( numOfScrolledOutLines) ;
		int  length = getCharCount() - start ;
		StringBuilder  str = new StringBuilder() ;
		for( int  count = 0 ; count < ptyRows - 1 ; count++)
		{
			str.append( "\n") ;
		}

		replaceTextRange( start , length , str.toString()) ;

		pty.resize( ptyCols , ptyRows) ;
	}

	public boolean  updatePty()
	{
		if( ! isActive())
		{
			return  false ;
		}
		else if( pty.read())
		{
			redrawPty() ;
			moveCaret() ;
		}

		return  true ;
	}

	private static  Properties  prop = null ;
	private static void  loadProperties()
	{
		prop = new Properties() ;

		try
		{
			prop.load( new FileInputStream( MLTermPty.getConfigDirectory() + "main")) ;
		}
		catch( IOException  e)
		{
			System.err.println( "mlterm/main is not found") ;
		}
	}

	public static String  getProperty( String  key)
	{
		if( prop == null)
		{
			loadProperties() ;
		}

		return  prop.getProperty( key) ;
	}

	public static void  setProperty( String  key , String  value)
	{
		if( prop == null)
		{
			loadProperties() ;
		}

		prop.setProperty( key , value) ;
	}


	/* --- static methods --- */

	private static MLTerm[]  mlterms = new MLTerm[32] ;
	private static int  numOfMLTerms = 0 ;
	private static Point  decoration = null ;

	private static void  resetWindowSize( MLTerm  mlterm)
	{

		mlterm.resetSize() ;
		Point  p = mlterm.getSize() ;

		Shell  shell = mlterm.getShell() ;

		if( decoration == null)
		{
			shell.setSize( p) ;
			Rectangle  r = shell.getClientArea() ;

			decoration = new Point( p.x - r.width , p.y - r.height) ;
		}

		shell.setSize( p.x + decoration.x , p.y + decoration.y) ;
	}

	private static void startMLTerm( final Shell  shell , final String  host ,
								final String  pass , final int  cols , final int  rows ,
								final String  encoding , final String[]  argv)
	{
		final MLTerm  mlterm = new MLTerm( shell , SWT.BORDER|SWT.V_SCROLL ,
									host , pass , cols , rows , encoding , argv) ;
		if( ! mlterm.isActive())
		{
			MessageBox  box = new MessageBox( shell , SWT.OK) ;
			box.setMessage( "Failed to open pty.") ;
			box.open() ;

			return ;
		}

		mlterm.setListener(
			new MLTermListener()
			{
				public void  sizeChanged()
				{
					resetWindowSize( mlterm) ;
				}

				public void  ptyClosed()
				{
					for( int  count = 0 ; count < numOfMLTerms ; count++)
					{
						if( mlterms[count] == mlterm)
						{
							mlterms[count] = mlterms[--numOfMLTerms] ;
							break ;
						}
					}
				}
			}) ;

		shell.addListener( SWT.Dispose ,
			new Listener()
			{
				public void handleEvent( Event  e)
				{
					for( int  count = 0 ; count < numOfMLTerms ; count++)
					{
						if( mlterms[count] == mlterm)
						{
							mlterms[count] = mlterms[--numOfMLTerms] ;
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
					if( event.stateMask == SWT.CONTROL &&
					    event.keyCode == SWT.F1 &&
						numOfMLTerms < mlterms.length)
					{
						Shell s = new Shell( shell.getDisplay()) ;
						s.setText( "mlterm") ;
						s.setLayout( new FillLayout()) ;

						startMLTerm( s , host , pass , cols , rows , encoding , argv) ;
					}
				}
			}) ;

		resetWindowSize( mlterm) ;

		shell.addListener( SWT.Resize ,
			new Listener()
			{
				private boolean  processing = false ;
				private int  prevWidth = 0 ;
				private int  prevHeight = 0 ;

				public void handleEvent( Event  e)
				{
					if( ! processing)
					{
						Rectangle  r = shell.getClientArea() ;
						if( r.width != prevWidth || r.height != prevHeight)
						{
							prevWidth = r.width ;
							prevHeight = r.height ;

							mlterm.resizePty( r.width , r.height) ;

							processing = true ;
							resetWindowSize( mlterm) ;
							processing = false ;
						}
					}
				}
			}) ;

		shell.open() ;

		mlterms[numOfMLTerms++] = mlterm ;
	}

	public static void main (String [] args)
	{
		String  host = MLTerm.getProperty( "default_server") ;
		String  pass = null ;
		String  encoding = null ;
		String[]  argv = null ;
		boolean  openDialog = false ;
		int  cols = 80 ;
		int  rows = 24 ;

		if( System.getProperty( "os.name").indexOf( "Windows") >= 0 || host != null)
		{
			openDialog = true ;
		}

		String  geom = MLTerm.getProperty( "geometry") ;

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
					geom = args[++count] ;
				}
				else if( args[count].equals( "-km"))
				{
					encoding = args[++count] ;
				}
				else if( args[count].equals( "-w"))
				{
					MLTerm.setProperty( "fontsize" , args[++count]) ;
				}
				else if( args[count].equals( "-fn"))
				{
					MLTerm.setProperty( "font" , args[++count]) ;
				}
				else if( args[count].equals( "-fg"))
				{
					MLTerm.setProperty( "fg_color" , args[++count]) ;
				}
				else if( args[count].equals( "-bg"))
				{
					MLTerm.setProperty( "bg_color" , args[++count]) ;
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

		if( geom != null)
		{
			String[]  array = geom.split( "x") ;
			if( array.length == 2)
			{
				try
				{
					int  c = Integer.parseInt( array[0]) ;
					int  r = Integer.parseInt( array[1]) ;

					cols = c ;
					rows = r ;
				}
				catch( NumberFormatException  e)
				{
					System.err.println( geom + " geometry is illegal.") ;
				}
			}
		}

		Display  display = new Display() ;

		Shell  shell = new Shell(display) ;
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
			if( array[3] != null)
			{
				argv = array[3].split( " ") ;
			}
		}

		startMLTerm( shell , host , pass , cols , rows , encoding , argv) ;

		MLTerm.startPtyWatcher( display) ;

		while( ! display.isDisposed() && numOfMLTerms > 0)
		{
			while( display.readAndDispatch()) ;

			for( int  count = 0 ; count < numOfMLTerms ; count++)
			{
				if( ! mlterms[count].updatePty())
				{
					mlterms[count].getShell().dispose() ;
				}
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

		display.dispose() ;

		synchronized(display)
		{
			display.notifyAll() ;
		}
	}
}
