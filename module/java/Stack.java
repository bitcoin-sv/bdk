package com.nchain.bdk;

public class Stack implements AutoCloseable {

    private final static boolean init = PackageInfo.loadJNI();

    Stack(long maxsize){
        initCppStack(maxsize);
    }

    // See https://www.baeldung.com/java-finalize
    // We use closable rather than finalizer as finalizer is not recommanded and is deprecated
    @Override
    public void close() {
        deleteCppStack();
    }

    public native long size();

    public native byte[] at(int pos);

    private native void initCppStack(long maxsize);
    private native void deleteCppStack();
    private long cppStack; // pointer holding cpp Stack object
}
