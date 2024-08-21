package config

// Typesafe definition of environment variable name
// There are an implicit conversion from key path to environment variable name
// Example
//
//	key  my.variable.name will map to the environment variable MY_VARIABLE_NAME

const (
	// Script config in environment
	keyMaxOpsPerScriptPolicy        = "gobdk.scriptconfig.maxopsperscriptpolicy"
	keyMaxScriptNumLengthPolicy     = "gobdk.scriptconfig.maxscriptnumlengthpolicy"
	keyMaxScriptSizePolicy          = "gobdk.scriptconfig.maxscriptsizepolicy"
	keyMaxPubKeysPerMultiSig        = "gobdk.scriptconfig.maxpubkeyspermultisig"
	keyMaxStackMemoryUsageConsensus = "gobdk.scriptconfig.maxstackmemoryusageconsensus"
	keyMaxStackMemoryUsagePolicy    = "gobdk.scriptconfig.maxstackmemoryusagepolicy"
)
