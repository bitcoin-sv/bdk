package com.nchain.bsv.scriptengine;

import org.junit.Assert;
import org.junit.Test;

public class ScriptEngineTest {

	public ScriptEngine jniIF = new ScriptEngine();

	@Test
	public void testEvaluateScript() {
		int[] intArray = new int[] { 0x00, 0x6b, 0x54, 0x55, 0x93, 0x59, 0x87 };
		final byte[] var = new byte[intArray.length];
		for (int i = 0; i < intArray.length; i++) {
			var[i] = (byte) intArray[i];
		}

		Status result = jniIF.Evaluate(var, true, 0, "", 0, 0);

		Assert.assertEquals(0, result.getStatusCode());
		Assert.assertEquals("No error", result.getStatusMessage());

	}

	@Test
	public void testEvaluateTestMessage() {
		int[] intArray = new int[] { 0x00, 0x6b, 0x54, 0x55, 0x93, 0x59, 0x87 };
		final byte[] var = new byte[intArray.length];
		for (int i = 0; i < intArray.length; i++) {
			var[i] = (byte) intArray[i];
		}

		Status result = jniIF.Evaluate(var, true, 0, "", 0, 0);

		Assert.assertEquals(0, result.getStatusCode());
		Assert.assertEquals("No error", result.getStatusMessage());
	}

	@Test
	public void testEvaluateSimpleScript() {
		String ScriptArray = "0x00 0x6b 0x54 0x55 0x93 0x59 0x87";
		Status result = jniIF.EvaluateString(ScriptArray, true, 0, "", 0, 0);
		Assert.assertEquals(0, result.getStatusCode());
		Assert.assertEquals("No error", result.getStatusMessage());
	}

	@Test
	public void testEvaluateLessSimpleScript() {
		String ScriptArray = new String(
				"'abcdefghijklmnopqrstuvwxyz' 0xaa 0x4c 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 0x88");
		Status result = jniIF.EvaluateString(ScriptArray, true, 0, "", 0, 0);
		Assert.assertEquals(0, result.getStatusCode());
		Assert.assertEquals("No error", result.getStatusMessage());
	}

	@Test
	public void testEvaluateLessSimpleOpCodes() {
		String ScriptArray = new String(
				"'abcdefghijklmnopqrstuvwxyz' OP_HASH256 OP_PUSHDATA1 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 OP_EQUALVERIFY");
		Status result = jniIF.EvaluateString(ScriptArray, true, 0, "", 0, 0);
		Assert.assertEquals(0, result.getStatusCode());
		Assert.assertEquals("No error", result.getStatusMessage());
	}

	@Test
	public void testEvaluateWithTxData() {
		String ScriptArray = new String(
				"0x47 0x304402201fcefdc44242241964b5ca81a7e480364364b11a7f5a90163c42c0db3f3886140220387c073f39d63f60deb93b7935a84b93eb498fc12fbe3d65551b905fc360637b01 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY");

		String HexID = new String(
				"0100000001d92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff010a000000000000000000000000");

		int amt = 10;

		Status result = jniIF.EvaluateString(ScriptArray, true, 0, HexID, 0, amt);
		Assert.assertEquals(0, result.getStatusCode());
		Assert.assertEquals("No error", result.getStatusMessage());
	}

	/* Test Cases for JIRA-BS-94 */
	/* Successful Transaction */
	@Test
	public void testEvaluateStringValidTxn() {
		String scriptArray = new String(
				"0x47 0x30440220172f38b9fca6c5f7f8c06dc134c0856d90311232399efb0e6b10fd86ab46c8960220782578acd276e140b6098ab365745828a48d54d9a44d67cbdb287b76fe6fc08001 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY RETURN 0x4c64 0x03030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303");
		String hexID = new String(
				"01000000017f7b57604038e4556debe8b1ec64531d3a84bfb4f81cb4faf9fd9939873da5350000000000ffffffff010a000000000000000000000000");

		int amt = 10;
		Status result = jniIF.EvaluateString(scriptArray, true, 524288, hexID, 0, amt);

		Assert.assertEquals("No error", result.getStatusMessage());
		Assert.assertEquals(0, result.getStatusCode());

	}
	
	/* Invalid Transaction 
	 * Above successful Transaction with invalid hexID parameter*/
	@Test
	public void testEvaluateStringInvalidTxn() {
		String scriptArray = new String(
				"0x47 0x30440220172f38b9fca6c5f7f8c06dc134c0856d90311232399efb0e6b10fd86ab46c8960220782578acd276e140b6098ab365745828a48d54d9a44d67cbdb287b76fe6fc08001 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY RETURN 0x4c64 0x03030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303");
		String hexID = new String(
				"09000000017f7b57604038e4556debe8b1ec64531d3a84bfb4f81cb4faf9fd9939873da5350000000000ffffffff010a000000000000000000000000");

		int amt = 10;
		Status result = jniIF.EvaluateString(scriptArray, true, 524288, hexID, 0, amt);
		
		Assert.assertEquals("Script failed an OP_CHECKSIGVERIFY operation", result.getStatusMessage());
		Assert.assertEquals(19, result.getStatusCode());

	}
	
	/* Bad Signature 
	 * Above successful Transaction with Bad Signature*/
	@Test
	public void testEvaluateStringBadSignature() {
		/* Bad Signature*/
		String scriptArray = new String(
				"0x47 0x304402205d1c873a5e115043534d4bdd06762fbb67989f30493c24be531688f655ea25250220558d14f2eef3ff5de1798372afef83a9dd7d401f81f5773443f9b48c46ce187c01 0x41 0x090b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIG RETURN 0x4c64 0x03030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303");

		String hexID = new String(
				"0100000001ca8e5e634222ad1dce6d4286af725cf97e31ff98922db1a467d38642a7e2cbe60000000000ffffffff010a000000000000000000000000");

		int amt = 10;
		Status result = jniIF.EvaluateString(scriptArray, true, 524288, hexID, 0, amt);
	
		Assert.assertEquals("Script failed an OP_EQUALVERIFY operation", result.getStatusMessage());
		Assert.assertEquals(17, result.getStatusCode());

	}

	/* Bad Pubkey
	 *  Above successful Transaction with Bad PubKey
	 */
	@Test
	public void testEvaluateStringBadPubKey() {
		
		/*Bad PubKey*/
		String scriptArray = new String(
				"0x47 0x80440220172f38b9fca6c5f7f8c06dc134c0856d90311232399efb0e6b10fd86ab46c8960220782578acd276e140b6098ab365745828a48d54d9a44d67cbdb287b76fe6fc08001 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY RETURN 0x4c64 0x03030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303");
		String hexID = new String(
				"01000000017f7b57604038e4556debe8b1ec64531d3a84bfb4f81cb4faf9fd9939873da5350000000000ffffffff010a000000000000000000000000");

		int amt = 10;
		Status result = jniIF.EvaluateString(scriptArray, true, 524288, hexID, 0, amt);
	
		Assert.assertEquals("Script failed an OP_CHECKSIGVERIFY operation", result.getStatusMessage());
		Assert.assertEquals(19, result.getStatusCode());

	}

}

