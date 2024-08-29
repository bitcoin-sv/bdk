package com.nchain.bdk;

import com.nchain.bdk.PackageInfo;

public class Assembler {

    private final static boolean init = PackageInfo.loadJNI();

    public native byte[] fromAsm(String script);
    public native String toAsm(byte[] script);
}

