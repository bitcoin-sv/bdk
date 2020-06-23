package jni.bscriptIF;

public class BScriptJNI {

    private final static int DEBUG = Integer.getInteger("debug", 0);

    static {
        
         if(DEBUG == 1){
            System.loadLibrary("jniBSCriptd");
         }else{
            System.loadLibrary("jniBSCript");
         }
    }

    public native BScriptJNIResult EvalScript(byte[] script,boolean concensus, int scriptflags,String txHex, int nIndex, int amount);
    public native boolean VerifyScript(byte[] script);
    
    public native BScriptJNIResult EvalScriptString(String script,boolean concensus, int scriptflags, String txHex, int nIndex, int amount);
    public native BScriptJNIResult VerifyScriptString(String scriptsig,String scriptpub, boolean concensus, int scriptflags, String txHex, int nIndex, int amount);

}
    
