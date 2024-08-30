package com.nchain.bdk;

import org.testng.Assert;
import org.testng.annotations.Test;

public class PackageInfoTest {

    private final static boolean init = PackageInfo.loadJNI();

    @Test
    public void testVersionIncrement() {

        // final int bsvVersionMajor = PackageInfo.BSV_CLIENT_VERSION_MAJOR;
        // final int bsvVersionMinor = PackageInfo.BSV_CLIENT_VERSION_MINOR;
        // final int bsvVersionPatch = PackageInfo.BSV_CLIENT_VERSION_PATCH;

        final int bdkVersionMajor = PackageInfo.BDK_VERSION_MAJOR;
        final int bdkVersionMinor = PackageInfo.BDK_VERSION_MINOR;
        final int bdkVersionPatch = PackageInfo.BDK_VERSION_PATCH;

        final int javaVersionMajor = PackageInfo.BDK_JAVA_VERSION_MAJOR;
        final int javaVersionMinor = PackageInfo.BDK_JAVA_VERSION_MINOR;
        final int javaVersionPatch = PackageInfo.BDK_JAVA_VERSION_PATCH;

        /* Java module versions should not be lower than core versions */
        Assert.assertTrue(javaVersionMajor>=bdkVersionMajor);
        Assert.assertTrue(javaVersionMinor>=bdkVersionMinor);
        Assert.assertTrue(javaVersionPatch>=bdkVersionPatch);

        final String JavaVersionString = String.valueOf(PackageInfo.BDK_JAVA_VERSION_MAJOR) + "." + String.valueOf(PackageInfo.BDK_JAVA_VERSION_MINOR) + "." + String.valueOf(PackageInfo.BDK_JAVA_VERSION_PATCH);
        Assert.assertEquals(PackageInfo.BDK_JAVA_VERSION_STRING , JavaVersionString);
    }
}
