package cgobench

import "testing"

// BenchmarkGoNoOp measures the overhead of a pure Go function call.
// This is the baseline - the minimum cost of any function call in Go.
func BenchmarkGoNoOp(b *testing.B) {
	for i := 0; i < b.N; i++ {
		CallGoNoOp()
	}
}

// BenchmarkCGoNoOp measures the overhead of calling a C function that does nothing.
// The difference between this and BenchmarkGoNoOp is the CGO boundary crossing overhead.
func BenchmarkCGoNoOp(b *testing.B) {
	for i := 0; i < b.N; i++ {
		CallCGoNoOp()
	}
}

// BenchmarkCGoSumBytes_1KB measures CGO overhead + data passing for 1KB
func BenchmarkCGoSumBytes_1KB(b *testing.B) {
	data := make([]byte, 1024)
	for i := range data {
		data[i] = byte(i % 256)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_ = CallCGoSumBytes(data)
	}
}

// BenchmarkCGoSumBytes_10KB measures CGO overhead + data passing for 10KB
func BenchmarkCGoSumBytes_10KB(b *testing.B) {
	data := make([]byte, 10*1024)
	for i := range data {
		data[i] = byte(i % 256)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_ = CallCGoSumBytes(data)
	}
}

// BenchmarkCGoSumBytes_100KB measures CGO overhead + data passing for 100KB
func BenchmarkCGoSumBytes_100KB(b *testing.B) {
	data := make([]byte, 100*1024)
	for i := range data {
		data[i] = byte(i % 256)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_ = CallCGoSumBytes(data)
	}
}

// BenchmarkGoSumBytes_1KB is the pure Go equivalent for comparison
func BenchmarkGoSumBytes_1KB(b *testing.B) {
	data := make([]byte, 1024)
	for i := range data {
		data[i] = byte(i % 256)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		var sum int64
		for _, v := range data {
			sum += int64(v)
		}
		_ = sum
	}
}
