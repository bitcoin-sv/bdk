package com.nchain.sesdk;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.file.Files;

import com.nchain.sesdk.*;

//The idea of the test is to create a very long script in function of n
//so that it'll take a few second to fully execute the script. Then a separate
//Thread cancels the token proving the concurrent cancellation was effective
//
//The script to create is of the form
//head [OP_PUSHDATA1 0x01 0x02 OP_PUSHDATA1 0x01 0x03 OP_PUSHDATA1 0x01 0x04]
//body [OP_PUSHDATA1 0x01 0x01 OP_DROP] (repeat n times with n very large number)
//tail [OP_DROP OP_DROP OP_DROP]
//So when execute this long script
//- If the script is fully executed, the output stack should be empty
//- If the script is cancelled, the output stack should remain at least [2,3,4] because the last 3 stack pop operations was not reached

// This program allows the user to pass the length for the script 
// java command to execute the test from command line 
// cd to bdk/module/example folder and run the below command
//java -Djava.library.path=path/build/INSTALLATION/lib: -cp path/build/generated/tools/bin/sesdk-0.1.1.jar:path/build/x64/release:. ExampleCancellationToken.java 4000000
//expected result for the above execution : 
//stack size 4
//Test successfully passed since the stack size is 4 and the script execution was cancelled before the completion (script generated is of size 16 MB)

//negative scenario: Execute test with argument - 400000 (script with small length)
// stack size 0
//Test failed since the stack size is 0 (script generated is of size 1.6 MB)


public class ExampleCancellationToken {
    public static void main(String[] args){
        final long pid = PackageInfo.getPID();
        long n = Long.parseLong(args[0]);
        String filename = "script_" + Long.toString(n) +".bin";
        File filePath = new File(System.getProperty("/module/example") + "/" + filename);
        if(!filePath.exists()) {
            writeBinaryScript(filename,n);
        }

        byte[] script = readBinaryScript(filename);

        Status result = new Status(-1, "Foo");
        Config c = new Config();
        ScriptEngine se = new ScriptEngine(c, 524288);// Flag allow MAX_OPS_PER_SCRIPT_AFTER_GENESIS
        CancellationToken t = new CancellationToken();

        try {
            ScriptExecutorThread f = new ScriptExecutorThread(se, script, t);
            f.start();
            // Release 10, Debug 50
            Thread.sleep(200);
            t.cancel();
            f.join();
            System.out.println(" stack size "+se.getStack().size());
            if(se.getStack().size() > 0) {
                System.out.println(" Test successfully passed since the stack size is "+se.getStack().size()+ " and the script execution was cancelled before the completion");
            }else {
                System.out.println(" Test failed since the stack size is "+se.getStack().size());
            }

            final Stack s = f.scriptEngine.getStack();
            final long ss = s.size();
            final int code = f.ret.getCode();
            final String err = f.ret.getMessage();
            result = f.ret;
        } catch (Exception e) {
            System.out.println(e.getMessage());
        }

        final int code = result.getCode();
        final Stack s = se.getStack();
        final long ss = s.size();
    }

    static void writeBinaryScript(String filename, long n){
        File filePath = new File( System.getProperty("user.dir") + "/" + filename);
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

    static byte[] readBinaryScript(String filename){
        File filePath = new File(System.getProperty("user.dir") + "/" + filename);
        try {
            byte[] script = Files.readAllBytes(filePath.toPath());
            return script;
        } catch (Exception e) {
            System.out.println("Exception while writing file " + e.getMessage());
        }
        return null;
    }

    static class ScriptExecutorThread extends Thread {

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


}//End of FirstJavaProgram Class

