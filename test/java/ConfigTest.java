package com.nchain.sesdk;

import org.testng.Assert;
import org.testng.annotations.Test;

public class ConfigTest {

    private final static boolean init = PackageInfo.loadJNI();
    public Config c = new Config();
    public Config configConsensusDisabled = new Config(true,false);
    public Config configFlagsDisabled = new Config(false,false);
    public Config configGenesisDisabled = new Config(false,true);
    public Config configFlagsEnabled = new Config(true,true);


    @Test
    public void defaultConfig(){
        final long v2 = c.getMaxOpsPerScript();
        final long v3 = c.getMaxScriptNumLength();
        final long v4 = c.getMaxScriptSize();
        final long v5 = c.getMaxPubKeysPerMultiSig();
        final long v1 = c.getMaxStackMemoryUsage();

        Assert.assertTrue(v1 >= 0);
        Assert.assertTrue(v2 >= 0);
        Assert.assertTrue(v3 >= 0);
        Assert.assertTrue(v4 >= 0);
        Assert.assertTrue(v5 >= 0);
    }

    @Test
    public void testCloseableIdempotent() {

        Config aconfig = new Config();
        final long v1 = aconfig.getMaxOpsPerScript();
        aconfig.close();
        aconfig.close();
    }

    @Test(expectedExceptions = RuntimeException.class)
    public void testUseAfterClosed() {

        Config aconfig = new Config();
        aconfig.close();
        final long v2 = aconfig.getMaxOpsPerScript();// Should throw
    }

    @Test(priority = 1)
    public void loadConfigFromFile(){
        final String configFile = getTestConfigFile();
        c.load(configFile);

        final long v1 = c.getMaxOpsPerScript();
        final long v2 = c.getMaxScriptNumLength();
        final long v3 = c.getMaxScriptSize();
        final long v4 = c.getMaxPubKeysPerMultiSig();
        final long v5 = c.getMaxStackMemoryUsage();

        Assert.assertTrue(v1 == 4294967295L);
        Assert.assertTrue(v2 == 750000L);
        Assert.assertTrue(v3 == 4294967295L);
        Assert.assertTrue(v4 == 4294967295L);
        Assert.assertTrue(v5 == 4294967295L);
    }

    //load Config with flags - isGenesisEnabled = true, isConsensus = false
    @Test
    public void testConfigWithConsensusDisabled() {
        Assert.assertTrue(configConsensusDisabled.getMaxOpsPerScript() >= 0);
        Assert.assertTrue(configConsensusDisabled.getMaxScriptNumLength() >= 0);
        Assert.assertTrue(configConsensusDisabled.getMaxScriptSize() >= 0);
        Assert.assertTrue(configConsensusDisabled.getMaxPubKeysPerMultiSig() >= 0);
        Assert.assertTrue(configConsensusDisabled.getMaxStackMemoryUsage() >= 0);
    }

    //load Config with flags - isGenesisEnabled = false, isConsensus = true
    @Test
    public void testConfigWithGenesisDisabled() {
        Assert.assertTrue(configGenesisDisabled.getMaxOpsPerScript() >= 0);
        Assert.assertTrue(configGenesisDisabled.getMaxScriptNumLength() >= 0);
        Assert.assertTrue(configGenesisDisabled.getMaxScriptSize() >= 0);
        Assert.assertTrue(configGenesisDisabled.getMaxPubKeysPerMultiSig() >= 0);
        Assert.assertTrue(configGenesisDisabled.getMaxStackMemoryUsage() >= 0);    	
    }

    //load Config with flags - isGenesisEnabled = false, isConsensus = false
    @Test
    public void testConfigWithFlagsDisabled() {
        Assert.assertTrue(configFlagsDisabled.getMaxOpsPerScript() >= 0);
        Assert.assertTrue(configFlagsDisabled.getMaxScriptNumLength() >= 0);
        Assert.assertTrue(configFlagsDisabled.getMaxScriptSize() >= 0);
        Assert.assertTrue(configFlagsDisabled.getMaxPubKeysPerMultiSig() >= 0);
        Assert.assertTrue(configFlagsDisabled.getMaxStackMemoryUsage() >= 0);    	
    }

    //load Config with flags - isGenesisEnabled = true, isConsensus = true this is the default values set when config object is created without passing the flags
    @Test
    public void testConfigWithFlagsEnabled() {
        Assert.assertTrue(configFlagsEnabled.getMaxOpsPerScript() == c.getMaxOpsPerScript());
        Assert.assertTrue(configFlagsEnabled.getMaxScriptNumLength() == c.getMaxScriptNumLength());
        Assert.assertTrue(configFlagsEnabled.getMaxScriptSize() == c.getMaxScriptSize());
        Assert.assertTrue(configFlagsEnabled.getMaxPubKeysPerMultiSig() == c.getMaxPubKeysPerMultiSig());
        Assert.assertTrue(configFlagsEnabled.getMaxStackMemoryUsage() == c.getMaxStackMemoryUsage());       
    }

    //Test Scenario : set MaxOpsPerScriptPolicy value for a config loaded with consensus flag set to false 
    //note : getMaxOpsPerScript returns the same value set by setMaxOpsPerScriptPolicy function only for the config object loaded with consensus flag set to false 
    @Test
    public void testSetMaxOpsPerScriptPolicy() {
        configConsensusDisabled.setMaxOpsPerScriptPolicy(4294967000L);

        Assert.assertEquals(configConsensusDisabled.getMaxOpsPerScript(), 4294967000L);
    }

    //Test Scenario : set MaxOpsPerScriptPolicy with more than limit value and this should fail with error message - Policy value for MaxOpsPerScript must not exceed consensus limit of 4294967295.
    @Test
    public void testSetMaxOpsPerScriptPolicyWithExceededVaue() {
        try {
            configConsensusDisabled.setMaxOpsPerScriptPolicy(4294967296L);
        }catch(Exception ex) {
            Assert.assertTrue(ex.getMessage().contentEquals("Policy value for MaxOpsPerScript must not exceed consensus limit of 4294967295."));
        }
    }

    //Test Scenario : set MaxScriptnumLength value for a config loaded with consensus flag set to false 
    //note : getMaxScriptNumLength returns the same value set by setMaxScriptnumLengthPolicy function only for the config object loaded with consensus flag set to false 
    @Test
    public void testSetMaxScriptnumLengthPolicy() {
        configConsensusDisabled.setMaxScriptnumLengthPolicy(699999);

        Assert.assertEquals(configConsensusDisabled.getMaxScriptNumLength(), 699999);
    }

    //Test Scenario : set MaxScriptnumLengthPolicy with more than limit value and this should fail with error message - Policy value for maximum script number length must not exceed consensus limit of 750000.
    @Test
    public void testSetMaxScriptnumLengthPolicyWithExceededValue() {
        try {    		
            configConsensusDisabled.setMaxScriptnumLengthPolicy(750001);
        }catch(Exception ex) {
            Assert.assertTrue(ex.getMessage().contentEquals("Policy value for maximum script number length must not exceed consensus limit of 750000."));
        }    	
    }

    //Test Scenario : set MaxScriptSizePolicy value for a config loaded with consensus flag set to false 
    //note : getMaxScriptSize returns the same value set by setMaxScriptSizePolicy function only for the config object loaded with consensus flag set to false 
    @Test
    public void testSetMaxScriptSizePolicy() {
        configConsensusDisabled.setMaxScriptSizePolicy(4294967290L);

        Assert.assertTrue(configConsensusDisabled.getMaxScriptSize() == 4294967290L);
    }

    //Test Scenario : set MaxScriptSizePolicy with more than limit value and this should fail with error message - Policy value for max script size must not exceed consensus limit of 4294967295
    @Test
    public void testSetMaxScriptSizePolicyWithExceededValue() {
        try {
            configConsensusDisabled.setMaxScriptSizePolicy(4294967296L);
        }catch(Exception ex) {
            Assert.assertTrue(ex.getMessage().contentEquals("Policy value for max script size must not exceed consensus limit of 4294967295"));
        }
    }

    //Test Scenario : set MaxPubkeysPerMultisigPolicy value for a config loaded with consensus flag set to false 
    //note : getMaxPubKeysPerMultiSig returns the same value set by setMaxPubkeysPerMultisigPolicy function only for the config object loaded with consensus flag set to false 
    @Test
    public void testsetMaxPubkeysPerMultisigPolicy() {
        configConsensusDisabled.setMaxPubkeysPerMultisigPolicy(4294967294L);

        Assert.assertTrue(configConsensusDisabled.getMaxPubKeysPerMultiSig() == 4294967294L);
    }

    //Test Scenario : set MaxPubkeysPerMultisigPolicy with more than limit value and this should fail with error message - Policy value for maximum public keys per multisig must not exceed consensus limit of 4294967295.
    @Test
    public void testsetMaxPubkeysPerMultisigPolicyWithExceededValue() {
        try {
            configConsensusDisabled.setMaxPubkeysPerMultisigPolicy(4294967296L);
        }catch(Exception ex) {
            Assert.assertTrue(ex.getMessage().contentEquals("Policy value for maximum public keys per multisig must not exceed consensus limit of 4294967295."));
        }
    }

    //Test Scenario : set MaxStackMemoryUsage value for a config loaded with consensus flag set to false 
    //note : getMaxStackMemoryUsage returns the same value set by setMaxStackMemoryUsage function only for the config object loaded with consensus flag set to false   
    @Test
    public void testSetMaxStackMemoryUsage() {
        configConsensusDisabled.setMaxStackMemoryUsage(0, 9223372036854775804L);

        Assert.assertTrue(configConsensusDisabled.getMaxStackMemoryUsage() == 9223372036854775804L);
    }

    //Test Scenario : set MaxStackMemoryUsage with more than limit value and this should fail with error message - Policy and consensus value for max stack memory usage must not be less than 0.
    @Test
    public void testSetMaxStackMemoryUsageWithExceededValue() {

        try {
            configConsensusDisabled.setMaxStackMemoryUsage(0, 9223372036854775807L+1);
        }catch(Exception ex) {
            Assert.assertTrue(ex.getMessage().contentEquals("Policy and consensus value for max stack memory usage must not be less than 0."));
        }
    } 


    private String getTestConfigFile(){
        return System.getProperty("java_test_data_dir") + "/test_config.conf";
    }
}
