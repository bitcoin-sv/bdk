package com.nchain.bsv.scriptengine;

import org.junit.Assert;
import org.junit.Test;

public class AssemblerTest{

    public Assembler jniIF = new Assembler();

    @Test
    public void testFromToAsm(){
        String asmInput = "4 5 ADD 9 EQUAL";

        byte[] binaryScript = jniIF.fromAsm(asmInput);
        String asmOutput = jniIF.toAsm(binaryScript);

        Assert.assertEquals(asmInput, asmOutput);
     }

     @Test
     public void testFromToAsmFails(){
        String asmInput = "4 5 OP_ADD 9 EQUAL";

        byte[] binaryScript = jniIF.fromAsm(asmInput);
        String asmOutput = jniIF.toAsm(binaryScript);

        Assert.assertNotEquals(asmInput, asmOutput);
      }

     @Test(expected = RuntimeException.class)
     public void testInvalidScript(){
          String asmInput = "mary had a little lamb";

          byte[] binaryScript = jniIF.fromAsm(asmInput);
    }
}