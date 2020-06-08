package jni.bscriptIF;

import org.junit.Assert;
import org.junit.Test;

public class BScriptJNITest {
	
	public BScriptJNI jniIF = new BScriptJNI();

	@Test
	public void ScriptExecute() {
	    int[] intArray = new int[] {0x00, 0x6b, 0x54, 0x55, 0x93, 0x59,0x87};
	    final byte[] var = new byte[intArray.length];
	    for (int i = 0; i < intArray.length; i++){
            var[i] = (byte) intArray[i];
        }
        Assert.assertEquals(jniIF.EvalScript(var), true);
	}
	
	@Test
	public void ScriptExecuteSimple(){
	     String[] ScriptArray = new String[] {"0x00 0x6b 0x54 0x55 0x93 0x59 0x87","0x00 0x6b 0x54 0x55 0x93 0x59 0x87"};
	     Assert.assertEquals(jniIF.EvalScriptString(ScriptArray),true);
	}
	
	@Test
	public void ScriptExecuteLessSimple(){
	     String[] ScriptArray = new String[] {"'abcdefghijklmnopqrstuvwxyz' 0xaa 0x4c 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 0x88"};
	     Assert.assertEquals(jniIF.EvalScriptString(ScriptArray),true);
	}
	
	@Test
	public void ScriptExecuteLessSimpleOpCodes(){
	     String[] ScriptArray = new String[] {"'abcdefghijklmnopqrstuvwxyz' OP_HASH256 OP_PUSHDATA1 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 OP_EQUALVERIFY"};
	     Assert.assertEquals(jniIF.EvalScriptString(ScriptArray),true);
	}
}
