package com.nchain.bsv.scriptengine;

import org.junit.Assert;
import org.junit.Test;

public class AssemblerTest {

	public Assembler assembler = new Assembler();

	@Test
	public void testFromToAsm() {
		String asmInput = "4 5 ADD 9 EQUAL";

		byte[] binaryScript = assembler.fromAsm(asmInput);
		String asmOutput = assembler.toAsm(binaryScript);

		Assert.assertEquals(asmInput, asmOutput);
	}

	@Test
    public void testToAsmString() {
    	 String asmInput = "4 5 0x93 9 0x87";
    	 String expectedAsmOutput = "4 5 ADD 9 EQUAL";

    	 String asmOutput = assembler.toAsm(asmInput);

    	 Assert.assertEquals(expectedAsmOutput, asmOutput);
    }


	@Test
	public void testFromToAsmFails() {
		String asmInput = "4 5 OP_ADD 9 EQUAL";

		byte[] binaryScript = assembler.fromAsm(asmInput);
		String asmOutput = assembler.toAsm(binaryScript);

		Assert.assertNotEquals(asmInput, asmOutput);
	}

	@Test(expected = RuntimeException.class)
	public void testInvalidScript() {
		String asmInput = "mary had a little lamb";

		byte[] binaryScript = assembler.fromAsm(asmInput);

	}

	@Test
	public void testFromToAsmEmptyInput() {
		String asmInput = "";
		String asmOutput = null;

		try {

			byte[] binaryScript = assembler.fromAsm(asmInput);
			asmOutput = assembler.toAsm(binaryScript);
		} catch (Exception ex) {
			Assert.assertEquals("value cannot be null", ex.getMessage());
		}

		Assert.assertEquals(asmInput, asmOutput);
	}

	@Test
	public void testFromToAsmNullInput() {
		String asmInput = null;
		String asmOutput = null;

		try {
			byte[] binaryScript = assembler.fromAsm(asmInput);
			asmOutput = assembler.toAsm(binaryScript);
		} catch (Exception ex) {
			Assert.assertEquals("value cannot be null", ex.getMessage());
		}

		Assert.assertEquals(asmInput, asmOutput);
	}
}

