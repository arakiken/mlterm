/*
 *	$Id$
 */

package mlterm;

import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.*;

public class ConfirmDialog {
  public static boolean show(String msg) {
    MessageBox box = new MessageBox(new Shell(Display.getCurrent()), SWT.OK | SWT.CANCEL);
    box.setMessage(msg);

    if (box.open() == SWT.OK) {
      return true;
    } else {
      return false;
    }
  }
}
