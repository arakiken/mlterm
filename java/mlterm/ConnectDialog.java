/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

package mlterm;

import org.eclipse.swt.*;
import org.eclipse.swt.widgets.*;
import org.eclipse.swt.layout.*;
import org.eclipse.swt.events.*;

public class ConnectDialog extends Dialog {
  private String proto = "ssh";
  private boolean okPressed = false;
  private boolean cancelPressed = false;

  public ConnectDialog(Shell parent, int style) {
    super(parent, style);
  }

  public ConnectDialog(Shell parent) {
    this(parent, 0);
  }

  /*
   * Return value:
   *  String[0] -> host
   *  String[1] -> password
   *  String[2] -> encoding (can be null)
   *  String[3] -> exec cmd (can be null)
   */
  public String[] open(String uri) {
    Shell shell = new Shell(getParent(), SWT.DIALOG_TRIM | SWT.APPLICATION_MODAL);
    shell.setText(getText());
    shell.setLayout(new GridLayout(2, false));

    if (uri == null) {
      uri = "";
    } else {
      String[] array = uri.split("://");
      if (array.length == 2) {
        proto = array[0];
        uri = array[1];
      }
    }

    /* Protocol */
    Label label = new Label(shell, SWT.NONE);
    label.setText("Protocol");

    Composite comp = new Composite(shell, SWT.NONE);
    RowLayout rowLayout = new RowLayout();
    rowLayout.fill = true;
    rowLayout.center = true;
    rowLayout.justify = true;
    comp.setLayout(rowLayout);

    Button ssh = new Button(comp, SWT.RADIO);
    ssh.setText("SSH");
    if (proto.equals("ssh")) {
      ssh.setSelection(true);
    }
    ssh.addSelectionListener(new SelectionAdapter() {
      public void widgetSelected(SelectionEvent e) {
        proto = "ssh";
      }
    });

    Button telnet = new Button(comp, SWT.RADIO);
    telnet.setText("TELNET");
    if (proto.equals("telnet")) {
      telnet.setSelection(true);
    }
    telnet.addSelectionListener(new SelectionAdapter() {
      public void widgetSelected(SelectionEvent e) {
        proto = "telnet";
      }
    });

    Button rlogin = new Button(comp, SWT.RADIO);
    rlogin.setText("RLOGIN");
    if (proto.equals("rlogin")) {
      rlogin.setSelection(true);
    }
    rlogin.addSelectionListener(new SelectionAdapter() {
      public void widgetSelected(SelectionEvent e) {
        proto = "rlogin";
      }
    });

    comp.pack();

    /* Server */
    label = new Label(shell, SWT.NONE);
    label.setText("Server");
    Text server = new Text(shell, SWT.BORDER);
    GridData textGrid = new GridData(GridData.FILL_HORIZONTAL);
    server.setLayoutData(textGrid);

    /* Port */
    label = new Label(shell, SWT.NONE);
    label.setText("Port");
    Text port = new Text(shell, SWT.BORDER);
    port.setLayoutData(textGrid);

    /* User */
    label = new Label(shell, SWT.NONE);
    label.setText("User");
    Text user = new Text(shell, SWT.BORDER);
    user.setLayoutData(textGrid);

    /* Password */
    label = new Label(shell, SWT.NONE);
    label.setText("Password");
    Text pass = new Text(shell, SWT.BORDER | SWT.PASSWORD);
    pass.setLayoutData(textGrid);

    /* Encoding */
    label = new Label(shell, SWT.NONE);
    label.setText("Encoding");
    Text encoding = new Text(shell, SWT.BORDER);
    encoding.setLayoutData(textGrid);

    /* Exec cmd */
    label = new Label(shell, SWT.NONE);
    label.setText("Exec cmd");
    Text execCmd = new Text(shell, SWT.BORDER);
    execCmd.setLayoutData(textGrid);

    /* OK/Cancel */
    comp = new Composite(shell, SWT.NONE);
    comp.setLayoutData(new GridData(SWT.FILL, SWT.FILL, true, true, 2, 1));
    comp.setLayout(rowLayout);

    Button ok = new Button(comp, SWT.PUSH);
    ok.setText("  OK  ");
    ok.addSelectionListener(new SelectionAdapter() {
      public void widgetSelected(SelectionEvent e) {
        okPressed = true;
      }
    });

    Button cancel = new Button(comp, SWT.PUSH);
    cancel.setText("Cancel");
    cancel.addSelectionListener(new SelectionAdapter() {
      public void widgetSelected(SelectionEvent e) {
        cancelPressed = true;
      }
    });

    comp.pack();

    String[] array = uri.split("@");
    if (array.length == 2) {
      user.setText(array[0]);
      uri = array[1];
    }

    array = uri.split(":");
    server.setText(array[0]);
    if (array.length == 2) {
      boolean isPort = true;
      try {
        Integer.parseInt(array[1]);
      } catch (NumberFormatException e) {
        isPort = false;
      }

      if (isPort) {
        port.setText(array[1]);
      } else {
        encoding.setText(array[1]);
      }
    } else if (array.length == 3) {
      port.setText(array[1]);
      encoding.setText(array[2]);
    }

    final Control[] tabList = new Control[] {
        ssh, telnet, rlogin, server, port, user, pass, encoding, execCmd, ok, cancel};

                KeyAdapter  keyAdapter =
			new KeyAdapter()
			{
				public void keyPressed( KeyEvent  e)
				{
					if( e.keyCode == SWT.TAB)
					{
						/* Tab list doesn't work in applet, so implement by myself. */

						for( int  count = 0 ;
                count < tabList.length; count++)
						{
                  if (e.widget == tabList[count]) {
                    if ((e.stateMask & SWT.SHIFT) != 0) {
                      if (count < 3) {
                        count = tabList.length - 1;
                      } else {
                        count--;

                        if (count < 3) {
                          for (; count >= 0; count--) {
                            if (((Button) tabList[count]).getSelection()) {
                              break;
                            }
                          }
                        }
                      }
                    } else {
                      if (count + 1 == tabList.length) {
                        for (count = 0; count < 3; count++) {
                          if (((Button) tabList[count]).getSelection()) {
                            break;
                          }
                        }
                      } else {
                        if (count < 3) {
                          count = 3;
                        } else {
                          count++;
                        }
                      }
                    }

                    tabList[count].forceFocus();

                    break;
                  }
                }
  }
  else if (e.keyCode == SWT.CR) {
    okPressed = true;
  }
}
}
;

for (int count = 0; count < tabList.length; count++) {
  tabList[count].addKeyListener(keyAdapter);
}

shell.pack();
shell.open();
Display display = shell.getDisplay();
array = null;
while (!shell.isDisposed()) {
  if (okPressed) {
    uri = server.getText();
    if (!uri.equals("")) {
      array = new String[4];

      String str = user.getText();
      if (!str.equals("")) {
        uri = str + "@" + uri;
      }

      uri = proto + "://" + uri;

      str = port.getText();
      if (!str.equals("")) {
        uri = uri + ":" + str;
      }

      array[0] = uri;
      array[1] = pass.getText();
      array[2] = encoding.getText();
      if (array[2].equals("")) {
        array[2] = null;
      }
      array[3] = execCmd.getText();
      if (array[3].equals("")) {
        array[3] = null;
      }
    }
  } else if (!cancelPressed) {
    if (!display.readAndDispatch()) {
      display.sleep();
    }

    continue;
  }

  shell.dispose();

  break;
}

return array;
}
}
