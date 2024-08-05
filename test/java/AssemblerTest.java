package com.nchain.sesdk;

import org.testng.Assert;
import org.testng.annotations.Test;

public class AssemblerTest {

    public Assembler assembler = new Assembler();

    @Test
    public void testFromToAsm() {
        String asmInput = "4 5 ADD 9 EQUAL";

        byte[] binaryScript = assembler.fromAsm(asmInput);
        String asmOutput = assembler.toAsm(binaryScript);

        Assert.assertEquals(asmInput, asmOutput);  // asm -> script -> asm does not always restore the original asm
    }

    @Test
    public void testFromToAsmFails() {
        String asmInput = "4 5 OP_ADD 9 EQUAL";

        byte[] binaryScript = assembler.fromAsm(asmInput);
        String asmOutput = assembler.toAsm(binaryScript);

        Assert.assertNotEquals(asmInput, asmOutput);   // toAsm(..) drops the OP_ prefix
    }

    @Test(expectedExceptions = RuntimeException.class)
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

