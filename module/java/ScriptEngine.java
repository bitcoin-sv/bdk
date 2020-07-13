package com.nchain.bsv.scriptengine;

public class ScriptEngine {

    private final static int DEBUG = Integer.getInteger("debug", 0);

    static {
        
         if(DEBUG == 1){
            System.loadLibrary("sesdk_jnid");
         }else{
            System.loadLibrary("sesdk_jni");
         }
    }

    public native Status evaluate(byte[] script,boolean concensus, int scriptflags,String txHex, int index, int amount);
    public native Status evaluateString(String script,boolean concensus, int scriptflags, String txHex, int index, int amount);
}
    
