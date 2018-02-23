/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

package mlterm;

import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.*;

public class ConfirmDialog {
  public static boolean show(String msg, boolean okcancel) {
    MessageBox box;

    if (okcancel) {
      box = new MessageBox(new Shell(Display.getCurrent()), SWT.OK | SWT.CANCEL);
    } else {
      box = new MessageBox(new Shell(Display.getCurrent()), SWT.OK);
    }
    box.setMessage(msg);

    if (box.open() == SWT.OK) {
      return true;
    } else {
      return false;
    }
  }
}
