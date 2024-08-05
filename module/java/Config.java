package com.nchain.sesdk;

public class Config implements AutoCloseable {

    private static boolean init = false;

    public Config(){
        this(null);
    }

    public Config(boolean g, boolean c){
        this(g, c, null);
    }

    public Config(String libraryPath) {
        if(!init) {
            init = PackageInfo.loadJNI(libraryPath);
        }
        this.isGenesisEnabled = true ;
        this.isConsensus      = true ;
        initCppConfig();
    }

    public Config(boolean g, boolean c, String libraryPath) {
        if(!init) {
            init = PackageInfo.loadJNI(libraryPath);
        }
        this.isGenesisEnabled = g ;
        this.isConsensus      = c ;
        initCppConfig();
    }

    @Override
    public void close() {
        deleteCppConfig();
    }

    public native void load(String filename);

	// Interface used by script engine and client code to retrieve limit information
    public native long getMaxOpsPerScript();
    public native long getMaxScriptNumLength();
    public native long getMaxScriptSize();
    public native long getMaxPubKeysPerMultiSig();
    public native long getMaxStackMemoryUsage();


    public native void setMaxOpsPerScriptPolicy(long v);
    public native void setMaxScriptnumLengthPolicy(long v);
    public native void setMaxScriptSizePolicy(long v);
    public native void setMaxPubkeysPerMultisigPolicy(long v);
    public native void setMaxStackMemoryUsage(long v1, long v2);

    public boolean isGenesisEnabled;
    public boolean isConsensus;

    private native void initCppConfig();
    private native void deleteCppConfig();
    private long cppConfig; // pointer holding cpp Config object
}
    
