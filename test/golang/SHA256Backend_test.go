package main

import (
	"os"
	"runtime"
	"strings"
	"sync"
	"testing"

	"github.com/bitcoin-sv/bdk/module/gobdk"
	"github.com/bitcoin-sv/bdk/module/gobdk/script"
	"github.com/stretchr/testify/assert"
)

// forcedAtStart records BDK_SHA256_FORCE_SCALAR as the process started. init
// then flips it: BDK reads it while the program is loaded, before any Go init
// function runs, so the flip must not change the selection.
var forcedAtStart = func() bool {
	v := os.Getenv("BDK_SHA256_FORCE_SCALAR")
	return v != "" && v != "0"
}()

func init() {
	if forcedAtStart {
		os.Setenv("BDK_SHA256_FORCE_SCALAR", "0")
	} else {
		os.Setenv("BDK_SHA256_FORCE_SCALAR", "1")
	}
}

// expectedSHA256Backend says which SHA-256 backend BDK should select on this
// machine, without BDK's own detection code: /proc/cpuinfo on Linux, and on
// Apple silicon a fixed expectation rather than a probe.
func expectedSHA256Backend(t *testing.T) string {
	t.Helper()
	if forcedAtStart {
		return "standard(forced)"
	}
	if runtime.GOOS == "darwin" && runtime.GOARCH == "arm64" {
		return "arm_shani,arm_shani(2way)" // every Apple silicon core implements FEAT_SHA256
	}
	if runtime.GOOS != "linux" {
		t.Skipf("no independent CPU probe for %s/%s", runtime.GOOS, runtime.GOARCH)
	}
	cpuinfo, err := os.ReadFile("/proc/cpuinfo")
	if err != nil {
		t.Skipf("cannot read /proc/cpuinfo: %v", err)
	}
	flags := map[string]bool{}
	for _, line := range strings.Split(string(cpuinfo), "\n") {
		if strings.HasPrefix(line, "flags") || strings.HasPrefix(line, "Features") {
			for _, f := range strings.Fields(line[strings.Index(line, ":")+1:]) {
				flags[f] = true
			}
			break
		}
	}
	switch runtime.GOARCH {
	case "amd64":
		if flags["sse4_1"] && flags["sha_ni"] {
			return "shani,shani(2way)"
		}
		if flags["sse4_1"] {
			return "sse4"
		}
	case "arm64":
		if flags["sha2"] {
			return "arm_shani,arm_shani(2way)"
		}
	}
	return "standard"
}

// Runs in its own process (one go test per file) that has constructed no
// validator. The backend was selected while the program loaded, from the
// environment it started with, so concurrent getter calls must all report it.
func TestSHA256BackendIsSelectedAtLoad(t *testing.T) {
	expected := expectedSHA256Backend(t)

	const callers = 16
	got := make([]string, callers)
	var start, done sync.WaitGroup
	start.Add(1)
	for i := 0; i < callers; i++ {
		done.Add(1)
		go func(i int) {
			defer done.Done()
			start.Wait()
			got[i] = gobdk.SHA256_BACKEND()
		}(i)
	}
	start.Done()
	done.Wait()

	for i := range got {
		assert.Equal(t, expected, got[i], "caller %d", i)
	}
	t.Logf("SHA-256 backend: %s", got[0])

	// Constructing a validator afterwards does not change the selection
	validator := script.NewTxValidator("main")
	assert.NotNil(t, validator)
	assert.Equal(t, expected, gobdk.SHA256_BACKEND())
}
