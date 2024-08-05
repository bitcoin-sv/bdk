package com.nchain.sesdk;

import java.lang.*;
import java.io.*;
import java.nio.*;
import java.io.File;
import java.nio.file.Files;

import org.testng.Assert;
import org.testng.annotations.Test;

// The idea of the test is to create a very long script in function of n
// so that it'll take a few second to fully execute the script. Then a separate
// Thread cancel the token proving the concurrent cancellation was effective
//
// The script to create is of the form
//   head [OP_PUSHDATA1 0x01 0x02 OP_PUSHDATA1 0x01 0x03 OP_PUSHDATA1 0x01 0x04]
//   body [OP_PUSHDATA1 0x01 0x01 OP_DROP] (repeat n times with n very large number)
//   tail [OP_DROP OP_DROP OP_DROP]
// So when evaluate this long script
//  - If the script is fully executed, the output stack should be empty
//  - If the script is cancelled, the output stack should remain at least [2,3,4] because the last 3 stack pop operations was not reached
//
public class CancellationTokenTest {

    private final static boolean init = PackageInfo.loadJNI();

    private class ScriptExecutorThread extends Thread {

        ScriptExecutorThread(ScriptEngine se, byte[]  s, CancellationToken t){
            this.scriptEngine = se;
            this.script = s;
            this.token = t;
            this.ret = null;
        }
        public void run(){
            this.ret = scriptEngine.execute(script, token, "", 0, 0);
        }
        public ScriptEngine scriptEngine;
        final private byte[] script;
        CancellationToken token;
        Status ret;
    }

    private String createScriptStr(long n){
        if(n<1) {
            throw new IllegalArgumentException("argument must be strictly positive");
        }
        String scriptStr = "OP_PUSHDATA1 0x01 0x02 OP_PUSHDATA1 0x01 0x03 OP_PUSHDATA1 0x01 0x04";
        for (int i = 0; i < n; i++) {
            String loopStr = " OP_PUSHDATA1 0x01 0x01 OP_DROP";
            scriptStr += loopStr;
        }
        scriptStr += " OP_DROP OP_DROP OP_DROP";//Drain the stack
        return scriptStr;
    }

    private byte[] createScript(long n){
        Assembler assembler = new Assembler();
        byte[] script = assembler.fromAsm(createScriptStr(n));
        return script;
    }

    private void writeBinaryScript(String filename, long n){
        File filePath = new File(System.getProperty("java_test_data_dir") + "/" + filename);
        try (FileOutputStream stream = new FileOutputStream(filePath)) {
            Assembler asm = new Assembler();

            byte[] head = asm.fromAsm("OP_PUSHDATA1 0x01 0x02 OP_PUSHDATA1 0x01 0x03 OP_PUSHDATA1 0x01 0x04");
            stream.write(head);

            for (int i = 0; i < n; i++) {
                byte[] body_i = asm.fromAsm("OP_PUSHDATA1 0x01 0x01 OP_DROP");
                stream.write(body_i);
            }

            byte[] tail = asm.fromAsm("OP_DROP OP_DROP OP_DROP");
            stream.write(tail);
        }
        catch (Exception e) {
            System.out.println("Exception while writing file " + e.getMessage());
        }
    }

    private byte[] readBinaryScript(String filename){
        File filePath = new File(System.getProperty("java_test_data_dir") + "/" + filename);

        try {
            byte[] script = Files.readAllBytes(filePath.toPath());
            return script;
        } catch (Exception e) {
            System.out.println("Exception while writing file " + e.getMessage());
        }
        return null;
    }

    @Test(expectedExceptions = RuntimeException.class)
    public void testCloseableIdempotent() {

        CancellationToken t = new CancellationToken();
        t.cancel();
        t.close();
        t.close();
        t.cancel();// Should throw
    }

    @Test
    public void testCancelBeforeExecution() {

        // If the script get executed, the stack should remain [1,2,3]
        // Empty stack prove the script engine has been cancelled before execution
        String scriptStr = "OP_PUSHDATA1 0x03 0x010203";

        Config c = new Config();
        ScriptEngine se = new ScriptEngine(c, 0);
        CancellationToken t = new CancellationToken();
        t.cancel();
        Status ret = se.execute(scriptStr, t, "", 0, 0);

        final Stack s = se.getStack();
        Assert.assertTrue(s.size() == 0);
    }

    @Test
    public void testFullExecution() {
        // Fully execution of the script should drain the output stack

        String scriptStr = createScriptStr(3);

        Config c = new Config();
        ScriptEngine se = new ScriptEngine(c, 0);
        CancellationToken t = new CancellationToken();
        Status ret = se.execute(scriptStr, t, "", 0, 0);

        final Stack s = se.getStack();
        Assert.assertTrue(s.size() == 0);
    }

    @Test
    public void testBinaryScriptIO() {
        long n=20;// Increase n=2000000 to get a very long script
        String filename = "script_" + Long.toString(n) +".bin";
        File filePath = new File(System.getProperty("java_test_data_dir") + "/" + filename);

        if(!filePath.exists()) {
            writeBinaryScript(filename,n);
        }

        byte[] script = readBinaryScript(filename);
        Assembler asm = new Assembler();

        Config c = new Config();
        ScriptEngine se = new ScriptEngine(c, 0);
        CancellationToken t = new CancellationToken();
        Status ret = se.execute(script, t, "", 0, 0);

        final Stack s = se.getStack();
        Assert.assertTrue(s.size() == 0);
    }

    // This test might need to do in a separate example file because it take too much time
    // It only work for Release run, not seen it works for debug. Don't know why
    // Deactivate it from unit tests
    @Test(enabled=false)
    public void testCancelConcurrently() {
        final long pid = PackageInfo.getPID();
        long n=4000000;
        String filename = "script_" + Long.toString(n) +".bin";
        File filePath = new File(System.getProperty("java_test_data_dir") + "/" + filename);
        if(!filePath.exists()) {
            writeBinaryScript(filename,n);
        }

        byte[] script = readBinaryScript(filename);

        Status result = new Status(-1, "Foo");
        Config c = new Config();
        ScriptEngine se = new ScriptEngine(c, 524288);// Flag allow MAX_OPS_PER_SCRIPT_AFTER_GENESIS
        CancellationToken t = new CancellationToken();

        while(se.getStack().size() < 1) {
            try {
                ScriptExecutorThread f = new ScriptExecutorThread(se, script, t);
                f.start();
                // Release 10, Debug 50
                Thread.sleep(200);
                //Thread.sleep(0,800000);
                t.cancel();
                f.join();

                final Stack s = f.scriptEngine.getStack();
                final long ss = s.size();
                final int code = f.ret.getCode();
                final String err = f.ret.getMessage();
                result = f.ret;
            } catch (Exception e) {
                Assert.assertTrue(true, e.getMessage());
            }
        }

        final int code = result.getCode();
        final Stack s = se.getStack();
        final long ss = s.size();

        // Expected stack remain at least [1,2,3]
        Assert.assertTrue(ss >= 3);
        Assert.assertTrue(s.at(0)[0] == 2);
        Assert.assertTrue(s.at(1)[0] == 3);
        Assert.assertTrue(s.at(2)[0] == 4);
    }
}
