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

	public ScriptEngine scriptEngine = new ScriptEngine();
	public Assembler assembler = new Assembler();

	@Test
	public void testEvaluateScript() {
		int[] intArray = new int[] { 0x00, 0x6b, 0x54, 0x55, 0x93, 0x59, 0x87 };

		final byte[] var = new byte[intArray.length];
		for (int i = 0; i < intArray.length; i++) {
			var[i] = (byte) intArray[i];
		}

		String scriptArray = assembler.toAsm(var);

		// Call evaluate method for var in byte array format
		Status resultEvaluate = scriptEngine.evaluate(var, true, 0, "", 0, 0);

		Assert.assertEquals(0, resultEvaluate.getCode());
		Assert.assertEquals("No error", resultEvaluate.getMessage());

		// Call evaluate method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 0, "", 0, 0);

		Assert.assertEquals(0, resultEvaluateString.getCode());
		Assert.assertEquals("No error", resultEvaluateString.getMessage());

	}

	@Test(expected = IllegalArgumentException.class)
	public void TestNullHexStr() {
		int[] intArray = new int[] { 0x00, 0x6b, 0x54, 0x55, 0x93, 0x59, 0x87 };
		final byte[] var = new byte[intArray.length];
		for (int i = 0; i < intArray.length; i++) {
			var[i] = (byte) intArray[i];
		}

		scriptEngine.evaluate(var, true, 0, null, 0, 0);
	}

	@Test
	public void testEvaluateTestMessage() {
		int[] intArray = new int[] { 0x00, 0x6b, 0x54, 0x55, 0x93, 0x59, 0x87 };
		final byte[] var = new byte[intArray.length];
		for (int i = 0; i < intArray.length; i++) {
			var[i] = (byte) intArray[i];
		}
		String scriptArray = assembler.toAsm(var);

		// Call evaluate method for var in byte array format
		Status resultEvaluate = scriptEngine.evaluate(var, true, 0, "", 0, 0);

		Assert.assertEquals(0, resultEvaluate.getCode());
		Assert.assertEquals("No error", resultEvaluate.getMessage());

		// Call evaluate method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 0, "", 0, 0);

		Assert.assertEquals(0, resultEvaluateString.getCode());
		Assert.assertEquals("No error", resultEvaluateString.getMessage());

	}

	@Test
	public void testEvaluateSimpleScript() {
		String scriptArray = "0x00 0x6b 0x54 0x55 0x93 0x59 0x87";
		byte[] binaryScript = assembler.fromAsm(scriptArray);

		// Call evaluate method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 0, "", 0, 0);

		Assert.assertEquals(0, resultEvaluateString.getCode());
		Assert.assertEquals("No error", resultEvaluateString.getMessage());

		// Call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 0, "", 0, 0);

		Assert.assertEquals(0, resultEvaluate.getCode());
		Assert.assertEquals("No error", resultEvaluate.getMessage());

	}

	@Test
	public void testEvaluateLessSimpleScript() {
		String scriptArray = new String(
				"'abcdefghijklmnopqrstuvwxyz' 0xaa 0x4c 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 0x88");
		byte[] binaryScript = assembler.fromAsm(scriptArray);

		// Call evaluate method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 0, "", 0, 0);

		Assert.assertEquals(0, resultEvaluateString.getCode());
		Assert.assertEquals("No error", resultEvaluateString.getMessage());

		// Call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 0, "", 0, 0);

		Assert.assertEquals(0, resultEvaluate.getCode());
		Assert.assertEquals("No error", resultEvaluate.getMessage());
	}

	@Test
	public void testEvaluateLessSimpleOpCodes() {
		String scriptArray = new String(
				"'abcdefghijklmnopqrstuvwxyz' OP_HASH256 OP_PUSHDATA1 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 OP_EQUALVERIFY");
		byte[] binaryScript = assembler.fromAsm(scriptArray);

		// Call evaluate method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 0, "", 0, 0);

		Assert.assertEquals(0, resultEvaluateString.getCode());
		Assert.assertEquals("No error", resultEvaluateString.getMessage());

		// Call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 0, "", 0, 0);

		Assert.assertEquals(0, resultEvaluate.getCode());
		Assert.assertEquals("No error", resultEvaluate.getMessage());
	}

	@Test
	public void testEvaluateWithTxData() {
		String scriptArray = new String(
				"0x47 0x304402201fcefdc44242241964b5ca81a7e480364364b11a7f5a90163c42c0db3f3886140220387c073f39d63f60deb93b7935a84b93eb498fc12fbe3d65551b905fc360637b01 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY");

		byte[] binaryScript = assembler.fromAsm(scriptArray);

		String HexID = new String(
				"0100000001d92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff010a000000000000000000000000");

		int amt = 10;

		// Call evaluate method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 0, HexID, 0, amt);

		Assert.assertEquals(0, resultEvaluateString.getCode());
		Assert.assertEquals("No error", resultEvaluateString.getMessage());

		// Call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 0, HexID, 0, amt);

		Assert.assertEquals(0, resultEvaluate.getCode());
		Assert.assertEquals("No error", resultEvaluate.getMessage());

	}

	/* Test Cases for JIRA-BS-94 */
	/* Successful Transaction */
	@Test
	public void testEvaluateValidTxn() {
		String scriptArray = new String(
				"0x47 0x30440220172f38b9fca6c5f7f8c06dc134c0856d90311232399efb0e6b10fd86ab46c8960220782578acd276e140b6098ab365745828a48d54d9a44d67cbdb287b76fe6fc08001 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY RETURN 0x4c64 0x03030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303");

		byte[] binaryScript = assembler.fromAsm(scriptArray);

		String hexID = new String(
				"01000000017f7b57604038e4556debe8b1ec64531d3a84bfb4f81cb4faf9fd9939873da5350000000000ffffffff010a000000000000000000000000");

		int amt = 10;

		// call evaluateString method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 524288, hexID, 0, amt);

		Assert.assertEquals("No error", resultEvaluateString.getMessage());
		Assert.assertEquals(0, resultEvaluateString.getCode());

		// call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 524288, hexID, 0, amt);

		Assert.assertEquals("No error", resultEvaluate.getMessage());
		Assert.assertEquals(0, resultEvaluate.getCode());

	}

	/*
	 * Invalid Transaction Above successful Transaction with invalid hexID parameter
	 */
	@Test
	public void testEvaluateInvalidTxn() {
		String scriptArray = new String(
				"0x47 0x30440220172f38b9fca6c5f7f8c06dc134c0856d90311232399efb0e6b10fd86ab46c8960220782578acd276e140b6098ab365745828a48d54d9a44d67cbdb287b76fe6fc08001 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY RETURN 0x4c64 0x03030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303");
		byte[] binaryScript = assembler.fromAsm(scriptArray);
		String hexID = new String(
				"09000000017f7b57604038e4556debe8b1ec64531d3a84bfb4f81cb4faf9fd9939873da5350000000000ffffffff010a000000000000000000000000");

		int amt = 10;

		// call evaluateString method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 524288, hexID, 0, amt);

		Assert.assertEquals("Script failed an OP_CHECKSIGVERIFY operation", resultEvaluateString.getMessage());
		Assert.assertEquals(19, resultEvaluateString.getCode());

		// call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 524288, hexID, 0, amt);

		Assert.assertEquals("Script failed an OP_CHECKSIGVERIFY operation", resultEvaluate.getMessage());
		Assert.assertEquals(19, resultEvaluate.getCode());

	}

	/*
	 * Bad Signature Above successful Transaction with Bad Signature
	 */
	@Test
	public void testEvaluateBadSignature() {
		/* Bad Signature */
		String scriptArray = new String(
				"0x47 0x304402205d1c873a5e115043534d4bdd06762fbb67989f30493c24be531688f655ea25250220558d14f2eef3ff5de1798372afef83a9dd7d401f81f5773443f9b48c46ce187c01 0x41 0x090b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIG RETURN 0x4c64 0x03030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303");
		byte[] binaryScript = assembler.fromAsm(scriptArray);
		String hexID = new String(
				"0100000001ca8e5e634222ad1dce6d4286af725cf97e31ff98922db1a467d38642a7e2cbe60000000000ffffffff010a000000000000000000000000");

		int amt = 10;

		// call evaluateString method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 524288, hexID, 0, amt);

		Assert.assertEquals("Script failed an OP_EQUALVERIFY operation", resultEvaluateString.getMessage());
		Assert.assertEquals(17, resultEvaluateString.getCode());

		// call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 524288, hexID, 0, amt);

		Assert.assertEquals("Script failed an OP_EQUALVERIFY operation", resultEvaluate.getMessage());
		Assert.assertEquals(17, resultEvaluate.getCode());

	}

	/*
	 * Bad Pubkey Above successful Transaction with Bad PubKey
	 */
	@Test
	public void testEvaluateBadPubKey() {

		/* Bad PubKey */
		String scriptArray = new String(
				"0x47 0x80440220172f38b9fca6c5f7f8c06dc134c0856d90311232399efb0e6b10fd86ab46c8960220782578acd276e140b6098ab365745828a48d54d9a44d67cbdb287b76fe6fc08001 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY RETURN 0x4c64 0x03030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303");
		byte[] binaryScript = assembler.fromAsm(scriptArray);
		String hexID = new String(
				"01000000017f7b57604038e4556debe8b1ec64531d3a84bfb4f81cb4faf9fd9939873da5350000000000ffffffff010a000000000000000000000000");

		int amt = 10;

		// call evaluateString method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 524288, hexID, 0, amt);

		Assert.assertEquals("Script failed an OP_CHECKSIGVERIFY operation", resultEvaluateString.getMessage());
		Assert.assertEquals(19, resultEvaluateString.getCode());

		// call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 524288, hexID, 0, amt);

		Assert.assertEquals("Script failed an OP_CHECKSIGVERIFY operation", resultEvaluate.getMessage());
		Assert.assertEquals(19, resultEvaluate.getCode());

	}

	/* Test Cases for JIRA-BS-95 */
	/* Transaction with multiple inputs uses the 2nd input index = 1 */
	@Test
	public void testEvaluateMultipleInputs() {

		String scriptArray = new String(
				"0x47 0x304402201217437c4be1f4a11f8de3b28fe73c0ceeab54d070eef1b38b29e3e44f17505102200706107eae921558e8e58938017c584e721094ffba5ae86c7202c6c19046116101 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY");
		byte[] binaryScript = assembler.fromAsm(scriptArray);
		String hexID = new String(
				"01000000020000000000000000000000000000000000000000000000000000000000000000ffffffff00ffffffffd92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff02ffffffffffffffff000a000000000000000000000000");

		int amt = 10;

		// call evaluateString method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 0, hexID, 1, amt);

		Assert.assertEquals("No error", resultEvaluateString.getMessage());
		Assert.assertEquals(0, resultEvaluateString.getCode());

		// call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 0, hexID, 1, amt);

		Assert.assertEquals("No error", resultEvaluate.getMessage());
		Assert.assertEquals(0, resultEvaluate.getCode());

	}

	/*
	 * Transaction with multiple inputs uses the 2nd input index = 1, here index = 5
	 * is passed to check negative scenario
	 */
	@Test
	public void testEvaluateIncorrectIndex() {

		String scriptArray = new String(
				"0x47 0x304402201217437c4be1f4a11f8de3b28fe73c0ceeab54d070eef1b38b29e3e44f17505102200706107eae921558e8e58938017c584e721094ffba5ae86c7202c6c19046116101 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY");

		byte[] binaryScript = assembler.fromAsm(scriptArray);

		String hexID = new String(
				"01000000020000000000000000000000000000000000000000000000000000000000000000ffffffff00ffffffffd92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff02ffffffffffffffff000a000000000000000000000000");

		int amt = 10;

		// call evaluateString method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 0, hexID, 5, amt);

		Assert.assertEquals("Script failed an OP_CHECKSIGVERIFY operation", resultEvaluateString.getMessage());
		Assert.assertEquals(19, resultEvaluateString.getCode());

		// call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 0, hexID, 5, amt);

		Assert.assertEquals("Script failed an OP_CHECKSIGVERIFY operation", resultEvaluate.getMessage());
		Assert.assertEquals(19, resultEvaluate.getCode());

	}

	@Test
	public void testEvaluateInvalidTestFile() throws IOException {
		List<List<String>> tests = readCsvFile("invalid_tests.csv");

		tests.forEach(t -> {
			String scriptArray = t.get(0);
			byte[] binaryScript = new byte[0];
			try {
				binaryScript = assembler.fromAsm(scriptArray);
			} catch (Exception ex) {
				Assert.assertTrue(ex.getMessage().contains("Error parsing script"));
			}
			int flag = Integer.valueOf(t.get(1));
			String txHex = t.get(2);

			Status resultEvaluateString = null;
			Status resultEvaluate = null;
			// Call evaluateString method for scriptArray in string format
			try {
				resultEvaluateString = scriptEngine.evaluate(scriptArray, true, flag, txHex, 0, 0);
			} catch (Exception ex) {
				return;
			}

			Assert.assertNotEquals(resultEvaluateString.getCode(), 0);

			// Call evaluate method for binaryScript in byte array format
			try {
				resultEvaluate = scriptEngine.evaluate(binaryScript, true, flag, txHex, 0, 0);
			} catch (Exception ex) {
				return;
			}

			Assert.assertNotEquals(resultEvaluate.getCode(), 0);

		});
	}

	@Test
	public void testEvaluateValidTestFile() throws IOException {
		List<List<String>> tests = readCsvFile("valid_tests.csv");

		tests.forEach(t -> {
			String scriptArray = t.get(0);
			byte[] binaryScript = assembler.fromAsm(scriptArray);
			int flag = Integer.valueOf(t.get(1));
			String txHex = t.get(2);

			// Call evaluateString method for scriptArray in string format
			Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, flag, txHex, 0, 0);

			Assert.assertEquals(resultEvaluateString.getCode(), 0);

			// Call evaluate method for binaryScript in byte array format
			Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, flag, txHex, 0, 0);

			Assert.assertEquals(resultEvaluate.getCode(), 0);

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
	public void testEvaluateNegativeIndex() {

		String scriptArray = new String(
				"0x47 0x304402201217437c4be1f4a11f8de3b28fe73c0ceeab54d070eef1b38b29e3e44f17505102200706107eae921558e8e58938017c584e721094ffba5ae86c7202c6c19046116101 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY");

		byte[] binaryScript = assembler.fromAsm(scriptArray);

		String hexID = new String(
				"01000000020000000000000000000000000000000000000000000000000000000000000000ffffffff00ffffffffd92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff02ffffffffffffffff000a000000000000000000000000");

		int amt = 10;

		// call evaluateString method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 0, hexID, -2, amt);

		Assert.assertEquals("Script failed an OP_CHECKSIGVERIFY operation", resultEvaluateString.getMessage());
		Assert.assertEquals(19, resultEvaluateString.getCode());

		// call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 0, hexID, -2, amt);

		Assert.assertEquals("Script failed an OP_CHECKSIGVERIFY operation", resultEvaluate.getMessage());
		Assert.assertEquals(19, resultEvaluate.getCode());

	}

	/* consensus flag = false */
	/* There is no change when flag=true or false for the moment */
	@Test
	public void testEvaluateConsensusFlag() {

		String scriptArray = new String(
				"0x47 0x304402201217437c4be1f4a11f8de3b28fe73c0ceeab54d070eef1b38b29e3e44f17505102200706107eae921558e8e58938017c584e721094ffba5ae86c7202c6c19046116101 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY");

		byte[] binaryScript = assembler.fromAsm(scriptArray);

		String hexID = new String(
				"01000000020000000000000000000000000000000000000000000000000000000000000000ffffffff00ffffffffd92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff02ffffffffffffffff000a000000000000000000000000");

		int amt = 10;

		// call evaluateString method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, false, 0, hexID, 1, amt);

		Assert.assertEquals("No error", resultEvaluateString.getMessage());
		Assert.assertEquals(0, resultEvaluateString.getCode());

		// call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, false, 0, hexID, 1, amt);

		Assert.assertEquals("No error", resultEvaluate.getMessage());
		Assert.assertEquals(0, resultEvaluate.getCode());

	}

	/* Incorrect Script Flag */
	/*
	 * valid script flag but does not work for OP_RETURN transaction. Pass
	 * scriptflag = 0 instead of correct script flag 524288
	 */
	@Test
	public void testEvaluateIncorrectScriptFlag() {

		String scriptArray = new String(
				"0x47 0x30440220172f38b9fca6c5f7f8c06dc134c0856d90311232399efb0e6b10fd86ab46c8960220782578acd276e140b6098ab365745828a48d54d9a44d67cbdb287b76fe6fc08001 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY RETURN 0x4c64 0x03030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303");

		byte[] binaryScript = assembler.fromAsm(scriptArray);

		String hexID = new String(
				"01000000017f7b57604038e4556debe8b1ec64531d3a84bfb4f81cb4faf9fd9939873da5350000000000ffffffff010a000000000000000000000000");

		int amt = 10;

		// call evaluateString method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 0, hexID, 0, amt);

		Assert.assertEquals("OP_RETURN was encountered", resultEvaluateString.getMessage());
		Assert.assertEquals(3, resultEvaluateString.getCode());

		// call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 0, hexID, 0, amt);

		Assert.assertEquals("OP_RETURN was encountered", resultEvaluate.getMessage());
		Assert.assertEquals(3, resultEvaluate.getCode());

	}

	/* Negative value Script Flag */
	@Test
	public void testEvaluateNegativeScriptFlag() {

		String scriptArray = new String(
				"0x47 0x304402201217437c4be1f4a11f8de3b28fe73c0ceeab54d070eef1b38b29e3e44f17505102200706107eae921558e8e58938017c584e721094ffba5ae86c7202c6c19046116101 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY");

		byte[] binaryScript = assembler.fromAsm(scriptArray);

		String hexID = new String(
				"01000000020000000000000000000000000000000000000000000000000000000000000000ffffffff00ffffffffd92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff02ffffffffffffffff000a000000000000000000000000");

		int amt = 10;

		// call evaluateString method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, -1, hexID, 1, amt);

		Assert.assertEquals("Signature must use SIGHASH_FORKID", resultEvaluateString.getMessage());
		Assert.assertEquals(43, resultEvaluateString.getCode());

		// call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, -1, hexID, 1, amt);

		Assert.assertEquals("Signature must use SIGHASH_FORKID", resultEvaluate.getMessage());
		Assert.assertEquals(43, resultEvaluate.getCode());

	}

	/* EvaluateString with Empty Script */
	@Test
	public void testEvaluateStringEmptyScript() {

		String scriptArray = new String("");

		String hexID = new String(
				"01000000020000000000000000000000000000000000000000000000000000000000000000ffffffff00ffffffffd92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff02ffffffffffffffff000a000000000000000000000000");
		Status result = null;

		int amt = 10;
		try {
			result = scriptEngine.evaluate(scriptArray, true, 0, hexID, 1, amt);

		} catch (Exception ex) {
			Assert.assertNull(result);
			Assert.assertEquals("empty script", ex.getMessage());
		}
	}

	/* EvaluateString with Null Script */
	@Test
	public void testEvaluateStringNullScript() {

		String scriptArray = null;

		String hexID = new String(
				"01000000020000000000000000000000000000000000000000000000000000000000000000ffffffff00ffffffffd92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff02ffffffffffffffff000a000000000000000000000000");
		Status result = null;

		int amt = 10;
		try {
			result = scriptEngine.evaluate(scriptArray, true, 0, hexID, 1, amt);

		} catch (Exception ex) {
			Assert.assertNull(result);
			Assert.assertEquals("value cannot be null", ex.getMessage());
		}
	}

	/* Evaluate with Empty Script */
	@Test
	public void testEvaluateEmptyScript() {

		byte[] binaryScript = new byte[0];

		String hexID = new String(
				"01000000020000000000000000000000000000000000000000000000000000000000000000ffffffff00ffffffffd92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff02ffffffffffffffff000a000000000000000000000000");
		Status result = null;

		int amt = 10;
		try {
			result = scriptEngine.evaluate(binaryScript, true, 0, hexID, 1, amt);

		} catch (Exception ex) {
			Assert.assertNotNull(result);
			Assert.assertEquals("empty script", ex.getMessage());
		}
	}

	/* Evaluate with Null Script */
	@Test
	public void testEvaluateNullScript() {

		byte[] binaryScript = null;

		String hexID = new String(
				"01000000020000000000000000000000000000000000000000000000000000000000000000ffffffff00ffffffffd92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff02ffffffffffffffff000a000000000000000000000000");
		Status result = null;

		int amt = 10;
		try {
			result = scriptEngine.evaluate(binaryScript, true, 0, hexID, 1, amt);

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
			result = scriptEngine.evaluate(var, true, 0, hexID, 0, 0);
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
			result = scriptEngine.evaluate(scriptArray, true, 0, hexID, 1, amt);
		} catch (Exception ex) {
			Assert.assertNull(result);
			Assert.assertEquals("value cannot be null", ex.getMessage());
		}

	}

	/*
	 * Test cases for JIRA BS-89 Successful 2 of 3 MultiSig Transaction
	 */
	@Test
	public void testEvaluate2of3tMultiSigWithTxData() {

		String scriptArray = new String(
				"0 0x48 0x3045022100b41305eb272560e18a0ca0e176fbd041b0a0327192049ad5f004fc4eab63fa0302203a61afe95053f7d31497255385d51605df982e0c2d167708072228a908d8b43601 0x48 0x30450221009d6e7052246947cd424a57ac2d1ea8f093b9894b6029cf0a9fb25828f50f309c02202964744834a47eac0cb255870a4755900c07ae09e4e00972f572cd166b38340b01 2 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 0x41 0x04183905ae25e815634ce7f5d9bedbaa2c39032ab98c75b5e88fe43f8dd8246f3c5473ccd4ab475e6a9e6620b52f5ce2fd15a2de32cbe905154b3a05844af70785 0x21 0x030b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a744 3 CHECKMULTISIG");

		byte[] binaryScript = assembler.fromAsm(scriptArray);

		String hexID = new String(
				"01000000019a73e72f65bbb2db0202d3b198af9ee587ef50bf060ebf24448985ecc2688ed20000000000ffffffff010a000000000000000000000000");

		int amt = 1;

		// call evaluateString method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 0, hexID, 0, amt);

		Assert.assertEquals(0, resultEvaluateString.getCode());
		Assert.assertEquals("No error", resultEvaluateString.getMessage());

		// call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 0, hexID, 0, amt);

		Assert.assertEquals(0, resultEvaluate.getCode());
		Assert.assertEquals("No error", resultEvaluate.getMessage());

	}

	/*
	 * Same as above transaction with one missing signature - Negative scenario
	 */
	@Test
	public void testEvaluate2of3InvalidMultiSigWithTxData() {

		String scriptArray = new String(
				"0 0x48 0x30450221009d6e7052246947cd424a57ac2d1ea8f093b9894b6029cf0a9fb25828f50f309c02202964744834a47eac0cb255870a4755900c07ae09e4e00972f572cd166b38340b01 2 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 0x41 0x04183905ae25e815634ce7f5d9bedbaa2c39032ab98c75b5e88fe43f8dd8246f3c5473ccd4ab475e6a9e6620b52f5ce2fd15a2de32cbe905154b3a05844af70785 0x21 0x030b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a744 3 CHECKMULTISIG");

		byte[] binaryScript = assembler.fromAsm(scriptArray);

		String hexID = new String(
				"01000000019a73e72f65bbb2db0202d3b198af9ee587ef50bf060ebf24448985ecc2688ed20000000000ffffffff010a000000000000000000000000");

		int amt = 1;

		// call evaluateString method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 0, hexID, 0, amt);

		Assert.assertEquals(23, resultEvaluateString.getCode());
		Assert.assertEquals("Operation not valid with the current stack size", resultEvaluateString.getMessage());

		// call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 0, hexID, 0, amt);

		Assert.assertEquals(23, resultEvaluate.getCode());
		Assert.assertEquals("Operation not valid with the current stack size", resultEvaluate.getMessage());

	}

	/*
	 * P2PK
	 */
	@Test
	public void testEvaluateP2PKWithTxData() {

		String scriptArray = new String(
				"0x47 0x304402200a5c6163f07b8d3b013c4d1d6dba25e780b39658d79ba37af7057a3b7f15ffa102201fd9b4eaa9943f734928b99a83592c2e7bf342ea2680f6a2bb705167966b742001 0x41 0x0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8 CHECKSIG");

		byte[] binaryScript = assembler.fromAsm(scriptArray);

		String hexID = new String(
				"01000000010000000000000000000000000000000000000000000000000000000000000000000000004847304402200a5c6163f07b8d3b013c4d1d6dba25e780b39658d79ba37af7057a3b7f15ffa102201fd9b4eaa9943f734928b99a83592c2e7bf342ea2680f6a2bb705167966b742001000000000000000000");

		// call evaluateString method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 0, hexID, 0, 0);

		Assert.assertEquals(0, resultEvaluateString.getCode());
		Assert.assertEquals("No error", resultEvaluateString.getMessage());

		// call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 0, hexID, 0, 0);

		Assert.assertEquals(0, resultEvaluate.getCode());
		Assert.assertEquals("No error", resultEvaluate.getMessage());

	}

	/*
	 * P2PK Missing Signature
	 */
	@Test
	public void testEvaluateInvalidP2PKWithTxData() {

		String scriptArray = new String(
				"0x41 0x0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8 CHECKSIG");

		byte[] binaryScript = assembler.fromAsm(scriptArray);

		String hexID = new String(
				"01000000010000000000000000000000000000000000000000000000000000000000000000000000004847304402200a5c6163f07b8d3b013c4d1d6dba25e780b39658d79ba37af7057a3b7f15ffa102201fd9b4eaa9943f734928b99a83592c2e7bf342ea2680f6a2bb705167966b742001000000000000000000");

		// call evaluateString method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 0, hexID, 0, 0);

		Assert.assertEquals(23, resultEvaluateString.getCode());
		Assert.assertEquals("Operation not valid with the current stack size", resultEvaluateString.getMessage());

		// call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 0, hexID, 0, 0);

		Assert.assertEquals(23, resultEvaluate.getCode());
		Assert.assertEquals("Operation not valid with the current stack size", resultEvaluate.getMessage());

	}

	/*
	 * P2PKH
	 */
	@Test
	public void testEvaluateP2PKHWithTxData() {

		String scriptArray = new String(
				"0x47 0x304402206e05a6fe23c59196ffe176c9ddc31e73a9885638f9d1328d47c0c703863b8876022076feb53811aa5b04e0e79f938eb19906cc5e67548bc555a8e8b8b0fc603d840c01 0x21 0x038282263212c609d9ea2a6e3e172de238d8c39cabd5ac1ca10646e23fd5f51508 DUP HASH160 0x14 0x1018853670f9f3b0582c5b9ee8ce93764ac32b93 EQUALVERIFY CHECKSIG");

		byte[] binaryScript = assembler.fromAsm(scriptArray);

		String hexID = new String(
				"01000000010000000000000000000000000000000000000000000000000000000000000000000000006a47304402206e05a6fe23c59196ffe176c9ddc31e73a9885638f9d1328d47c0c703863b8876022076feb53811aa5b04e0e79f938eb19906cc5e67548bc555a8e8b8b0fc603d840c0121038282263212c609d9ea2a6e3e172de238d8c39cabd5ac1ca10646e23fd5f51508000000000000000000");

		// call evaluateString method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 0, hexID, 0, 0);

		Assert.assertEquals(0, resultEvaluateString.getCode());
		Assert.assertEquals("No error", resultEvaluateString.getMessage());

		// call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 0, hexID, 0, 0);

		Assert.assertEquals(0, resultEvaluate.getCode());
		Assert.assertEquals("No error", resultEvaluate.getMessage());

	}

	/*
	 * P2PKH Bad Pub key
	 */
	@Test
	public void testEvaluateInvalidP2PKHWithTxData() {

		String scriptArray = new String(
				"0x47 0x3044022034bb0494b50b8ef130e2185bb220265b9284ef5b4b8a8da4d8415df489c83b5102206259a26d9cc0a125ac26af6153b17c02956855ebe1467412f066e402f5f05d1201 0x21 0x03363d90d446b00c9c99ceac05b6262ee053441c7e55552ffe526bad8f83ff4640 DUP HASH160 0x14 0xc0834c0c158f53be706d234c38fd52de7eece656 EQUALVERIFY CHECKSIG");

		byte[] binaryScript = assembler.fromAsm(scriptArray);

		String hexID = new String(
				"01000000010000000000000000000000000000000000000000000000000000000000000000000000006a473044022034bb0494b50b8ef130e2185bb220265b9284ef5b4b8a8da4d8415df489c83b5102206259a26d9cc0a125ac26af6153b17c02956855ebe1467412f066e402f5f05d12012103363d90d446b00c9c99ceac05b6262ee053441c7e55552ffe526bad8f83ff4640000000000000000000");

		// call evaluateString method for scriptArray in string format
		Status resultEvaluateString = scriptEngine.evaluate(scriptArray, true, 0, hexID, 0, 0);

		Assert.assertEquals(17, resultEvaluateString.getCode());
		Assert.assertEquals("Script failed an OP_EQUALVERIFY operation", resultEvaluateString.getMessage());

		// call evaluate method for binaryScript in byte array format
		Status resultEvaluate = scriptEngine.evaluate(binaryScript, true, 0, hexID, 0, 0);

		Assert.assertEquals(17, resultEvaluate.getCode());
		Assert.assertEquals("Script failed an OP_EQUALVERIFY operation", resultEvaluate.getMessage());

	}

}
