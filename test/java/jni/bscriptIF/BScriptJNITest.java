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

        BScriptJNIResult result = jniIF.EvalScript(var);

        Assert.assertEquals(result.getStatusCode(), 0);
	}

	@Test
    public void ScriptExecuteTestMessage() {
    	int[] intArray = new int[] {0x00, 0x6b, 0x54, 0x55, 0x93, 0x59,0x87};
    	final byte[] var = new byte[intArray.length];
    	for (int i = 0; i < intArray.length; i++){
            var[i] = (byte) intArray[i];
         }

         BScriptJNIResult result = jniIF.EvalScript(var);

         Assert.assertEquals(result.getStatusMessage(), "No error");
    }


	@Test
	public void ScriptExecuteSimple(){
	     String[] ScriptArray = new String[] {"0x00 0x6b 0x54 0x55 0x93 0x59 0x87","0x00 0x6b 0x54 0x55 0x93 0x59 0x87"};

	     BScriptJNIResult[] result = jniIF.EvalScriptString(ScriptArray);

	     Assert.assertEquals(result[0].getStatusCode(), 0);
	}



	@Test
	public void ScriptExecuteLessSimple(){
	     String[] ScriptArray = new String[] {"'abcdefghijklmnopqrstuvwxyz' 0xaa 0x4c 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 0x88"};

	     BScriptJNIResult[] result = jniIF.EvalScriptString(ScriptArray);

	     Assert.assertEquals(result[0].getStatusCode(), 0);
	}

	@Test
	public void ScriptExecuteLessSimpleOpCodes(){
	     String[] ScriptArray = new String[] {"'abcdefghijklmnopqrstuvwxyz' OP_HASH256 OP_PUSHDATA1 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 OP_EQUALVERIFY"};

	     BScriptJNIResult[] result = jniIF.EvalScriptString(ScriptArray);

         Assert.assertEquals(result[0].getStatusCode(), 0);
	}
}
