package cgobench

import (
	"fmt"
	"runtime"
	"sync"
	"sync/atomic"
	"syscall"
	"testing"
	"time"

	bdkscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
)

// BenchmarkValidateTransaction_GoLoad validates a 10,000-input P2PKH
// transaction through ValidateTransaction while `burners` goroutines keep
// Go code running on every P, at several settings of
// script.SetParallelInputHelpers: 0 (no helpers, every input on the calling
// thread), 2, GOMAXPROCS/4 and automatic (-1). It reports, per validation,
// process CPU time (user + system, all threads), and the burner goroutines'
// throughput over the same interval: how much faster helpers make the
// transaction and how much they slow the Go work down. The "none" mode
// validates nothing (it sleeps 50 ms per op) and gives the burners' rate on
// their own.
//
//	go test -run '^$' -bench GoLoad -benchtime 10x
func BenchmarkValidateTransaction_GoLoad(b *testing.B) {
	const nInputs = 10000
	etx, utxoHeights := makeManyInputP2PKHTx(b, nInputs)
	procs := runtime.GOMAXPROCS(0)

	for _, burners := range []int{0, procs} {
		for _, mode := range []struct {
			name    string
			helpers int64
		}{
			{"none", 0},
			{"helpers=0", 0},
			{"helpers=2", 2},
			{fmt.Sprintf("helpers=%d", max(1, procs/4)), int64(max(1, procs/4))},
			{"helpers=auto", -1},
		} {
			if mode.name == "none" && burners == 0 {
				continue
			}
			b.Run(fmt.Sprintf("gomaxprocs=%d/burners=%d/%s", procs, burners, mode.name), func(b *testing.B) {
				validate := mode.name != "none"
				if validate {
					previous := bdkscript.GetParallelInputHelpers()
					bdkscript.SetParallelInputHelpers(mode.helpers)
					defer bdkscript.SetParallelInputHelpers(previous)
					// One unmeasured call checks the tx and starts the helper pool.
					if err := se.ValidateTransaction(etx, utxoHeights, inputsBlockHeight, true); err != nil {
						b.Fatalf("ValidateTransaction: %v", err)
					}
				}

				var units atomic.Uint64
				stop := make(chan struct{})
				var wg sync.WaitGroup
				for i := 0; i < burners; i++ {
					wg.Add(1)
					go func(seed uint64) {
						defer wg.Done()
						x := seed
						for {
							select {
							case <-stop:
								return
							default:
							}
							for j := 0; j < 20000; j++ {
								x = x*6364136223846793005 + 1442695040888963407
							}
							if x == 0 {
								units.Add(0) // keeps x live
							}
							units.Add(1)
						}
					}(uint64(i) + 1)
				}

				cpu0 := processCPU()
				units0 := units.Load()
				start := time.Now()
				b.ResetTimer()
				for i := 0; i < b.N; i++ {
					if !validate {
						time.Sleep(50 * time.Millisecond)
						continue
					}
					if err := se.ValidateTransaction(etx, utxoHeights, inputsBlockHeight, true); err != nil {
						b.Fatalf("ValidateTransaction: %v", err)
					}
				}
				b.StopTimer()
				elapsed := time.Since(start)
				cpu := processCPU() - cpu0
				burnt := units.Load() - units0
				close(stop)
				wg.Wait()

				b.ReportMetric(float64(cpu.Microseconds())/1000/float64(b.N), "cpu-ms/op")
				if burners > 0 {
					b.ReportMetric(float64(burnt)/elapsed.Seconds(), "burner-units/s")
				}
			})
		}
	}
}

func processCPU() time.Duration {
	var usage syscall.Rusage
	if err := syscall.Getrusage(syscall.RUSAGE_SELF, &usage); err != nil {
		return 0
	}
	return time.Duration(usage.Utime.Nano() + usage.Stime.Nano())
}
