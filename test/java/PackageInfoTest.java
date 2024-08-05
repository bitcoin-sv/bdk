package com.nchain.sesdk;

import org.testng.Assert;
import org.testng.annotations.Test;

public class PackageInfoTest {

    private final static boolean init = PackageInfo.loadJNI();

    @Test
    public void testVersionIncrement() {

        final int javaVersionMajor = PackageInfo.SESDK_JAVA_VERSION_MAJOR;
        final int javaVersionMinor = PackageInfo.SESDK_JAVA_VERSION_MINOR;
        final int javaVersionPatch = PackageInfo.SESDK_JAVA_VERSION_PATCH;

        final int coreVersionMajor = PackageInfo.SESDK_CORE_VERSION_MAJOR;
        final int coreVersionMinor = PackageInfo.SESDK_CORE_VERSION_MINOR;
        final int coreVersionPatch = PackageInfo.SESDK_CORE_VERSION_PATCH;

        final int sesdkVersionMajor = PackageInfo.SESDK_VERSION_MAJOR;
        final int sesdkVersionMinor = PackageInfo.SESDK_VERSION_MINOR;
        final int sesdkVersionPatch = PackageInfo.SESDK_VERSION_PATCH;

        /* Java module versions should not be lower than core versions */
        Assert.assertTrue(javaVersionMajor>=coreVersionMajor);
        Assert.assertTrue(javaVersionMinor>=coreVersionMinor);
        Assert.assertTrue(javaVersionPatch>=coreVersionPatch);

        /* Java module versions should not be greater than global versions */
        Assert.assertTrue(javaVersionMajor<=sesdkVersionMajor);
        Assert.assertTrue(javaVersionMinor<=sesdkVersionMinor);
        Assert.assertTrue(javaVersionPatch<=sesdkVersionPatch);

        final String JavaVersionString = String.valueOf(PackageInfo.SESDK_JAVA_VERSION_MAJOR) + "." + String.valueOf(PackageInfo.SESDK_JAVA_VERSION_MINOR) + "." + String.valueOf(PackageInfo.SESDK_JAVA_VERSION_PATCH);
        Assert.assertEquals(PackageInfo.SESDK_JAVA_VERSION_STRING , JavaVersionString);
    }
}
