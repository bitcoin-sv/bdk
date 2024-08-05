package com.nchain.sesdk;

public class CancellationToken implements AutoCloseable {

    private final static boolean init = PackageInfo.loadJNI();

    public CancellationToken(){
        initCppToken();
    }

    @Override
    public void close() {
        deleteCppToken();
    }

    public native void cancel();

    private native void initCppToken();
    private native void deleteCppToken();
    private long cppToken; // pointer holding cpp CancellationToken object
}
    
