package jni.bscriptIF;
import java.util.Arrays;


// javac -h . BScriptJNI.java
public class BScriptJNI {

    private final static int DEBUG = Integer.getInteger("debug", 0);

    static {
        
         if(DEBUG == 1){
            System.loadLibrary("jniBSCriptd");
         }else{
            System.loadLibrary("jniBSCript");
         }
    }

    public native boolean EvalScript(byte[] script);
    public native boolean VerifyScript(byte[] script);
    
    public native boolean EvalScriptString(String[] script);
    public native boolean VerifyScriptString(String[] script);
    
}
    
