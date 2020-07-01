package com.nchain.bsv.scriptengine;

public class ScriptEngine {

    private final static int DEBUG = Integer.getInteger("debug", 0);

    static {
        
         if(DEBUG == 1){
            System.loadLibrary("jniBSCriptd");
         }else{
            System.loadLibrary("jniBSCript");
         }
    }

    public native Status Evaluate(byte[] script,boolean concensus, int scriptflags,String txHex, int nIndex, int amount);
    public native boolean Verify(byte[] script);
    
    public native Status EvaluateString(String script,boolean concensus, int scriptflags, String txHex, int nIndex, int amount);
    public native Status VerifyString(String scriptsig,String scriptpub, boolean concensus, int scriptflags, String txHex, int nIndex, int amount);

}
    
