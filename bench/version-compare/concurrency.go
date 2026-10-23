package main

import (
	"encoding/json"
	"fmt"
	"os"
	"runtime"
	"sort"
	"sync"
	"sync/atomic"
	"time"

	gobdk "github.com/bitcoin-sv/bdk/module/gobdk"
)

type concResult struct {
	Mode            string    `json:"mode"`
	Variant         string    `json:"variant"`
	Round           int       `json:"round"`
	Status          string    `json:"status"`
	Error           string    `json:"error,omitempty"`
	GobdkModule     string    `json:"gobdk_module"`
	BDKVersion      string    `json:"bdk_version"`
	SHA256Backend   string    `json:"sha256_backend"`
	HelpersSet      string    `json:"helpers_set"`
	HelpersGet      *int64    `json:"helpers_get,omitempty"`
	GOMAXPROCS      int       `json:"gomaxprocs"`
	NumCPU          int       `json:"num_cpu"`
	SmallShape      string    `json:"small_shape"`
	LargeShape      string    `json:"large_shape"`
	SmallGoroutines int       `json:"small_goroutines"`
	WindowSeconds   float64   `json:"window_seconds"`
	SmallCount      int64     `json:"small_count"`
	SmallPerSecond  float64   `json:"small_per_second"`
	LargeCount      int       `json:"large_count"`
	LargeLatencyNs  []float64 `json:"large_latency_ns"`
	LargeMedianNs   float64   `json:"large_median_ns"`
	CPUSeconds      float64   `json:"cpu_seconds"`
	Timestamp       string    `json:"timestamp"`
}

// runConcurrency validates the small shape in a loop from nSmall goroutines and,
// unless largeName is empty, the large shape in a loop from one more goroutine,
// all sharing one TxValidator, for window. It counts small validations that
// finish inside the window and records each large validation's latency.
func runConcurrency(all []shape, smallName, largeName string, nSmall int, window time.Duration, round int,
	cacheDir, helpersSet, jsonOut, benchOut string) int {
	if nSmall <= 0 {
		nSmall = runtime.GOMAXPROCS(0)
	}
	variant := fmt.Sprintf("small%dx%s", nSmall, smallName)
	if largeName != "" {
		variant += "+1x" + largeName
	}
	r := concResult{
		Mode: "concurrency", Variant: variant, Round: round, GobdkModule: gobdkModuleVersion(),
		BDKVersion: gobdk.BDK_VERSION_STRING(), SHA256Backend: sha256Backend(), HelpersSet: helpersSet,
		GOMAXPROCS: runtime.GOMAXPROCS(0), NumCPU: runtime.NumCPU(), SmallShape: smallName, LargeShape: largeName,
		SmallGoroutines: nSmall, Timestamp: time.Now().UTC().Format(time.RFC3339),
	}
	code := concurrency(&r, all, smallName, largeName, nSmall, window, cacheDir)

	line, _ := json.Marshal(r)
	if jsonOut == "" {
		fmt.Println(string(line))
	} else {
		appendLine(jsonOut, string(line))
	}
	if benchOut != "" && r.Status == "ok" {
		appendLine(benchOut, fmt.Sprintf("BenchmarkConcurrency/%s-%d\t1\t%.0f large-ns/op\t%.1f small-tx/s",
			variant, r.GOMAXPROCS, r.LargeMedianNs, r.SmallPerSecond))
	}
	return code
}

func concurrency(r *concResult, all []shape, smallName, largeName string, nSmall int, window time.Duration, cacheDir string) int {
	type loaded struct {
		etx     []byte
		heights []int32
		sh      *shape
	}
	load := func(name string) (*loaded, error) {
		sh := findShape(all, name)
		if sh == nil {
			return nil, fmt.Errorf("unknown shape %q", name)
		}
		etx, heights, err := loadOrBuild(sh, cacheDir)
		if err != nil {
			return nil, err
		}
		return &loaded{etx, heights, sh}, nil
	}
	small, err := load(smallName)
	if err != nil {
		r.Status, r.Error = "error", err.Error()
		return 1
	}
	var large *loaded
	if largeName != "" {
		if large, err = load(largeName); err != nil {
			r.Status, r.Error = "error", err.Error()
			return 1
		}
	}

	// One validator shared by every goroutine, as a node shares one.
	se, err := newValidator()
	if err != nil {
		r.Status, r.Error = "error", err.Error()
		return 1
	}
	if haveHelpers {
		h := getHelpers()
		r.HelpersGet = &h
	}
	check := func(l *loaded) error {
		err := se.ValidateTransaction(l.etx, l.heights, blockHeight, l.sh.consensus)
		if (err == nil) != l.sh.wantValid {
			return fmt.Errorf("%s: want valid=%v, got %v", l.sh.name, l.sh.wantValid, err)
		}
		return nil
	}
	// Warm-up, which also starts any helper threads.
	for _, l := range []*loaded{small, large} {
		if l == nil {
			continue
		}
		if err := check(l); err != nil {
			r.Status, r.Error = "wrong_verdict", err.Error()
			return 1
		}
	}

	var (
		smallCount atomic.Int64
		failed     atomic.Value
		wg         sync.WaitGroup
		mu         sync.Mutex
		latencies  []float64
	)
	cpu0 := cpuTime()
	start := time.Now()
	deadline := start.Add(window)
	for g := 0; g < nSmall; g++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for time.Now().Before(deadline) {
				if err := check(small); err != nil {
					failed.Store(err.Error())
					return
				}
				if time.Now().Before(deadline) {
					smallCount.Add(1)
				}
			}
		}()
	}
	if large != nil {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for time.Now().Before(deadline) {
				t0 := time.Now()
				if err := check(large); err != nil {
					failed.Store(err.Error())
					return
				}
				if end := time.Now(); end.Before(deadline) {
					mu.Lock()
					latencies = append(latencies, float64(end.Sub(t0).Nanoseconds()))
					mu.Unlock()
				}
			}
		}()
	}
	wg.Wait()
	r.CPUSeconds = (cpuTime() - cpu0).Seconds()
	if msg, ok := failed.Load().(string); ok {
		r.Status, r.Error = "wrong_verdict", msg
		return 1
	}

	r.WindowSeconds = window.Seconds()
	r.SmallCount = smallCount.Load()
	r.SmallPerSecond = float64(r.SmallCount) / window.Seconds()
	r.LargeLatencyNs = latencies
	r.LargeCount = len(latencies)
	if len(latencies) > 0 {
		s := append([]float64(nil), latencies...)
		sort.Float64s(s)
		if n := len(s); n%2 == 1 {
			r.LargeMedianNs = s[n/2]
		} else {
			r.LargeMedianNs = (s[n/2-1] + s[n/2]) / 2
		}
	}
	if large != nil && len(latencies) == 0 {
		r.Status, r.Error = "error", "no large validation finished inside the window"
		fmt.Fprintln(os.Stderr, r.Error)
		return 1
	}
	r.Status = "ok"
	return 0
}
