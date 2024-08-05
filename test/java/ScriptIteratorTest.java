package com.nchain.sesdk;

import org.testng.Assert;
import org.testng.annotations.Test;

public class ScriptIteratorTest {

    // Return iterator for script, ex createScriptIterator("4 5 OP_ADD 9 OP_EQUAL")
    private ScriptIterator createScriptIterator(String script_str) {
        final long pid = PackageInfo.getPID();
        Assembler assembler = new Assembler();
        byte[] script = assembler.fromAsm(script_str);

        // Test the script is healthy by execute it
        CancellationToken token = new CancellationToken();
        Config c = new Config();
        ScriptEngine scriptEngine = new ScriptEngine(c, 0);
        Status ret = scriptEngine.execute(script, token, "", 0, 0);
        final int retCode = ret.getCode();
        final String retMsg = ret.getMessage();
        Assert.assertTrue(retCode == 0 || retCode == 3);// No error or error op return
        Assert.assertTrue(retMsg.contains("No error") || retMsg.contains("OP_RETURN"));

        ScriptIterator it = new ScriptIterator(script);
        return it;
    }

    // Return iterator for script, ex createScriptIterator("0x00, 0x6b, 0x54, 0x55,
    // 0x93, 0x59, 0x87")
    private ScriptIterator createScriptIterator(int[] arrayOfOpcodes) {

        final byte[] var = new byte[arrayOfOpcodes.length];
        for (int i = 0; i < arrayOfOpcodes.length; i++) {
            var[i] = (byte) arrayOfOpcodes[i];
        }

        // Test the script is healthy by execute it
        CancellationToken token = new CancellationToken();
        Config c = new Config();
        ScriptEngine scriptEngine = new ScriptEngine(c, 0);
        Status ret = scriptEngine.execute(var, token, "", 0, 0);
        final int retCode = ret.getCode();
        final String retMsg = ret.getMessage();
        Assert.assertTrue(retCode == 0 || retCode == 3);// No error or error op return
        Assert.assertTrue(retMsg.contains("No error") || retMsg.contains("OP_RETURN"));

        ScriptIterator it = new ScriptIterator(var);
        return it;
    }

    @Test(expectedExceptions = RuntimeException.class)
    public void testNullScript() {
        final byte[] arr = null;
        final ScriptIterator it = new ScriptIterator(arr);// Should throw
    }

    @Test
    public void testEmptyScript() {
        final byte[] arr = new byte[0];
        final ScriptIterator it = new ScriptIterator(arr);// Should not throw

        Assert.assertTrue(!it.next());
    }

    @Test
    public void testConstructor() {

        final ScriptIterator it = createScriptIterator("4 5 OP_ADD 9 OP_EQUAL");

        Assert.assertTrue(it.getOpcode() == -1);
        Assert.assertTrue(it.getData() == null);
    }

    @Test
    public void testNext() {

        final long pid = PackageInfo.getPID();

        final ScriptIterator it = createScriptIterator("4 5 OP_ADD 9 OP_EQUAL");
		
        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x54); // OP_4
        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x55); // OP_5
        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x93);// OP_ADD
        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x59); // OP_9
        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x87);// OP_EQUAL
		Assert.assertFalse(it.next());
		
		{
			String assembly = null;		
			try	{
				
				final ScriptIterator it2 = createScriptIterator(assembly);
				Assert.assertTrue(false);
			}
			catch(Exception e){}

			assembly = "";
			final ScriptIterator it3 = createScriptIterator(assembly);
			Assert.assertFalse(it3.next());	
		}

		{
			int[] script = null;
			try	{
				
				final ScriptIterator it4 = createScriptIterator(script);
				Assert.assertTrue(false);
			}
			catch(Exception e){}

			script = new int[] {};
			final ScriptIterator it5 = createScriptIterator(script);
			Assert.assertFalse(it5.next());	
		}
    }

    @Test
    public void testReset() {

        final ScriptIterator it = createScriptIterator("4 5 OP_ADD 9 OP_EQUAL");
        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x54); // OP_4
        it.reset();
        Assert.assertTrue(it.getOpcode() == -1);
    }

    @Test
    public void testOverflow() {

        final ScriptIterator it = createScriptIterator("4 5 OP_ADD 9 OP_EQUAL");

        int nb_opcode = 0;
        while (it.next()) {
            nb_opcode += 1;
        }

        Assert.assertTrue(nb_opcode == 5);
        Assert.assertTrue(!it.next());// exceeded number opcode
    }

    @Test
    public void testScriptWithPushData1() {

        final ScriptIterator it = createScriptIterator("OP_PUSHDATA1 0x01 0x07 7 OP_EQUAL");

        Assert.assertTrue(it.getData() == null);
        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x4c); // OP_PUSHDATA1
        byte[] pushData = it.getData();
        Assert.assertTrue(pushData != null);
        Assert.assertTrue(pushData.length == 1);
        Assert.assertTrue(pushData[0] == 0x07);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x57); // OP_7
        Assert.assertTrue(it.getData() == null);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x87);// OP_EQUAL
    }

    @Test
    public void testScriptWithPushData3() {

        final ScriptIterator it = createScriptIterator("OP_PUSHDATA1 0x03 0x010203 OP_RETURN");

        Assert.assertTrue(it.getData() == null);
        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x4c); // OP_PUSHDATA1
        byte[] pushData = it.getData();
        Assert.assertTrue(pushData != null);
        Assert.assertTrue(pushData.length == 3);
        Assert.assertTrue(pushData[0] == 0x01);
        Assert.assertTrue(pushData[1] == 0x02);
        Assert.assertTrue(pushData[2] == 0x03);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x6a); // OP_RETURN
        Assert.assertTrue(it.getData() == null);
    }

    // Test Scenario : pass script in the form of byte array ex - { 0x00, 0x6b,
    // 0x54, 0x55, 0x93, 0x59, 0x87 }

    @Test
    public void testScriptWithArrayofOpcodes() {

        int[] intArray = new int[] { 0x00, 0x6b, 0x54, 0x55, 0x93, 0x59, 0x87 }; // "OP_0 OP_TOALTSTACK OP_4 OP_5 OP_ADD
                                                                                 // OP_9 OP_EQUAL"

        final ScriptIterator it = createScriptIterator(intArray);

        Assert.assertTrue(it.getData() == null);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x00);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x6b);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x54);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x55);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x93);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x59);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x87);

        Assert.assertFalse(it.next());

        Assert.assertTrue(it.reset());// checking reset function
        Assert.assertTrue(it.getData() == null);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x00);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x6b);
    }

    // Test Scenario : pass valid p2sh script

    @Test
    public void testScriptWithP2SHScript() {

        final ScriptIterator it = createScriptIterator("'abcdef' OP_SIZE 6 OP_EQUAL");

        Assert.assertTrue(it.getData() == null);
        Assert.assertTrue(it.next());
        byte[] data = it.getData();

        Assert.assertTrue(data.length == 6); // length of abcdef
        Assert.assertTrue(data[0] == 'a');
        Assert.assertTrue(data[1] == 'b');
        Assert.assertTrue(data[2] == 'c');
        Assert.assertTrue(data[3] == 'd');
        Assert.assertTrue(data[4] == 'e');
        Assert.assertTrue(data[5] == 'f');

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 130); // OP_SIZE

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x56); // OP_6

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 135); // OP_EQUAL

    }

    // Test Scenario : pass script with disabled opcode ex - 2 0 IF 2MUL ELSE 1
    // ENDIF
    // Note : Expected behaviour : Should throw exception with reason - passed
    // script has disabled opcode.
    // Actual result : process like normal valid script.

    @Test
    public void testScriptWithDisabledOpcode() {

        String disabledOpcodeScript = "2 0 IF 2MUL ELSE 1 ENDIF NOP";
        Assembler assembler = new Assembler();
        byte[] script = assembler.fromAsm(disabledOpcodeScript);

        ScriptIterator it = new ScriptIterator(script);
        Assert.assertTrue(it.getData() == null);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x52);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x00);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x63);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x8d);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x67);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x51);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x68);

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x61);

        Assert.assertFalse(it.next());

    }

    // Test Scenario : pass invalid opcodes ex - 4 5 OP_ADDN 9 OP_EQUAL
    // Note : Expected behaviour : Should throw exception with reason - passed
    // script has invalid opcode.
    // Actual result : fails when exception is not caught in the test with reason -
    // Error
    // parsing script: 4 5 OP_ADDN 9 OP_EQUAL

    @Test(expectedExceptions = RuntimeException.class)
    public void testScriptWithInvalidOpcodes() {

        String invalidScript = "4 5 OP_ADDN 9 OP_EQUAL";
        Assembler assembler = new Assembler();
        byte[] script = assembler.fromAsm(invalidScript);

        ScriptIterator it = new ScriptIterator(script);
        Assert.assertTrue(it.getData() == null);

    }

    // Test Scenario : p2pkh script

    @Test
    public void testScriptWithP2PKH() {

        String p2pkhScript = "'b7978cc96eb13e0865d3f95657561a7f725be952438637475920bac9eb21' OP_DUP OP_HASH160 OP_PUSHDATA1 0x14 0xbef80ecf3a44500fda1bc92176e442891662aed2 OP_EQUAL";
        Assembler assembler = new Assembler();
        byte[] script = assembler.fromAsm(p2pkhScript);

        ScriptIterator it = new ScriptIterator(script);
        Assert.assertTrue(it.getData() == null);
        Assert.assertTrue(it.next());

        byte[] data = it.getData();
        Assert.assertTrue(data.length == 60); // length of 'b7978cc96eb13e0865d3f95657561a7f725be952438637475920bac9eb21' 

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x76); //OP_DUP

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0xa9); //OP_HASH160

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x4c); //OP_PUSHDATA1

        data = it.getData();
        Assert.assertNotNull(data);
        Assert.assertTrue(data.length == 20);  // length of data 0xbef80ecf3a44500fda1bc92176e442891662aed2 - 20 bytes

        Assert.assertTrue(it.next());
        Assert.assertTrue(it.getOpcode() == 0x87); // OP_EQUAL

    }
}

