package org.racob.test.safearray;

import org.racob.activeX.ActiveXComponent;
import org.racob.com.ComException;
import org.racob.com.Dispatch;
import org.racob.com.SafeArray;
import org.racob.com.Variant;
import org.racob.test.BaseTestCase;

/**
 * Test class to verify dispatch with SafeArray
 */
public class SafeArrayDispatchTest extends BaseTestCase {
	public void testDispatchWithSafeArray() {
		try {
			String scriptCommand = "1+(2*4)-3";
			String lang = "VBScript";
			ActiveXComponent sControl = new ActiveXComponent("ScriptControl");
			Dispatch.put(sControl, "Language", lang);

			Variant result = Dispatch.call(sControl, "Eval", scriptCommand);
			assertTrue(result.toString().equals("6"));

			// wrap the script control in a variant
			Variant v = new Variant(sControl);

			// create a safe array of type dispatch
			SafeArray sa = new SafeArray(Variant.VariantDispatch);

			// put the variant in the array
			sa.add(v);

			// take it back out
			Variant v2 = (Variant) sa.get(0);
			Dispatch d = v2.toDispatch();

			// make sure you can call eval on it
			result = Dispatch.call(d, "Eval", scriptCommand);
			assertTrue(result.toString().equals("6"));
		} catch (ComException e) {
			e.printStackTrace();
			fail("script failure " + e);
		}
	}
}
