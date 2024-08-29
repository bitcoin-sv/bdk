package com.nchain.bdk;

public class ScriptIterator {

    private final static boolean init = PackageInfo.loadJNI();

    public native void helpctor(byte[] script);

    public ScriptIterator(byte[] script)
    {
        this.numOpcodes=0;
        this.scriptBinaryArray=null;
        this.pushData=null;
        this.posOpcode=-1;
        this.opcode=-1;

        helpctor(script);
    }

    public int getOpcode()
    {
        return opcode;
    }

    public byte[] getData()
    {
        return pushData;
    }

    public native boolean next();
    public native boolean reset();

    private int numOpcodes;
    private byte[] scriptBinaryArray;
    private byte[] pushData;

    private int posOpcode;
    private int opcode;
}
    
