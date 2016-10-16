/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

package mlterm;

public interface MLTermPtyListener {
  public void executeCommand(String cmd);

  public void linesScrolledOut(int size);

  /*
   * If width and height are greater than 0, resize by pixel.
   * If cols and rows are greater than 0, resize by character.
   */
  public void resize(int width, int height, int cols, int rows);

  public void bell();
}
