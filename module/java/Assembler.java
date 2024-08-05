package com.nchain.sesdk;

import com.nchain.sesdk.PackageInfo;

public class Assembler {

    private final static boolean init = PackageInfo.loadJNI();

    public native byte[] fromAsm(String script);
    public native String toAsm(byte[] script);
}

