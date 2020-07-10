package com.nchain.bsv.scriptengine;

public class Assembler {

    private final static int DEBUG = Integer.getInteger("debug", 0);

    static {
         if(DEBUG == 1){
            System.loadLibrary("jniBSCriptd");
         }else{
            System.loadLibrary("jniBSCript");
         }
    }

    public native byte[] fromAsm(String script);
    public native String toAsm(byte[] script);
}

