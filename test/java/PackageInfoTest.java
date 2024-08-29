package com.nchain.sesdk;

import org.testng.Assert;
import org.testng.annotations.Test;

public class PackageInfoTest {

    private final static boolean init = PackageInfo.loadJNI();

    @Test
    public void testVersionIncrement() {

        final int javaVersionMajor = PackageInfo.BDK_JAVA_VERSION_MAJOR;
        final int javaVersionMinor = PackageInfo.BDK_JAVA_VERSION_MINOR;
        final int javaVersionPatch = PackageInfo.BDK_JAVA_VERSION_PATCH;

        final int coreVersionMajor = PackageInfo.BDK_CORE_VERSION_MAJOR;
        final int coreVersionMinor = PackageInfo.BDK_CORE_VERSION_MINOR;
        final int coreVersionPatch = PackageInfo.BDK_CORE_VERSION_PATCH;

        final int sesdkVersionMajor = PackageInfo.BDK_VERSION_MAJOR;
        final int sesdkVersionMinor = PackageInfo.BDK_VERSION_MINOR;
        final int sesdkVersionPatch = PackageInfo.BDK_VERSION_PATCH;

        /* Java module versions should not be lower than core versions */
        Assert.assertTrue(javaVersionMajor>=coreVersionMajor);
        Assert.assertTrue(javaVersionMinor>=coreVersionMinor);
        Assert.assertTrue(javaVersionPatch>=coreVersionPatch);

        /* Java module versions should not be greater than global versions */
        Assert.assertTrue(javaVersionMajor<=sesdkVersionMajor);
        Assert.assertTrue(javaVersionMinor<=sesdkVersionMinor);
        Assert.assertTrue(javaVersionPatch<=sesdkVersionPatch);

        final String JavaVersionString = String.valueOf(PackageInfo.BDK_JAVA_VERSION_MAJOR) + "." + String.valueOf(PackageInfo.BDK_JAVA_VERSION_MINOR) + "." + String.valueOf(PackageInfo.BDK_JAVA_VERSION_PATCH);
        Assert.assertEquals(PackageInfo.BDK_JAVA_VERSION_STRING , JavaVersionString);
    }
}
