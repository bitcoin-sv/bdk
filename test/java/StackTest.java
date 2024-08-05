package com.nchain.sesdk;

import org.testng.Assert;
import org.testng.annotations.Test;

public class StackTest {

    private final static boolean init = PackageInfo.loadJNI();

    private byte[] createScript(String script_str){
        Assembler assembler = new Assembler();
        byte[] script = assembler.fromAsm(script_str);
        return script;
    }

    private ScriptEngine createScriptEngine(){
        Config c = new Config();
        ScriptEngine s = new ScriptEngine(c, 0);
        return s;
    }

    CancellationToken token = new CancellationToken();

    @Test(expectedExceptions = RuntimeException.class)
    public void testCloseableIdempotent() {

        Stack astack = new Stack(128);
        final long v1 = astack.size();
        astack.close();
        astack.close();
        final long v2 = astack.size();// Should throw
    }

    @Test
    public void testSimpleScript() {

        final byte[] script_barr = createScript("4 5 OP_ADD 9 OP_EQUAL");
        ScriptEngine scriptEngine = createScriptEngine();
        Status resultEvaluate = scriptEngine.execute(script_barr, token, "", 0, 0);

        final Stack s = scriptEngine.getStack();
        Assert.assertTrue(s.size() == 1);
        byte[] topstack = s.at(0);

        // OP_EQUAL return true (1) to the stack
        Assert.assertTrue(topstack.length == 1);
        Assert.assertTrue(topstack[0] == 1);
    }

    @Test
    public void testScriptPushData1() {

        final byte[] script_barr = createScript("OP_PUSHDATA1 0x01 0x07 OP_RETURN");
        ScriptEngine scriptEngine = createScriptEngine();
        Status resultEvaluate = scriptEngine.execute(script_barr, token, "", 0, 0);

        final Stack s = scriptEngine.getStack();
        Assert.assertTrue(s.size() == 1);
        byte[] topstack = s.at(0);

        // OP_RETURN return pushed data to the stack
        Assert.assertTrue(topstack.length == 1);
        Assert.assertTrue(topstack[0] == 7);
    }

    @Test
    public void testScriptPushData3() {

        final byte[] script_barr = createScript("OP_PUSHDATA1 0x03 0x010203 OP_RETURN");
        ScriptEngine scriptEngine = createScriptEngine();
        Status resultEvaluate = scriptEngine.execute(script_barr, token, "", 0, 0);

        final Stack s = scriptEngine.getStack();
        Assert.assertTrue(s.size() == 1);
        byte[] topstack = s.at(0);

        // OP_RETURN return pushed data to the stack
        Assert.assertTrue(topstack.length == 3);
        Assert.assertTrue(topstack[0] == 0x01);
        Assert.assertTrue(topstack[1] == 0x02);
        Assert.assertTrue(topstack[2] == 0x03);
    }

    @Test
    public void testScriptPushPushData3() {

        final byte[] script_barr = createScript("OP_PUSHDATA1 0x01 0x01 OP_PUSHDATA1 0x02 0x0203 OP_RETURN");
        ScriptEngine scriptEngine = createScriptEngine();
        Status resultEvaluate = scriptEngine.execute(script_barr, token, "", 0, 0);

        final Stack s = scriptEngine.getStack();
        Assert.assertTrue(s.size() == 2);
        byte[] topstack1 = s.at(0);
        byte[] topstack2 = s.at(1);

        // OP_RETURN return pushed data to the stack
        Assert.assertTrue(topstack1.length == 1);
        Assert.assertTrue(topstack1[0] == 0x01);

        // OP_RETURN return pushed data to the stack
        Assert.assertTrue(topstack2.length == 2);
        Assert.assertTrue(topstack2[0] == 0x02);
        Assert.assertTrue(topstack2[1] == 0x03);
    }

    @Test
    public void testIncrementalEvaluation() {
        // Incrementally execute the script "4 5 OP_ADD 6 OP_MUL", then check the stack state every step
        final String ss1 = "4";
        final String ss2 = "5";
        final String ss3 = "OP_ADD";
        final String ss4 = "6 OP_MUL";

        ScriptEngine scriptEngine = createScriptEngine();
        final Stack s = scriptEngine.getStack();
        Status ret1 = scriptEngine.execute(ss1, token, "", 0, 0);
        Assert.assertTrue(ret1.getCode() == 0);
        Assert.assertTrue(s.size() == 1);
        Assert.assertTrue(s.at(0)[0] == 4); //stack [4]

        Status ret2 = scriptEngine.execute(ss2, token, "", 0, 0);
        Assert.assertTrue(ret2.getCode() == 0);
        Assert.assertTrue(s.size() == 2);
        Assert.assertTrue(s.at(0)[0] == 4);
        Assert.assertTrue(s.at(1)[0] == 5); //stack [4 5]

        Status ret3 = scriptEngine.execute(ss3, token, "", 0, 0);
        Assert.assertTrue(ret3.getCode() == 0);
        Assert.assertTrue(s.size() == 1);
        Assert.assertTrue(s.at(0)[0] == 9); //stack [9]

        Status ret4 = scriptEngine.execute(ss4, token, "", 0, 0);
        Assert.assertTrue(ret4.getCode() == 0);
        Assert.assertTrue(s.size() == 1);
        Assert.assertTrue(s.at(0)[0] == 54); //stack [54]
    }

    // Test incrementally executing a script with branching operators
    @Test
    public void testIncrementalBranching() {

        // Full script "1 OP_IF 6 OP_ELSE 7 OP_ENDIF" partitioned into 2 parts
        final String part1 = "1 OP_IF 6 OP_ELSE";
        final String part2 = "7 OP_ENDIF";

        ScriptEngine scriptEngine = createScriptEngine();
        final Stack s = scriptEngine.getStack();
        Status ret1 = scriptEngine.execute(part1, token, "", 0, 0);
        Assert.assertTrue(s.size() == 1);
        Assert.assertTrue(s.at(0)[0] == 6); //stack [6]

        Status ret2 = scriptEngine.execute(part2, token, "", 0, 0);
        Assert.assertTrue(s.size() == 1);
        Assert.assertTrue(s.at(0)[0] == 6); //stack [6]
    }

    //Test Scenario : Empty Script execution
    @Test
    public void testEmptyScriptExecution() {
        
        final byte[] script_barr = createScript("");
        ScriptEngine scriptEngine = createScriptEngine();

        final Stack s = scriptEngine.getStack();
        final Stack altS = scriptEngine.getAltStack();

        //check stack and altstack size before evaluation
        Assert.assertTrue(s.size() == 0);
        Assert.assertTrue(altS.size() == 0);

        Status resultEvaluate = scriptEngine.execute(script_barr, token, "", 0, 0);

        Assert.assertTrue(s.size() == 0);
        Assert.assertTrue(altS.size() == 0);        
    }
    
    //Test Scenario : Null Script execution
    @Test(expectedExceptions = RuntimeException.class)
    public void testNullScriptExecution() {
        
        final byte[] script_barr = createScript(null);
        ScriptEngine scriptEngine = createScriptEngine();

        final Stack s = scriptEngine.getStack();
        final Stack altS = scriptEngine.getAltStack();

        //check stack and altstack size before evaluation
        Assert.assertTrue(s.size() == 0);
        Assert.assertTrue(altS.size() == 0);

        Status resultEvaluate = scriptEngine.execute(script_barr, token, "", 0, 0);

        Assert.assertTrue(s.size() == 0);
        Assert.assertTrue(altS.size() == 0);        
    }

    //Test Scenario : Script with Successful execution using alt stack
    @Test
    public void testSuccessfulExecutionWithAltStack() {
        
        final byte[] script_barr = createScript("12 0 13 OP_TOALTSTACK OP_DROP OP_FROMALTSTACK ADD 25 OP_EQUAL");
        ScriptEngine scriptEngine = createScriptEngine();

        final Stack s = scriptEngine.getStack();
        final Stack altS = scriptEngine.getAltStack();

        //check stack and altstack size before evaluation
        Assert.assertTrue(s.size() == 0);
        Assert.assertTrue(altS.size() == 0);

        Status resultEvaluate = scriptEngine.execute(script_barr, token, "", 0, 0);

        Assert.assertTrue(s.size() == 1);
        Assert.assertTrue(altS.size() == 0); // no element left in the altstack hence expecting the length to be zero

        byte[] topStack = s.at(0);

        if(resultEvaluate.getMessage().contentEquals("No error")) {            
            //check stack
            Assert.assertTrue(topStack.length == 1);
            Assert.assertFalse(topStack[0] == 0); // when script execution is successful it leaves value other than 0            
        }        
    }

    //Test Scenario : Script with unSuccessful execution using alt stack
    // actual behaviour : leaves the stack empty 
    // expected behaviour : stack should hold 0 value hence stack size should be 1 - this has to be investigated JIRA raised - SDK-137 
    @Test
    public void testUnSuccessfulExecutionWithAltStack() {
        
        final byte[] script_barr = createScript("12 0 12 OP_TOALTSTACK OP_DROP OP_FROMALTSTACK ADD 25 OP_EQUAL");
        ScriptEngine scriptEngine = createScriptEngine();

        final Stack s = scriptEngine.getStack();
        final Stack altS = scriptEngine.getAltStack();

        //check stack and altstack size before evaluation
        Assert.assertTrue(s.size() == 0);
        Assert.assertTrue(altS.size() == 0);

        Status resultEvaluate = scriptEngine.execute(script_barr, token, "", 0, 0);

        Assert.assertTrue(s.size() == 1);
        Assert.assertTrue(altS.size() == 0); //no element left in the altstack hence expecting the length to be zero

        byte[] topStack = s.at(0);

        if(resultEvaluate.getMessage().contentEquals("No error")) {            
            //check stack
            //Assert.assertTrue(topStack.length == 1);
            //Assert.assertTrue(topStack[0] == 0);   // when script execution is unsuccessful it should leave 0 value 
        }        
    }

    //Test Scenario : Script which leaves an element in the alt stack after evaluation
    @Test
    public void testScriptWithElementInAltStack() {
        
        final byte[] script_barr = createScript("12 13 12 OP_TOALTSTACK OP_ADD 25 OP_EQUAL");
        ScriptEngine scriptEngine = createScriptEngine();

        final Stack s = scriptEngine.getStack();
        final Stack altS = scriptEngine.getAltStack();

        //check stack and altstack size before evaluation
        Assert.assertTrue(s.size() == 0);
        Assert.assertTrue(altS.size() == 0);

        Status resultEvaluate = scriptEngine.execute(script_barr, token, "", 0, 0);

        if(resultEvaluate.getMessage().contentEquals("No error")) {            

            Assert.assertTrue(s.size() == 1);
            Assert.assertTrue(altS.size() == 1);

            byte[] topStack = s.at(0);
            byte[] topAltStack = altS.at(0);

            //check stack            
            Assert.assertTrue(topStack.length == 1);
            Assert.assertTrue(topAltStack.length == 1);
            Assert.assertTrue(topStack[0] == 1);
            Assert.assertTrue(topAltStack[0] == 12);
        }        
    }

    //Test Scenario : Script which leaves more that 1 element in the alt stack after evaluation
    //check 
    @Test
    public void testScriptWithElementsInAltStack() {
        
        final byte[] script_barr = createScript("12 13 12 OP_TOALTSTACK 13 OP_TOALTSTACK OP_ADD 25 OP_EQUAL");
        ScriptEngine scriptEngine = createScriptEngine();

        final Stack s = scriptEngine.getStack();
        final Stack altS = scriptEngine.getAltStack();

        //check stack and altstack size before evaluation
        Assert.assertTrue(s.size() == 0);
        Assert.assertTrue(altS.size() == 0);

        Status resultEvaluate = scriptEngine.execute(script_barr, token, "", 0, 0);

        if(resultEvaluate.getMessage().contentEquals("No error")) {
            //check stack and alt stack lengths    
            Assert.assertTrue(s.size() == 1);
            Assert.assertTrue(altS.size() == 2); // length should be 2 in alt stack since we left 2 elements in the script

            Assert.assertTrue(s.at(0).length == 1);
            Assert.assertTrue(altS.at(0).length == 1);
            Assert.assertTrue(altS.at(1).length == 1); 

            //check stack and alt stack values
            Assert.assertTrue(s.at(0)[0] == 1);
            Assert.assertTrue(altS.at(0)[0] == 12); // altstack[12 13]
            Assert.assertTrue(altS.at(1)[0] == 13); // altstack[12 13]
        }        
    }

    //Test Scenario : Incremental script evaluation with alt stack
    //check 
    @Test
    public void testIncrementalEvaluationWithAltStack() {
        
        final String[] script_barr = {"12", "13" , "OP_TOALTSTACK","OP_FROMALTSTACK", "OP_ADD", "25 OP_EQUAL"};
        ScriptEngine scriptEngine = createScriptEngine();

        final Stack s = scriptEngine.getStack();
        final Stack altS = scriptEngine.getAltStack();

        //check stack and altstack size before evaluation
        Assert.assertTrue(s.size() == 0);
        Assert.assertTrue(altS.size() == 0);

        for(int i = 0; i < script_barr.length; i++) {
            Status resultEvaluate = scriptEngine.execute(script_barr[i], token, "", 0, 0);            
            if(i == 0) {
                Assert.assertTrue(resultEvaluate.getCode() == 0);
                Assert.assertTrue(s.size() == 1);
                Assert.assertTrue(s.at(0)[0] == 12); //stack [12]

                Assert.assertTrue(altS.size() == 0);              
            }else if( i == 1 || i == 3) {
                Assert.assertTrue(resultEvaluate.getCode() == 0);
                Assert.assertTrue(s.size() == 2);
                Assert.assertTrue(s.at(0)[0] == 12); //stack [12]
                Assert.assertTrue(s.at(1)[0] == 13); //stack [12 13]

                Assert.assertTrue(altS.size() == 0);  //altstack[]          
            }else if(i == 2) {
                Assert.assertTrue(resultEvaluate.getCode() == 0);
                Assert.assertTrue(s.size() == 1);
                Assert.assertTrue(s.at(0)[0] == 12); //stack [12]

                Assert.assertTrue(altS.size() == 1);
                Assert.assertTrue(altS.at(0)[0] == 13); //altstack [13]
            }else if(i == 4){
                Assert.assertTrue(resultEvaluate.getCode() == 0);
                Assert.assertTrue(s.size() == 1);
                Assert.assertTrue(s.at(0)[0] == 25); //stack [12]

                Assert.assertTrue(altS.size() == 0); // altstack[]
            }else if(i == 5) {
                Assert.assertTrue(resultEvaluate.getCode() == 0);
                Assert.assertTrue(s.size() == 1);
                Assert.assertTrue(s.at(0)[0] == 1); //stack [0x1]

                Assert.assertTrue(altS.size() == 0); //altstack[]
            }
        }
    }
}
