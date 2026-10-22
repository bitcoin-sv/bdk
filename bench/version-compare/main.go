// Command bdk-bench times script.TxValidator.ValidateTransaction on one
// deterministic transaction shape, for comparing gobdk releases. Run one process
// per (version, shape, round); see run.sh.
package main

import (
	"bufio"
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"runtime"
	"runtime/debug"
	"strings"
	"syscall"
	"time"

	gobdk "github.com/bitcoin-sv/bdk/module/gobdk"
	"github.com/bitcoin-sv/bdk/module/gobdk/script"
)

type result struct {
	Shape         string  `json:"shape"`
	Group         string  `json:"group"`
	Round         int     `json:"round"`
	Status        string  `json:"status"`
	Error         string  `json:"error,omitempty"`
	GobdkModule   string  `json:"gobdk_module"`
	BDKVersion    string  `json:"bdk_version"`
	BSVVersion    string  `json:"bsv_version"`
	BSVCommit     string  `json:"bsv_commit"`
	SourceCommit  string  `json:"source_commit"`
	SHA256Backend string  `json:"sha256_backend"`
	ForceScalar   string  `json:"force_scalar_env"`
	Inputs        int     `json:"inputs"`
	TxBytes       int     `json:"tx_bytes"`
	TxSHA256      string  `json:"tx_sha256"`
	Consensus     bool    `json:"consensus"`
	BlockHeight   int32   `json:"block_height"`
	WantValid     bool    `json:"want_valid"`
	Verdict       string  `json:"verdict"`
	BuildSeconds  float64 `json:"build_seconds"`
	WarmupNs      int64   `json:"warmup_ns_per_op"`
	Iters         int     `json:"iters"`
	NsPerOp       float64 `json:"ns_per_op"`
	CPUNsPerOp    float64 `json:"cpu_ns_per_op"`
	GoVersion     string  `json:"go_version"`
	GOOS          string  `json:"goos"`
	GOARCH        string  `json:"goarch"`
	GOMAXPROCS    int     `json:"gomaxprocs"`
	CPUModel      string  `json:"cpu_model"`
	Timestamp     string  `json:"timestamp"`
}

func main() {
	shapeName := flag.String("shape", "", "shape to run")
	round := flag.Int("round", 0, "round number, recorded in the output")
	minTime := flag.Duration("min-time", 2*time.Second, "minimum measured time")
	maxTime := flag.Duration("max-time", 60*time.Second, "stop adding iterations past this estimate")
	minIters := flag.Int("min-iters", 3, "minimum measured iterations")
	cacheDir := flag.String("cache", "", "directory caching built transactions, so every version validates identical bytes")
	jsonOut := flag.String("json", "", "append the JSON result line to this file (default stdout)")
	benchOut := flag.String("bench", "", "append a Go benchmark line to this file")
	list := flag.Bool("list", false, "print shape names and exit")
	flag.Parse()

	all := shapes()
	if *list {
		for _, s := range all {
			fmt.Println(s.name)
		}
		return
	}
	var sh *shape
	for i := range all {
		if all[i].name == *shapeName {
			sh = &all[i]
		}
	}
	if sh == nil {
		fmt.Fprintf(os.Stderr, "unknown shape %q\n", *shapeName)
		os.Exit(2)
	}

	r := result{
		Shape: sh.name, Group: sh.group, Round: *round, Inputs: sh.inputs, Consensus: sh.consensus,
		BlockHeight: blockHeight, WantValid: sh.wantValid,
		GobdkModule: gobdkModuleVersion(), BDKVersion: gobdk.BDK_VERSION_STRING(),
		BSVVersion: gobdk.BSV_VERSION_STRING(), BSVCommit: gobdk.BSV_GIT_COMMIT_HASH(),
		SourceCommit: gobdk.SOURCE_GIT_COMMIT_HASH(), SHA256Backend: sha256Backend(),
		ForceScalar: os.Getenv("BDK_SHA256_FORCE_SCALAR"),
		GoVersion:   runtime.Version(), GOOS: runtime.GOOS, GOARCH: runtime.GOARCH,
		GOMAXPROCS: runtime.GOMAXPROCS(0), CPUModel: cpuModel(),
		Timestamp: time.Now().UTC().Format(time.RFC3339),
	}

	code := run(sh, &r, *cacheDir, *minTime, *maxTime, *minIters)
	emit(r, *jsonOut, *benchOut)
	os.Exit(code)
}

func run(sh *shape, r *result, cacheDir string, minTime, maxTime time.Duration, minIters int) int {
	start := time.Now()
	etx, heights, err := loadOrBuild(sh, cacheDir)
	r.BuildSeconds = time.Since(start).Seconds()
	if err != nil {
		r.Status, r.Error = "error", "build: "+err.Error()
		return 1
	}
	sum := sha256.Sum256(etx)
	r.TxBytes, r.TxSHA256 = len(etx), hex.EncodeToString(sum[:])

	se := script.NewTxValidator("main")
	if se == nil {
		r.Status, r.Error = "error", "NewTxValidator returned nil"
		return 1
	}
	if err := se.SetGenesisActivationHeight(genesisHeight); err != nil {
		r.Status, r.Error = "error", err.Error()
		return 1
	}
	if err := se.SetChronicleActivationHeight(chronicleHeight); err != nil {
		r.Status, r.Error = "error", err.Error()
		return 1
	}

	validate := func() error { return se.ValidateTransaction(etx, heights, blockHeight, sh.consensus) }
	check := func(err error) bool {
		if sh.wantValid {
			return err == nil
		}
		return err != nil
	}

	// Warm-up: one call, which also gives the verdict and the per-op estimate.
	t0 := time.Now()
	err = validate()
	warm := time.Since(t0)
	r.WarmupNs = warm.Nanoseconds()
	r.Verdict = verdict(err)
	if !check(err) {
		r.Status, r.Error = "wrong_verdict", fmt.Sprintf("warm-up: want valid=%v, got %v", sh.wantValid, err)
		return 1
	}
	// A short warm-up for fast shapes, so caches and branch predictors settle.
	for warmEnd := time.Now().Add(200 * time.Millisecond); warm < 50*time.Millisecond && time.Now().Before(warmEnd); {
		if err := validate(); !check(err) {
			r.Status, r.Error = "wrong_verdict", fmt.Sprintf("warm-up: want valid=%v, got %v", sh.wantValid, err)
			return 1
		}
	}

	per := math.Max(float64(warm.Nanoseconds()), 1)
	iters := int(math.Ceil(float64(minTime.Nanoseconds()) / per))
	if capIters := int(float64(maxTime.Nanoseconds()) / per); iters > capIters {
		iters = capIters
	}
	if iters < minIters {
		iters = minIters
	}

	cpu0 := cpuTime()
	t0 = time.Now()
	for i := 0; i < iters; i++ {
		if err := validate(); !check(err) {
			r.Status, r.Error = "wrong_verdict", fmt.Sprintf("iteration %d: want valid=%v, got %v", i, sh.wantValid, err)
			return 1
		}
	}
	wall := time.Since(t0)
	cpu := cpuTime() - cpu0

	r.Iters = iters
	r.NsPerOp = float64(wall.Nanoseconds()) / float64(iters)
	r.CPUNsPerOp = float64(cpu.Nanoseconds()) / float64(iters)
	r.Status = "ok"
	return 0
}

func verdict(err error) string {
	if err == nil {
		return "valid"
	}
	return "invalid: " + err.Error()
}

// loadOrBuild reads the shape from cacheDir when present, otherwise builds it
// and, with a cacheDir, stores it.
func loadOrBuild(sh *shape, cacheDir string) ([]byte, []int32, error) {
	if cacheDir == "" {
		return sh.build()
	}
	path := filepath.Join(cacheDir, sh.name+".etx")
	if etx, err := os.ReadFile(path); err == nil {
		heights := make([]int32, sh.inputs)
		for i := range heights {
			heights[i] = utxoHeight
		}
		return etx, heights, nil
	}
	etx, heights, err := sh.build()
	if err != nil {
		return nil, nil, err
	}
	tmp := path + ".tmp"
	if err := os.WriteFile(tmp, etx, 0o644); err != nil {
		return nil, nil, err
	}
	return etx, heights, os.Rename(tmp, path)
}

func emit(r result, jsonOut, benchOut string) {
	line, _ := json.Marshal(r)
	if jsonOut == "" {
		fmt.Println(string(line))
	} else {
		appendLine(jsonOut, string(line))
	}
	if benchOut != "" && r.Status == "ok" {
		appendLine(benchOut, fmt.Sprintf("BenchmarkValidateTransaction/%s-%d\t%d\t%.0f ns/op\t%.0f cpu-ns/op",
			r.Shape, r.GOMAXPROCS, r.Iters, r.NsPerOp, r.CPUNsPerOp))
	}
}

func appendLine(path, line string) {
	f, err := os.OpenFile(path, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0o644)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	defer f.Close()
	fmt.Fprintln(f, line)
}

func gobdkModuleVersion() string {
	if bi, ok := debug.ReadBuildInfo(); ok {
		for _, d := range bi.Deps {
			if d.Path == "github.com/bitcoin-sv/bdk/module/gobdk" {
				if d.Replace != nil {
					return d.Version + " => " + d.Replace.Version
				}
				return d.Version
			}
		}
	}
	return "unknown"
}

func cpuTime() time.Duration {
	var ru syscall.Rusage
	if err := syscall.Getrusage(syscall.RUSAGE_SELF, &ru); err != nil {
		return 0
	}
	return time.Duration(ru.Utime.Nano() + ru.Stime.Nano())
}

func cpuModel() string {
	f, err := os.Open("/proc/cpuinfo")
	if err != nil {
		return ""
	}
	defer f.Close()
	sc := bufio.NewScanner(f)
	for sc.Scan() {
		k, v, ok := bytes.Cut(sc.Bytes(), []byte(":"))
		if ok && strings.TrimSpace(string(k)) == "model name" {
			return strings.TrimSpace(string(v))
		}
	}
	return ""
}
