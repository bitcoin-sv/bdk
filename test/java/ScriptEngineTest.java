package com.nchain.bsv.scriptengine;

import org.junit.Assert;
import org.junit.Test;
import java.io.FileReader;
import java.io.BufferedReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.io.IOException;

public class ScriptEngineTest {

	public ScriptEngine jniIF = new ScriptEngine();

	@Test
	public void testEvaluateScript() {
		int[] intArray = new int[] { 0x00, 0x6b, 0x54, 0x55, 0x93, 0x59, 0x87 };
		final byte[] var = new byte[intArray.length];
		for (int i = 0; i < intArray.length; i++) {
			var[i] = (byte) intArray[i];
		}

		Status result = jniIF.evaluate(var, true, 0, "", 0, 0);

		Assert.assertEquals(0, result.getStatusCode());
		Assert.assertEquals("No error", result.getStatusMessage());

	}

	@Test(expected = IllegalArgumentException.class)
	public void TestNullHexStr() {
		int[] intArray = new int[] { 0x00, 0x6b, 0x54, 0x55, 0x93, 0x59, 0x87 };
		final byte[] var = new byte[intArray.length];
		for (int i = 0; i < intArray.length; i++) {
			var[i] = (byte) intArray[i];
		}

		jniIF.evaluate(var, true, 0, null, 0, 0);
	}

	@Test
	public void testEvaluateTestMessage() {
		int[] intArray = new int[] { 0x00, 0x6b, 0x54, 0x55, 0x93, 0x59, 0x87 };
		final byte[] var = new byte[intArray.length];
		for (int i = 0; i < intArray.length; i++) {
			var[i] = (byte) intArray[i];
		}

		Status result = jniIF.evaluate(var, true, 0, "", 0, 0);

		Assert.assertEquals(0, result.getStatusCode());
		Assert.assertEquals("No error", result.getStatusMessage());
	}

	@Test
	public void testEvaluateSimpleScript() {
		String ScriptArray = "0x00 0x6b 0x54 0x55 0x93 0x59 0x87";
		Status result = jniIF.evaluateString(ScriptArray, true, 0, "", 0, 0);
		Assert.assertEquals(0, result.getStatusCode());
		Assert.assertEquals("No error", result.getStatusMessage());
	}

	@Test
	public void testEvaluateLessSimpleScript() {
		String ScriptArray = new String(
				"'abcdefghijklmnopqrstuvwxyz' 0xaa 0x4c 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 0x88");
		Status result = jniIF.evaluateString(ScriptArray, true, 0, "", 0, 0);
		Assert.assertEquals(0, result.getStatusCode());
		Assert.assertEquals("No error", result.getStatusMessage());
	}

	@Test
	public void testEvaluateLessSimpleOpCodes() {
		String ScriptArray = new String(
				"'abcdefghijklmnopqrstuvwxyz' OP_HASH256 OP_PUSHDATA1 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 OP_EQUALVERIFY");
		Status result = jniIF.evaluateString(ScriptArray, true, 0, "", 0, 0);
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

		Status result = jniIF.evaluateString(ScriptArray, true, 0, HexID, 0, amt);
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
		Status result = jniIF.evaluateString(scriptArray, true, 524288, hexID, 0, amt);

		Assert.assertEquals("No error", result.getStatusMessage());
		Assert.assertEquals(0, result.getStatusCode());

	}

	/*
	 * Invalid Transaction Above successful Transaction with invalid hexID parameter
	 */
	@Test
	public void testEvaluateStringInvalidTxn() {
		String scriptArray = new String(
				"0x47 0x30440220172f38b9fca6c5f7f8c06dc134c0856d90311232399efb0e6b10fd86ab46c8960220782578acd276e140b6098ab365745828a48d54d9a44d67cbdb287b76fe6fc08001 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY RETURN 0x4c64 0x03030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303");
		String hexID = new String(
				"09000000017f7b57604038e4556debe8b1ec64531d3a84bfb4f81cb4faf9fd9939873da5350000000000ffffffff010a000000000000000000000000");

		int amt = 10;
		Status result = jniIF.evaluateString(scriptArray, true, 524288, hexID, 0, amt);

		Assert.assertEquals("Script failed an OP_CHECKSIGVERIFY operation", result.getStatusMessage());
		Assert.assertEquals(19, result.getStatusCode());

	}

	/*
	 * Bad Signature Above successful Transaction with Bad Signature
	 */
	@Test
	public void testEvaluateStringBadSignature() {
		/* Bad Signature */
		String scriptArray = new String(
				"0x47 0x304402205d1c873a5e115043534d4bdd06762fbb67989f30493c24be531688f655ea25250220558d14f2eef3ff5de1798372afef83a9dd7d401f81f5773443f9b48c46ce187c01 0x41 0x090b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIG RETURN 0x4c64 0x03030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303");

		String hexID = new String(
				"0100000001ca8e5e634222ad1dce6d4286af725cf97e31ff98922db1a467d38642a7e2cbe60000000000ffffffff010a000000000000000000000000");

		int amt = 10;
		Status result = jniIF.evaluateString(scriptArray, true, 524288, hexID, 0, amt);

		Assert.assertEquals("Script failed an OP_EQUALVERIFY operation", result.getStatusMessage());
		Assert.assertEquals(17, result.getStatusCode());

	}

	/*
	 * Bad Pubkey Above successful Transaction with Bad PubKey
	 */
	@Test
	public void testEvaluateStringBadPubKey() {

		/* Bad PubKey */
		String scriptArray = new String(
				"0x47 0x80440220172f38b9fca6c5f7f8c06dc134c0856d90311232399efb0e6b10fd86ab46c8960220782578acd276e140b6098ab365745828a48d54d9a44d67cbdb287b76fe6fc08001 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY RETURN 0x4c64 0x03030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303");
		String hexID = new String(
				"01000000017f7b57604038e4556debe8b1ec64531d3a84bfb4f81cb4faf9fd9939873da5350000000000ffffffff010a000000000000000000000000");

		int amt = 10;
		Status result = jniIF.evaluateString(scriptArray, true, 524288, hexID, 0, amt);

		Assert.assertEquals("Script failed an OP_CHECKSIGVERIFY operation", result.getStatusMessage());
		Assert.assertEquals(19, result.getStatusCode());

	}

	/* Test Cases for JIRA-BS-95 */
	/* Transaction with multiple inputs uses the 2nd input index = 1 */
	@Test
	public void testEvaluateStringMultipleInputs() {

		String scriptArray = new String(
				"0x47 0x304402201217437c4be1f4a11f8de3b28fe73c0ceeab54d070eef1b38b29e3e44f17505102200706107eae921558e8e58938017c584e721094ffba5ae86c7202c6c19046116101 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY");

		String hexID = new String(
				"01000000020000000000000000000000000000000000000000000000000000000000000000ffffffff00ffffffffd92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff02ffffffffffffffff000a000000000000000000000000");

		int amt = 10;
		Status result = jniIF.evaluateString(scriptArray, true, 0, hexID, 1, amt);

		Assert.assertEquals("No error", result.getStatusMessage());
		Assert.assertEquals(0, result.getStatusCode());

	}

	/*
	 * Transaction with multiple inputs uses the 2nd input index = 1, here index = 5
	 * is passed to check negative scenario
	 */
	@Test
	public void testEvaluateStringIncorrectIndex() {

		String scriptArray = new String(
				"0x47 0x304402201217437c4be1f4a11f8de3b28fe73c0ceeab54d070eef1b38b29e3e44f17505102200706107eae921558e8e58938017c584e721094ffba5ae86c7202c6c19046116101 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY");

		String hexID = new String(
				"01000000020000000000000000000000000000000000000000000000000000000000000000ffffffff00ffffffffd92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff02ffffffffffffffff000a000000000000000000000000");

		int amt = 10;
		Status result = jniIF.evaluateString(scriptArray, true, 0, hexID, 5, amt);

		Assert.assertEquals("Script failed an OP_CHECKSIGVERIFY operation", result.getStatusMessage());
		Assert.assertEquals(19, result.getStatusCode());

	}

	@Test
	public void testEvaluateInvalidTestFile() throws IOException {
		List<List<String>> tests = readCsvFile("invalid_tests.csv");

		tests.forEach(t -> {
			String scriptArray = t.get(0);
			int flag = Integer.valueOf(t.get(1));
			String txHex = t.get(2);

			Status result;
			try {
				result = jniIF.evaluateString(scriptArray, true, flag, txHex, 0, 0);
			} catch (Exception ex) {
				return;
			}

			Assert.assertNotEquals(result.getStatusCode(), 0);
		});
	}

	@Test
	public void testEvaluateValidTestFile() throws IOException {
		List<List<String>> tests = readCsvFile("valid_tests.csv");

		tests.forEach(t -> {
			String scriptArray = t.get(0);
			int flag = Integer.valueOf(t.get(1));
			String txHex = t.get(2);

			Status result = jniIF.evaluateString(scriptArray, true, flag, txHex, 0, 0);

			Assert.assertEquals(result.getStatusCode(), 0);
		});
	}

	private List<List<String>> readCsvFile(String fileName) throws IOException {
		List<List<String>> tests = new ArrayList<>();
		try (BufferedReader br = new BufferedReader(
				new FileReader(System.getProperty("java_test_data_dir") + "/" + fileName))) {
			String line;
			while ((line = br.readLine()) != null) {
				String[] values = line.split(",");
				tests.add(Arrays.asList(values));
			}
		}

		return tests;
	}

	/*
	 * Test cases for JIRA BS-44 Transaction using the 2nd input index = 1 , here
	 * pass Index = -2
	 */
	@Test
	public void testEvaluateStringNegativeIndex() {

		String scriptArray = new String(
				"0x47 0x304402201217437c4be1f4a11f8de3b28fe73c0ceeab54d070eef1b38b29e3e44f17505102200706107eae921558e8e58938017c584e721094ffba5ae86c7202c6c19046116101 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY");

		String hexID = new String(
				"01000000020000000000000000000000000000000000000000000000000000000000000000ffffffff00ffffffffd92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff02ffffffffffffffff000a000000000000000000000000");

		int amt = 10;
		Status result = jniIF.evaluateString(scriptArray, true, 0, hexID, -2, amt);

		Assert.assertEquals("Script failed an OP_CHECKSIGVERIFY operation", result.getStatusMessage());
		Assert.assertEquals(19, result.getStatusCode());

	}

	/* consensus flag = false */
	/* There is no change when flag=true or false for the moment */
	@Test
	public void testEvaluateStringConsensusFlag() {

		String scriptArray = new String(
				"0x47 0x304402201217437c4be1f4a11f8de3b28fe73c0ceeab54d070eef1b38b29e3e44f17505102200706107eae921558e8e58938017c584e721094ffba5ae86c7202c6c19046116101 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY");

		String hexID = new String(
				"01000000020000000000000000000000000000000000000000000000000000000000000000ffffffff00ffffffffd92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff02ffffffffffffffff000a000000000000000000000000");

		int amt = 10;
		Status result = jniIF.evaluateString(scriptArray, false, 0, hexID, 1, amt);

		Assert.assertEquals("No error", result.getStatusMessage());
		Assert.assertEquals(0, result.getStatusCode());

	}

	/* Incorrect Script Flag */
	/*
	 * valid script flag but does not work for OP_RETURN transaction. Pass
	 * scriptflag = 0 instead of correct script flag 524288
	 */
	@Test
	public void testEvaluateStringIncorrectScriptFlag() {

		String scriptArray = new String(
				"0x47 0x30440220172f38b9fca6c5f7f8c06dc134c0856d90311232399efb0e6b10fd86ab46c8960220782578acd276e140b6098ab365745828a48d54d9a44d67cbdb287b76fe6fc08001 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY RETURN 0x4c64 0x03030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303");
		String hexID = new String(
				"01000000017f7b57604038e4556debe8b1ec64531d3a84bfb4f81cb4faf9fd9939873da5350000000000ffffffff010a000000000000000000000000");

		int amt = 10;
		Status result = jniIF.evaluateString(scriptArray, true, 0, hexID, 0, amt);

		Assert.assertEquals("OP_RETURN was encountered", result.getStatusMessage());
		Assert.assertEquals(3, result.getStatusCode());

	}

	/* Negative Script Flag */
	@Test
	public void testEvaluateStringNegativeScriptFlag() {

		String scriptArray = new String(
				"0x47 0x304402201217437c4be1f4a11f8de3b28fe73c0ceeab54d070eef1b38b29e3e44f17505102200706107eae921558e8e58938017c584e721094ffba5ae86c7202c6c19046116101 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY");

		String hexID = new String(
				"01000000020000000000000000000000000000000000000000000000000000000000000000ffffffff00ffffffffd92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff02ffffffffffffffff000a000000000000000000000000");

		int amt = 10;
		Status result = jniIF.evaluateString(scriptArray, true, -1, hexID, 1, amt);

		Assert.assertEquals("Signature must use SIGHASH_FORKID", result.getStatusMessage());
		Assert.assertEquals(43, result.getStatusCode());

	}

	/* Evaluate with Empty Script */
	@Test
	public void testEvaluateStringEmptyScript() {

		String scriptArray = new String("");

		String hexID = new String(
				"01000000020000000000000000000000000000000000000000000000000000000000000000ffffffff00ffffffffd92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff02ffffffffffffffff000a000000000000000000000000");
		Status result = null;

		int amt = 10;
		try {
			result = jniIF.evaluateString(scriptArray, true, -1, hexID, 1, amt);

		} catch (Exception ex) {
			Assert.assertNull(result);
			Assert.assertEquals("empty script", ex.getMessage());
		}
	}

	/* Evaluate String with Null Script */
	@Test
	public void testEvaluateStringNullScript() {

		String scriptArray = null;

		String hexID = new String(
				"01000000020000000000000000000000000000000000000000000000000000000000000000ffffffff00ffffffffd92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff02ffffffffffffffff000a000000000000000000000000");
		Status result = null;

		int amt = 10;
		try {
			result = jniIF.evaluateString(scriptArray, true, -1, hexID, 1, amt);

		} catch (Exception ex) {
			Assert.assertNull(result);
			Assert.assertEquals("value cannot be null", ex.getMessage());
		}
	}

	/* Evaluate with Empty Script */
	@Test
	public void testEvaluateEmptyScript() {

		byte[] scriptArray = new byte[0];

		String hexID = new String(
				"0100000001000000000000000000000000000000000000000000000000000000000000000000000000085152006384675168000000000000000000");
		Status result = null;

		int amt = 10;
		try {
			result = jniIF.evaluate(scriptArray, true, 0, hexID, 0, amt);

		} catch (Exception ex) {
			Assert.assertNotNull(result);
			Assert.assertEquals("empty script", ex.getMessage());
		}
	}

	/* Evaluate String with Null Script */
	@Test
	public void testEvaluateNullScript() {

		String scriptArray = null;

		String hexID = new String(
				"01000000020000000000000000000000000000000000000000000000000000000000000000ffffffff00ffffffffd92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff02ffffffffffffffff000a000000000000000000000000");
		Status result = null;

		int amt = 10;
		try {
			result = jniIF.evaluateString(scriptArray, true, -1, hexID, 1, amt);

		} catch (Exception ex) {
			Assert.assertNull(result);
			Assert.assertEquals("value cannot be null", ex.getMessage());
		}
	}

	/* Evaluate with Null Transaction value */
	@Test
	public void testEvaluateNullTxn() {

		int[] intArray = new int[] { 0x00, 0x6b, 0x54, 0x55, 0x93, 0x59, 0x87 };
		final byte[] var = new byte[intArray.length];
		for (int i = 0; i < intArray.length; i++) {
			var[i] = (byte) intArray[i];
		}

		String hexID = null;
		Status result = null;

		try {
			result = jniIF.evaluate(var, true, 0, hexID, 0, 0);
		} catch (Exception ex) {
			Assert.assertNull(result);
			Assert.assertEquals("value cannot be null", ex.getMessage());
		}
	}

	@Test
	/* EvaluateString with Null Transaction value */
	public void testEvaluateStringNullTxn() {

		String scriptArray = new String(
				"0x47 0x304402201217437c4be1f4a11f8de3b28fe73c0ceeab54d070eef1b38b29e3e44f17505102200706107eae921558e8e58938017c584e721094ffba5ae86c7202c6c19046116101 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY");

		String hexID = null;

		Status result = null;

		int amt = 10;
		try {
			result = jniIF.evaluateString(scriptArray, true, 33554432, hexID, 1, amt);
		} catch (Exception ex) {
			Assert.assertNull(result);
			Assert.assertEquals("value cannot be null", ex.getMessage());
		}

	}

}
