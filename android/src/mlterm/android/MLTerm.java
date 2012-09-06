/*
 *	$Id$
 */

package  mlterm.android ;

import  android.app.Activity ;
import  android.os.Bundle ;
import  android.widget.EditText ;
import  android.view.ViewGroup.LayoutParams ;


public class MLTerm extends Activity
{
	@Override public void onCreate( Bundle  state)
	{
		super.onCreate( state) ;

		EditText  edit = new  EditText( this) ;
		setContentView( edit ,
			new LayoutParams( LayoutParams.WRAP_CONTENT , LayoutParams.WRAP_CONTENT)) ;
	}
}
