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
import android.view.View.OnClickListener;
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
import java.net.URI;
import java.net.URL;
import java.io.*;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.LinearLayout;
import android.widget.TableLayout;
import android.widget.TableRow;
import android.widget.Button;
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
  private native void dialogOkClicked(String user, String serv /* can include proto */,
                                      String port, String encoding,
                                      String pass, String cmd, String privkey);
  private native void updateScreen();
  private native void execCommand(String cmd);
  private native void resumeNative();
  private native void pauseNative();
  private native boolean isSSH();

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
  private final int MENU_ZMODEM_START_ID = 10;
  private final int MENU_RESET_ID = 11;
  private final int MENU_SCP_ID = 12;
  private final int MENU_CONFIG_ID = 13;
  private final int MENU_CANCEL_ID = 14;

  @Override
  public void onCreateContextMenu(ContextMenu menu, View view,
                                  ContextMenu.ContextMenuInfo info) {
    super.onCreateContextMenu(menu, view, info);
    /* menu.setHeaderTitle("Menu"); */
    menu.add(0, MENU_PASTE_ID, 0, "Paste from clipboard");
    menu.add(0, MENU_NEWSCREEN_ID, 0, "Open new screen");
    menu.add(0, MENU_LOCALPTY_ID, 0, "Open local pty");
    menu.add(0, MENU_SSH_ID, 0, "Connect to SSH server");
    if (isSSH()) {
      menu.add(0, MENU_SCP_ID, 0, "SCP");
    }
    menu.add(0, MENU_VSPLIT_ID, 0, "Split screen vertically");
    menu.add(0, MENU_HSPLIT_ID, 0, "Split screen horizontally");
    menu.add(0, MENU_NEXTPTY_ID, 0, "Next pty");
    menu.add(0, MENU_PREVPTY_ID, 0, "Previous pty");
    menu.add(0, MENU_UPDATESCREEN_ID, 0, "Update screen");
    menu.add(0, MENU_CLOSESCREEN_ID, 0, "Close splitted screen");
    menu.add(0, MENU_ZMODEM_START_ID, 0, "Start zmodem");
    menu.add(0, MENU_RESET_ID, 0, "Reset terminal");
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
        execCommand("open_pty");
        return true;
      case MENU_LOCALPTY_ID:
        execCommand("mlclientx --serv=");
        return true;
      case MENU_SSH_ID:
        execCommand("ssh");
        return true;
      case MENU_VSPLIT_ID:
        execCommand("vsplit_screen");
        return true;
      case MENU_HSPLIT_ID:
        execCommand("hsplit_screen");
        return true;
      case MENU_NEXTPTY_ID:
        execCommand("next_pty");
        return true;
      case MENU_PREVPTY_ID:
        execCommand("prev_pty");
        return true;
      case MENU_UPDATESCREEN_ID:
        updateScreen();
        return true;
      case MENU_CLOSESCREEN_ID:
        execCommand("close_screen");
        return true;
      case MENU_ZMODEM_START_ID:
        execCommand("zmodem_start");
        return true;
      case MENU_RESET_ID:
        execCommand("full_reset");
        return true;
      case MENU_SCP_ID:
        showDialog(2); /* SCP dialog */
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
  private LinearLayout connectDialogLayout;
  private TableLayout servListDialogLayout;
  private boolean openLocalPty = false;

  @Override protected Dialog onCreateDialog(int id) {
    if (id == 1) {
      Dialog dialog = new AlertDialog.Builder(this)
        .setIcon(android.R.drawable.ic_dialog_info)
        .setTitle("Connect to SSH Server")
        .setView(connectDialogLayout)
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
    } else if (id == 3) {
      Dialog dialog = new AlertDialog.Builder(this)
        .setIcon(android.R.drawable.ic_dialog_info)
        .setTitle("Server List")
        .setView(servListDialogLayout)
        .setPositiveButton("Save&Next", new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface dialog, int whichButton) {
              writeServerList();
            }
          })
        .setNegativeButton("Local", new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface dialog, int whichButton) {
              openLocalPty = true;
            }
          })
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
    } else if (id == 2) {
      final EditText remote_edit;
      final EditText local_edit;

      LinearLayout layout = new LinearLayout(this);
      layout.setOrientation(LinearLayout.VERTICAL);

      LinearLayout.LayoutParams params =
        new LinearLayout.LayoutParams(LinearLayout.LayoutParams.FILL_PARENT,
                                      LinearLayout.LayoutParams.FILL_PARENT);

      remote_edit = new EditText(this);
      remote_edit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
      layout.addView(makeTextEntry("Remote", remote_edit), params);

      local_edit = new EditText(this);
      local_edit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
      layout.addView(makeTextEntry("Local ", local_edit), params);

      Dialog dialog = new AlertDialog.Builder(this)
        .setIcon(android.R.drawable.ic_dialog_info)
        .setTitle("Source file path / Dest dir path")
        .setView(layout)
        .setPositiveButton("Send", new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface dialog, int whichButton) {
              execCommand("scp \"local:" + local_edit.getText().toString() + "\" \"remote:" +
                          remote_edit.getText().toString() + "\"");
            }
          })
        .setNeutralButton("Receive", new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface dialog, int whichButton) {
              execCommand("scp \"remote:" + remote_edit.getText().toString() + "\" \"local:" +
                          local_edit.getText().toString() + "\"");
            }
          })
        .setNegativeButton("Cancel", null)
        .create();

      return dialog;
    } else {
      return null;
    }
  }

  /* 0: Select, 1:Server, 2:User, 3:Port, 4:Encoding, 5: Privkey, 6: X, 7: + */
  private final int SERVER_LIST_COLUMNS = 8;

  /* See bl_get_home_dir() in bl_path.c */
  private String getServerListConfig() throws IOException {
    String configDir;
    File file = new File("/sdcard");

    if (file.isDirectory()) {
      configDir = "/sdcard/.mlterm";
    } else {
      file = new File("/mnt/sdcard");

      if (file.isDirectory()) {
        configDir = "/mnt/sdcard/.mlterm";
      } else {
        file = new File("/extsdcard");

        if (file.isDirectory()) {
          configDir = "/extsdcard";
        } else {
          showMessage("Config dir not found");

          throw new IOException();
        }
      }
    }

    file = new File(configDir);
    if (!file.exists()) {
      try {
        file.mkdir();
      } catch (Exception e) {
        showMessage("Failed to make " + configDir);

        throw new IOException();
      }
    }

    return configDir + "/servers";
  }

  private void writeServerList() {
    try {
      FileWriter out = new FileWriter(getServerListConfig());

      int num = servListDialogLayout.getChildCount();
      for (int count = 1 /* skip title */; count < num; count++) {
        TableRow tableRow = (TableRow)servListDialogLayout.getChildAt(count);

        if (tableRow.getChildCount() == SERVER_LIST_COLUMNS) {
          String server = ((EditText)tableRow.getChildAt(1)).getText().toString();

          if (!server.isEmpty()) {
            int proto_end = server.indexOf("://");

            if (proto_end == -1) {
              out.write("ssh://");
            } else {
              out.write(server.substring(0, proto_end + 3));
              server = server.substring(proto_end + 3);
            }

            String user = ((EditText)tableRow.getChildAt(2)).getText().toString();

            if (!user.isEmpty()) {
              out.write(user);
              out.write("@");
            }
            out.write(server);

            String port = ((EditText)tableRow.getChildAt(3)).getText().toString();
            if (!port.isEmpty()) {
              out.write(":");
              out.write(port);
            }

            out.write("\t");
            out.write(((EditText)tableRow.getChildAt(4)).getText().toString());
            out.write("\t");
            out.write(((EditText)tableRow.getChildAt(5)).getText().toString());
            out.write("\tEOL\n");
          }
        }
      }

      out.close();
    } catch (IOException e) {
      //e.printStackTrace();
    }
  }

  private void addServerToTable(URI uri, String encoding, String privkey, int index) {
    final TableRow tableRow = new TableRow(this);

    final EditText e_serv = new EditText(this);
    final EditText e_user = new EditText(this);
    final EditText e_port = new EditText(this);
    final EditText e_encoding = new EditText(this);
    final EditText e_privkey = new EditText(this);

    if (uri != null) {
      String proto = uri.getScheme();

      if (proto.equals("mosh")) {
        e_serv.setText("mosh://" + uri.getHost());
      } else {
        e_serv.setText(uri.getHost());
      }
      e_user.setText(uri.getUserInfo());
      int port = uri.getPort();
      if (port != -1) {
        e_port.setText(String.valueOf(port));
      }
    }
    if (encoding != null) {
      e_encoding.setText(encoding);
    }
    if (privkey != null) {
      e_privkey.setText(privkey);
    }

    Button button = new Button(this);
    button.setText("Select");
    button.setOnClickListener(new OnClickListener() {
        public void onClick(View v) {
          writeServerList();

          serv_edit = e_serv;
          user_edit = e_user;
          port_edit = e_port;
          encoding_edit = e_encoding;
          privkey_edit = e_privkey;

          dismissDialog(3);
        }
      });
    tableRow.addView(button);

    tableRow.addView(e_serv);
    tableRow.addView(e_user);
    tableRow.addView(e_port);
    tableRow.addView(e_encoding);
    tableRow.addView(e_privkey);

    button = new Button(this);
    button.setText("X");
    button.setOnClickListener(new OnClickListener() {
        public void onClick(View v) {
          servListDialogLayout.removeView(tableRow);
        }
      });
    tableRow.addView(button);

    button = new Button(this);
    button.setText("+");
    button.setOnClickListener(new OnClickListener() {
        public void onClick(View v) {
          int num = servListDialogLayout.getChildCount();

          for (int count = 1; count < num; count++) {
            if (servListDialogLayout.getChildAt(count) == tableRow) {
              addServerToTable(null, null, null, count);
              break;
            }
          }
        }
      });
    tableRow.addView(button);

    if (index >= 0) {
      servListDialogLayout.addView(tableRow, index);
    } else {
      servListDialogLayout.addView(tableRow);
    }
  }

  private void showServerList() {
    servListDialogLayout = new TableLayout(this);

    TableRow tableRow = new TableRow(this);
    String title[] = { "", "Server", "User", "Port", "Encoding", "Privkey", "", "" };

    for (int count = 0; count < title.length; count++) {
      TextView text = new TextView(this);
      text.setText(title[count]);
      tableRow.addView(text);
    }

    servListDialogLayout.addView(tableRow);

    try {
      BufferedReader in = new BufferedReader(new FileReader(getServerListConfig()));
      String line;

      while ((line = in.readLine()) != null) {
        String[] list = line.split("\t");

        if (list.length == 4) { /* list[3] == "EOL" */
          try {
            /* Can throw IllegalArgumentException or NullPointerException */
            URI uri = URI.create(list[0]);

            addServerToTable(uri, list[1], list[2], -1);
          } catch (Exception e) {
            //e.printStackTrace();
          }
        }
      }

      in.close();
    } catch (FileNotFoundException e) {
    } catch (IOException e) {
    }

    /* Empty entry at the end */
    addServerToTable(null, null, null, -1);

    /* 1:Server, 2:User, 3:Port, 4:Encoding, 5: Privkey, 6: X, 7: + */
    for (int index = 1; index < SERVER_LIST_COLUMNS; index++) {
      servListDialogLayout.setColumnShrinkable(index, true);
    }

    synchronized(nativeThread) {
      runOnUiThread(new Runnable() {
          @Override public void run() {
            showDialog(3);
          }
        });

      try {
        nativeThread.wait();
      } catch (InterruptedException e) {}
    }

    removeDialog(1);
  }

  /* Called from native activity thread */
  private void showConnectDialog(String proto, String user, String serv, String port,
                                 String encoding, String privkey) {
    nativeThread = Thread.currentThread();

    if (serv == null) {
      showServerList();

      if (openLocalPty) {
        nativeThread = null;
        openLocalPty = false;

        return;
      }
    }

    connectDialogLayout = new LinearLayout(this);
    connectDialogLayout.setOrientation(LinearLayout.VERTICAL);

    LinearLayout.LayoutParams params =
      new LinearLayout.LayoutParams(LinearLayout.LayoutParams.FILL_PARENT,
                                    LinearLayout.LayoutParams.FILL_PARENT);

    /*
     * showServerList() can set serv_edit, user_edit, port_edit, encoding_edit
     * and privkey_edit.
     */
    if (serv_edit != null) {
      serv = serv_edit.getText().toString();
      proto = null; /* Prefer the value of showServerList() */
    }
    if (user_edit != null) {
      user = user_edit.getText().toString();
    }
    if (port_edit != null) {
      port = port_edit.getText().toString();
    }
    if (encoding_edit != null) {
      encoding = encoding_edit.getText().toString();
    }
    if (privkey_edit != null) {
      privkey = privkey_edit.getText().toString();
    }

    serv_edit = new EditText(this);
    serv_edit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    connectDialogLayout.addView(makeTextEntry("Server   ", serv_edit), params);

    port_edit = new EditText(this);
    port_edit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    connectDialogLayout.addView(makeTextEntry("Port     ", port_edit), params);

    user_edit = new EditText(this);
    user_edit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    connectDialogLayout.addView(makeTextEntry("User     ", user_edit), params);

    pass_edit = new EditText(this);
    pass_edit.setInputType(InputType.TYPE_CLASS_TEXT |
                           InputType.TYPE_TEXT_VARIATION_PASSWORD);
    connectDialogLayout.addView(makeTextEntry("Password ", pass_edit), params);

    encoding_edit = new EditText(this);
    encoding_edit.setInputType(InputType.TYPE_CLASS_TEXT |
                               InputType.TYPE_TEXT_VARIATION_URI);
    connectDialogLayout.addView(makeTextEntry("Encoding ", encoding_edit), params);

    cmd_edit = new EditText(this);
    cmd_edit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    connectDialogLayout.addView(makeTextEntry("Exec Cmd ", cmd_edit), params);

    privkey_edit = new EditText(this);
    privkey_edit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    connectDialogLayout.addView(makeTextEntry("Priv key ", privkey_edit), params);

    boolean serv_value_is_set;

    if (serv != null && !serv.isEmpty()) {
      if (proto == null) {
        serv_edit.setText(serv, TextView.BufferType.NORMAL);
      } else {
        serv_edit.setText(proto + "://" + serv, TextView.BufferType.NORMAL);
      }
      serv_value_is_set = true;
    } else {
      serv_value_is_set = false;
    }

    if (port != null && !port.isEmpty()) {
      port_edit.setText(port, TextView.BufferType.NORMAL);
    }

    final EditText focus_edit;

    if (user != null && !user.isEmpty()) {
      user_edit.setText(user, TextView.BufferType.NORMAL);

      if (serv_value_is_set) {
        focus_edit = pass_edit;
      } else {
        focus_edit = null;
      }
    } else {
      if (serv_value_is_set) {
        focus_edit = user_edit;
      } else {
        focus_edit = null;
      }
    }

    if (encoding != null && !encoding.isEmpty()) {
      encoding_edit.setText(encoding, TextView.BufferType.NORMAL);
    }

    if (privkey != null && !privkey.isEmpty()) {
      privkey_edit.setText(privkey, TextView.BufferType.NORMAL);
    }

    synchronized(nativeThread) {
      runOnUiThread(new Runnable() {
          @Override public void run() {
            if (focus_edit != null) {
              focus_edit.requestFocus();
            }

            showDialog(1);
          }
        });

      try {
        nativeThread.wait();
      } catch (InterruptedException e) {}
    }

    nativeThread = null;
    removeDialog(1);

    serv_edit = port_edit = user_edit = pass_edit = encoding_edit = cmd_edit = privkey_edit = null;
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
