package edu.umich.soar;

import sml.Identifier;
import sml.IntElement;

/**
 * Wrapper for int valued working memory elements.
 * 
 * @author voigtjr
 *
 */
public class IntWme implements Wme
{
    /**
     * Create working memory element instance with no value. The value will be set
     * (and underlying SML Wme created) on first update.
     * @param parent Parent identifier
     * @param attr Attribute
     * @return Wme ready for update
     */
    public static IntWme newInstance(Identifier parent, String attr) {
	return new IntWme(parent, attr);
    }
    
    /**
     * Create working memory element instance with known initial value.
     * @param parent Parent identifier
     * @param attr Attribute
     * @param val Initial value
     * @return Wme ready for update
     */
    public static IntWme newInstance(Identifier parent, String attr, int val) {
	IntWme temp = new IntWme(parent, attr);
	temp.update(val);
	return temp;
    }
    
    private final Identifier parent;
    private final String attr;
    private IntElement wme;
    
    private IntWme(Identifier parent, String attr) {
	this.parent = parent;
	this.attr = attr;
    }
    
    /**
     * Update to possibly new value. See Agent.BlinkIfNoChange()
     * @param val The new value
     */
    public void update(int val) {
	if (wme != null) {
	    wme.Update(val);
	} else {
	    wme = parent.CreateIntWME(attr, val);
	}
    }
    
    /**
     * Get the current value.
     * @throws IllegalStateException if destroyed or value was not set
     * @return Current value
     */
    public int getValue() {
	if (wme == null) 
	    throw new IllegalStateException();
	return wme.GetValue();
    }
    
    @Override
    public void destroy() {
	if (wme != null) {
	    wme.DestroyWME();
	    wme = null;
	}
    }
}
