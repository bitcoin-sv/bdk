package config

// ScriptConfig hold the settings that impact the script operations
// There are equivalent in the bitcoin.conf file
type ScriptConfig struct {
	MaxOpsPerScriptPolicy        uint64 `mapstructure:"maxOpsPerScriptPolicy" json:"maxOpsPerScriptPolicy" validate:"required"`
	MaxScriptNumLengthPolicy     uint64 `mapstructure:"maxScriptNumLengthPolicy" json:"maxScriptNumLengthPolicy" validate:"required"`
	MaxScriptSizePolicy          uint64 `mapstructure:"maxScriptSizePolicy" json:"maxScriptSizePolicy" validate:"required"`
	MaxPubKeysPerMultiSig        uint64 `mapstructure:"maxPubKeysPerMultiSig" json:"maxPubKeysPerMultiSig" validate:"required"`
	MaxStackMemoryUsageConsensus uint64 `mapstructure:"maxStackMemoryUsageConsensus" json:"maxStackMemoryUsageConsensus" validate:"required"`
	MaxStackMemoryUsagePolicy    uint64 `mapstructure:"maxStackMemoryUsagePolicy" json:"maxStackMemoryUsagePolicy" validate:"required"`
}

// LoadScriptConfig return the functional loader to load parameters from environment variable
// to ScriptConfig structure
func LoadScriptConfig() SettingLoader {

	return func(s *Setting) error {

		s.scriptConfig.MaxOpsPerScriptPolicy = s.env.GetUint64(keyMaxOpsPerScriptPolicy)
		s.scriptConfig.MaxScriptNumLengthPolicy = s.env.GetUint64(keyMaxScriptNumLengthPolicy)
		s.scriptConfig.MaxScriptSizePolicy = s.env.GetUint64(keyMaxScriptSizePolicy)
		s.scriptConfig.MaxPubKeysPerMultiSig = s.env.GetUint64(keyMaxPubKeysPerMultiSig)
		s.scriptConfig.MaxStackMemoryUsageConsensus = s.env.GetUint64(keyMaxStackMemoryUsageConsensus)
		s.scriptConfig.MaxStackMemoryUsagePolicy = s.env.GetUint64(keyMaxStackMemoryUsagePolicy)

		return nil
	}
}