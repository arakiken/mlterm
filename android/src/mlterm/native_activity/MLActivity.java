/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

package mlterm.native_activity;

import android.app.NativeActivity;
import android.view.KeyEvent;
import android.view.WindowManager;
import android.view.WindowManager.LayoutParams;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.View;
import android.view.ViewGroup;
import android.view.ContextMenu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.text.InputType;
import android.content.Context;
import android.content.ClipboardManager;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.graphics.Rect;
import android.graphics.BitmapFactory;
import android.graphics.Bitmap;
import android.util.AttributeSet;
import java.net.URL;
import java.io.*;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.LinearLayout;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.widget.Toast;
import android.content.DialogInterface;
import android.app.AlertDialog;
import android.app.Dialog;
import android.graphics.Color;

public class MLActivity extends NativeActivity {
  static {
    System.loadLibrary("mlterm");
  }

  private native void visibleFrameChanged(int yoffset, int width, int height);
  private native void commitTextLock(String str);
  private native void commitTextNoLock(String str);
  private native void preeditText(String str);
  private native String convertToTmpPath(String path);
  private native void splitAnimationGif(String path);
  private native void dialogOkClicked(String user, String serv, String port, String encoding,
                                      String pass, String cmd, String privkey);
  private native void updateScreen();
  private native void execCommand(int cmd);
  private native void resumeNative();
  private native void pauseNative();

  private String keyString;
  private View contentView;
  private int inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE;
  private int imeOptions = EditorInfo.IME_FLAG_NO_ENTER_ACTION | EditorInfo.IME_ACTION_DONE
      | EditorInfo.IME_FLAG_NO_FULLSCREEN;
  private ClipboardManager clipMan;
  private Thread nativeThread;
  private boolean needRedraw = false;

  private class TextInputConnection extends BaseInputConnection {
    public TextInputConnection(View v, boolean fulledit) {
      super(v, fulledit);
    }

    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
      super.commitText(text, newCursorPosition);

      if (nativeThread == null) {
        commitTextLock(text.toString());
      }

      return true;
    }

    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
      super.setComposingText(text, newCursorPosition);

      if (nativeThread == null) {
        preeditText(text.toString());
      }

      return true;
    }

    @Override
    public boolean finishComposingText() {
      super.finishComposingText();

      if (nativeThread == null) {
        preeditText("");
      }

      return true;
    }
  }

  private class NativeContentView extends View {
    public NativeContentView(Context context) {
      super(context);
    }

    public NativeContentView(Context context, AttributeSet attrs) {
      super(context, attrs);
    }

    @Override
    public boolean onCheckIsTextEditor() {
      return true;
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
      outAttrs.inputType = inputType;
      outAttrs.imeOptions = imeOptions;

      return new TextInputConnection(this, true);
    }
  }

  private void forceAsciiInput() {
    if (true) {
      inputType |= InputType.TYPE_TEXT_VARIATION_URI;
    } else {
      inputType |= InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD;
    }

    imeOptions |= 0x80000000; /* EditorInfo.IME_FLAG_FORCE_ASCII ; (API level 16) */

    ((InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE)).restartInput(contentView);
  }

  private void showSoftInput() {
    ((InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE))
        .showSoftInput(contentView, InputMethodManager.SHOW_FORCED);
  }

  private void setTextToClipboard(String str) {
    ClipData.Item item = new ClipData.Item(str);
    String[] mimeType = new String[1];
    mimeType[0] = ClipDescription.MIMETYPE_TEXT_PLAIN;
    ClipData cd = new ClipData(new ClipDescription("text_data", mimeType), item);
    clipMan.setPrimaryClip(cd);
  }

  private void getTextFromClipboard() {
    ClipData cd = clipMan.getPrimaryClip();
    if (cd != null) {
      ClipData.Item item = cd.getItemAt(0);

      commitTextNoLock(item.getText().toString());
    }
  }

  @Override
  public boolean dispatchKeyEvent(KeyEvent event) {
    if (event.getAction() == KeyEvent.ACTION_MULTIPLE) {
      keyString = event.getCharacters();
    }

    return super.dispatchKeyEvent(event);
  }

  @Override
  protected void onCreate(Bundle state) {
    super.onCreate(state);

    getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);

    if (false) {
      /* setContentView() is called in NativeActivity.onCreate(). */
      contentView = ((ViewGroup) findViewById(android.R.id.content)).getChildAt(0);
    } else {
      contentView = new NativeContentView(this);
      setContentView(contentView);
      contentView.getViewTreeObserver().addOnGlobalLayoutListener(this);
    }

    contentView.setFocusable(true);
    contentView.setFocusableInTouchMode(true);
    contentView.requestFocus();

    registerForContextMenu(contentView);

    /* android-11 or later */
    getWindow().getDecorView().addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
      @Override
      public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
          int oldTop, int oldRight, int oldBottom) {
        Rect r = new Rect();
        v.getWindowVisibleDisplayFrame(r);
        if (nativeThread == null) {
          visibleFrameChanged(r.top, r.right, r.bottom - r.top);
        }
      }
    });

    /*
     * It is prohibited to call getSystemService(CLIPBOARD_SERVICE)
     * in native activity thread.
     */
    clipMan = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
  }

  @Override
  protected void onSaveInstanceState(Bundle state) {
    if (nativeThread != null) {
      synchronized(nativeThread) {
        nativeThread.notifyAll(); /* AlertDialog is ignored if home button is pressed. */
      }
    }

    super.onSaveInstanceState(state);
  }

  @Override
  protected void onPause() {
    if (nativeThread != null) {
      synchronized(nativeThread) {
        nativeThread.notifyAll(); /* AlertDialog is ignored if back button is pressed. */
      }
    }

    super.onPause();

    ((InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE))
        .hideSoftInputFromWindow(contentView.getWindowToken(), 0);

    needRedraw = true;

    pauseNative();
  }

  @Override
  protected void onStop() {
    super.onStop();

    needRedraw = false;
  }

  @Override
  protected void onResume() {
    super.onResume();

    if (needRedraw) {
      updateScreen();
      needRedraw = false;
    }

    resumeNative();
  }

  @Override
  protected void onRestart() {
    super.onRestart();

    showSoftInput();
  }

  private void performLongClick() {
    runOnUiThread(new Runnable() {
        @Override public void run() {
          contentView.performLongClick();
        }
      });
  }

  private final int MENU_PASTE_ID = 0;
  private final int MENU_NEWSCREEN_ID = 1;
  private final int MENU_LOCALPTY_ID = 2;
  private final int MENU_SSH_ID = 3;
  private final int MENU_VSPLIT_ID = 4;
  private final int MENU_HSPLIT_ID = 5;
  private final int MENU_NEXTPTY_ID = 6;
  private final int MENU_PREVPTY_ID = 7;
  private final int MENU_UPDATESCREEN_ID = 8;
  private final int MENU_CLOSESCREEN_ID = 9;
  private final int MENU_CONFIG_ID = 10;
  private final int MENU_CANCEL_ID = 11;

  @Override
  public void onCreateContextMenu(ContextMenu menu, View view,
                                     ContextMenu.ContextMenuInfo info) {
    super.onCreateContextMenu(menu, view, info);
    /* menu.setHeaderTitle("Menu"); */
    menu.add(0, MENU_PASTE_ID, 0, "Paste from clipboard");
    menu.add(0, MENU_NEWSCREEN_ID, 0, "Open new screen");
    menu.add(0, MENU_LOCALPTY_ID, 0, "Open local pty");
    menu.add(0, MENU_SSH_ID, 0, "Connect to SSH server");
    menu.add(0, MENU_VSPLIT_ID, 0, "Split screen vertically");
    menu.add(0, MENU_HSPLIT_ID, 0, "Split screen horizontally");
    menu.add(0, MENU_NEXTPTY_ID, 0, "Next pty");
    menu.add(0, MENU_PREVPTY_ID, 0, "Previous pty");
    menu.add(0, MENU_UPDATESCREEN_ID, 0, "Update screen");
    menu.add(0, MENU_CLOSESCREEN_ID, 0, "Close splitted screen");
    menu.add(0, MENU_CONFIG_ID, 0, "Configuration");
    menu.add(0, MENU_CANCEL_ID, 0, "Cancel");
  }

  @Override
  public boolean onContextItemSelected(MenuItem item) {
    switch (item.getItemId()) {
      case MENU_PASTE_ID:
        getTextFromClipboard();
        return true;
      case MENU_NEWSCREEN_ID:
        execCommand(0);
        return true;
      case MENU_LOCALPTY_ID:
        execCommand(3);
        return true;
      case MENU_SSH_ID:
        execCommand(7);
        return true;
      case MENU_VSPLIT_ID:
        execCommand(1);
        return true;
      case MENU_HSPLIT_ID:
        execCommand(2);
        return true;
      case MENU_NEXTPTY_ID:
        execCommand(4);
        return true;
      case MENU_PREVPTY_ID:
        execCommand(5);
        return true;
      case MENU_UPDATESCREEN_ID:
        updateScreen();
        return true;
      case MENU_CLOSESCREEN_ID:
        execCommand(6);
        return true;
      case MENU_CONFIG_ID:
        Intent intent = new Intent(this, MLPreferenceActivity.class);
        startActivity(intent);
        return true;
      default:
        return super.onContextItemSelected(item);
    }
  }

  private String saveDefaultFont() {
    File file = getFileStreamPath("unifont.pcf");
    if (!file.exists() ||
        /* If unifont.pcf is older than apk, rewrite unifont.pcf. */
        (new File(getApplicationInfo().publicSourceDir)).lastModified() > file.lastModified()) {
      try {
        InputStream is = getResources().getAssets().open("unifont.pcf");
        OutputStream os = openFileOutput("unifont.pcf", 0);
        int len;
        byte[] buf = new byte[102400];

        while ((len = is.read(buf)) >= 0) {
          os.write(buf, 0, len);
        }

        os.close();
        is.close();

        System.err.println("unifont.pcf written.\n");
      } catch (Exception e) {
      }
    }

    return file.getPath();
  }

  private int[] getBitmap(String path, int width, int height) {
    try {
      InputStream is;

      if (path.indexOf("://") != -1) {
        URL url = new URL(path);
        is = url.openConnection().getInputStream();

        if (path.indexOf(".gif") != -1) {
          String tmp = convertToTmpPath(path);
          OutputStream os = new FileOutputStream(tmp);

          int len;
          byte[] buf = new byte[10240];

          while ((len = is.read(buf)) >= 0) {
            os.write(buf, 0, len);
          }

          os.close();
          is.close();

          splitAnimationGif(path);
          is = new FileInputStream(tmp);
        }
      } else {
        if (path.indexOf("mlterm/anim") == -1 && path.indexOf(".gif") != -1) {
          splitAnimationGif(path);
        }

        is = new FileInputStream(path);
      }

      Bitmap bmp = BitmapFactory.decodeStream(is);

      /* decodeStream() can return null. */
      if (bmp != null) {
        if (width != 0 && height != 0) {
          bmp = Bitmap.createScaledBitmap(bmp, width, height, false);
        }

        width = bmp.getWidth();
        height = bmp.getHeight();
        int[] pixels = new int[width * height + 2];

        bmp.getPixels(pixels, 0, width, 0, 0, width, height);
        pixels[pixels.length - 2] = width;
        pixels[pixels.length - 1] = height;

        return pixels;
      }
    } catch (Exception e) {
      System.err.println(e);
    }

    return null;
  }

  private LinearLayout makeTextEntry(String title, EditText edit) {
    LinearLayout layout = new LinearLayout(this);
    layout.setOrientation(LinearLayout.HORIZONTAL);

    TextView label = new TextView(this);
    label.setBackgroundColor(Color.TRANSPARENT);
    label.setText(title);

    layout.addView(label,
                   new LinearLayout.LayoutParams(LinearLayout.LayoutParams.FILL_PARENT,
                                                 LinearLayout.LayoutParams.WRAP_CONTENT, 2));
    layout.addView(edit,
                   new LinearLayout.LayoutParams(LinearLayout.LayoutParams.FILL_PARENT,
                                                 LinearLayout.LayoutParams.WRAP_CONTENT, 1));

    return layout;
  }

  private EditText serv_edit;
  private EditText port_edit;
  private EditText user_edit;
  private EditText pass_edit;
  private EditText encoding_edit;
  private EditText cmd_edit;
  private EditText privkey_edit;
  private LinearLayout dialogLayout;

  @Override protected Dialog onCreateDialog(int id) {
    if (id == 1) {
      Dialog dialog = new AlertDialog.Builder(this)
        .setIcon(android.R.drawable.ic_dialog_info)
        .setTitle("Connect to SSH Server")
        .setView(dialogLayout)
        .setPositiveButton("Connect", new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface dialog, int whichButton) {
              dialogOkClicked(user_edit.getText().toString(),
                              serv_edit.getText().toString(),
                              port_edit.getText().toString(),
                              encoding_edit.getText().toString(),
                              pass_edit.getText().toString(),
                              cmd_edit.getText().toString(),
                              privkey_edit.getText().toString());
            }
          })
        .setNegativeButton("Local", null)
        .create();

      dialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
          public void onDismiss(DialogInterface dialog) {
            if (nativeThread != null) {
              synchronized(nativeThread) {
                nativeThread.notifyAll();
              }
            }
          }
        });

      return dialog;
    } else {
      return null;
    }
  }

  /* Called from native activity thread */
  private void showConnectDialog(String user, String serv, String port, String encoding,
                                 String privkey) {
    nativeThread = Thread.currentThread();

    dialogLayout = new LinearLayout(this);
    dialogLayout.setOrientation(LinearLayout.VERTICAL);

    LinearLayout.LayoutParams params =
      new LinearLayout.LayoutParams(LinearLayout.LayoutParams.FILL_PARENT,
                                    LinearLayout.LayoutParams.FILL_PARENT);

    serv_edit = new EditText(this);
    serv_edit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    dialogLayout.addView(makeTextEntry("Server   ", serv_edit), params);

    port_edit = new EditText(this);
    port_edit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    dialogLayout.addView(makeTextEntry("Port     ", port_edit), params);

    user_edit = new EditText(this);
    user_edit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    dialogLayout.addView(makeTextEntry("User     ", user_edit), params);

    pass_edit = new EditText(this);
    pass_edit.setInputType(InputType.TYPE_CLASS_TEXT |
                           InputType.TYPE_TEXT_VARIATION_PASSWORD);
    dialogLayout.addView(makeTextEntry("Password ", pass_edit), params);

    encoding_edit = new EditText(this);
    encoding_edit.setInputType(InputType.TYPE_CLASS_TEXT |
                               InputType.TYPE_TEXT_VARIATION_URI);
    dialogLayout.addView(makeTextEntry("Encoding ", encoding_edit), params);

    cmd_edit = new EditText(this);
    cmd_edit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    dialogLayout.addView(makeTextEntry("Exec Cmd ", cmd_edit), params);

    privkey_edit = new EditText(this);
    privkey_edit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    dialogLayout.addView(makeTextEntry("Priv key ", privkey_edit), params);

    if (serv != null) {
      serv_edit.setText(serv, TextView.BufferType.NORMAL);
    }

    if (port != null) {
      port_edit.setText(port, TextView.BufferType.NORMAL);
    }

    if (user != null) {
      user_edit.setText(user, TextView.BufferType.NORMAL);
    }

    if (encoding != null) {
      encoding_edit.setText(encoding, TextView.BufferType.NORMAL);
    }

    if (privkey != null) {
      privkey_edit.setText(privkey, TextView.BufferType.NORMAL);
    }

    synchronized(nativeThread) {
      runOnUiThread(new Runnable() {
          @Override public void run() {
            showDialog(1);
          }
        });

      try {
        nativeThread.wait();
      } catch (InterruptedException e) {}
    }

    nativeThread = null;
    removeDialog(1);

    serv_edit = port_edit = user_edit = pass_edit = encoding_edit = privkey_edit = null;
  }

  /* Called from native activity thread or UI thread (from context menu) */
  private void showMessage(String message) {
    final Context ctx = this;
    final String msg = message;

    runOnUiThread(new Runnable() {
        @Override public void run() {
          Toast.makeText(ctx, msg, Toast.LENGTH_LONG).show();
        }
      });
  }
}
