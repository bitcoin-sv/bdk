package config

import (
	"fmt"
	"strings"

	"github.com/spf13/viper"
)

type SettingLoader func(s *Setting) error

// Setting hold the flat settings for the bdk core GlobalConfig
type Setting struct {
	// env hold Viper instance as a helper to load environment variable to Setting structure
	env *viper.Viper

	// ScriptConfig hold information to inject to GlobalConfig to bdk core for script features
	scriptConfig ScriptConfig
}

// LoadSetting load environment values into Setting structure
func LoadSetting(loaders ...SettingLoader) Setting {
	s := Setting{}
	s.initViper()

	// Use all loader to load environment variable to Setting
	for _, load := range loaders {
		if err := load(&s); err != nil {
			panic(fmt.Errorf("failed to load settings, error : %w", err))
		}
	}
	return s
}

// ScriptConfig return the information about the build
func (s *Setting) ScriptConfig() ScriptConfig {
	return s.scriptConfig
}

// InitViper initialize viper instance held in Setting if it is nil
// Do nothing if viper instance is non nil
func (s *Setting) initViper() {
	if s.env == nil {

		// TODO : load bitcoin.conf from a default location if found

		s.env = viper.New()
		s.env.AutomaticEnv()
		s.env.SetEnvKeyReplacer(strings.NewReplacer(".", "_"))
	}
}
