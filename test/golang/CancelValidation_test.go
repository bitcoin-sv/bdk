package main

import (
	"bytes"
	"context"
	"encoding/binary"
	"encoding/hex"
	"errors"
	"fmt"
	"math/rand"
	"runtime"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	goscript "github.com/bitcoin-sv/bdk/module/gobdk/script"
	pkgerrors "github.com/pkg/errors"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// Mainnet tx 7be4fa421844154ec4105894def768a8bcd80da25792947d585274ce38c07105, extended;
// valid in consensus mode at cancelTestValidHeight.
const cancelTestValidTxHex = "020000000000000000ef023f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44000000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffffa73018250000000017a91413402e079464ec2a85e5a613732c78b0613fcc65873f6c667203b47ce2fed8c8bcc78d764c39da9c0094f1a49074e05f66910e9c44010000006b4c69522102401d5481712745cf7ada12b7251c85ca5f1b8b6c859c7e81b8002a85b0f36d3c21039d8b1e461715ddd4d10806125be8592e6f48fb69e4c31699ce6750da1c9eaeb32103af3b35d4ad547fd1ce102bbd5cce36de2277723796f1b4001ec0ea6a1db6474053aeffffffff34b82f000000000017a91413402e079464ec2a85e5a613732c78b0613fcc65870187e74725000000001976a9141be3d23725148a90807ee6df191bcdfcf083a3b288ac00000000"

const cancelTestValidHeight = int32(632099)

var cancelTestValidUtxo = []int32{631924, 631924}

func cancelTestValidTx(t testing.TB) []byte {
	tx, err := hex.DecodeString(cancelTestValidTxHex)
	require.NoError(t, err)
	return tx
}

// cancelTestInvalidTx is the valid fixture with one byte of the first input's
// signature changed, so it fails script verification with a verdict.
func cancelTestInvalidTx(t testing.TB) []byte {
	tx := cancelTestValidTx(t)
	tx[60] ^= 0x01
	return tx
}

// Regtest heights at which the spent outputs and the block are post-Chronicle.
const (
	cancelTestLongBlockHeight = int32(20000)
	cancelTestLongUtxoHeight  = int32(19000)
)

func cancelTestVarInt(b *bytes.Buffer, n uint64) {
	switch {
	case n < 0xfd:
		b.WriteByte(byte(n))
	case n <= 0xffff:
		b.WriteByte(0xfd)
		_ = binary.Write(b, binary.LittleEndian, uint16(n))
	case n <= 0xffffffff:
		b.WriteByte(0xfe)
		_ = binary.Write(b, binary.LittleEndian, uint32(n))
	default:
		b.WriteByte(0xff)
		_ = binary.Write(b, binary.LittleEndian, n)
	}
}

// cancelTestExtendedTx builds a one-input, one-output extended transaction.
func cancelTestExtendedTx(scriptSig, lockingScript []byte) []byte {
	var b bytes.Buffer
	_ = binary.Write(&b, binary.LittleEndian, uint32(2))
	b.Write([]byte{0, 0, 0, 0, 0, 0xEF})
	cancelTestVarInt(&b, 1)
	prevTxID := make([]byte, 32)
	prevTxID[0] = 1
	b.Write(prevTxID)
	_ = binary.Write(&b, binary.LittleEndian, uint32(0))
	cancelTestVarInt(&b, uint64(len(scriptSig)))
	b.Write(scriptSig)
	_ = binary.Write(&b, binary.LittleEndian, uint32(0xffffffff))
	_ = binary.Write(&b, binary.LittleEndian, uint64(100000))
	cancelTestVarInt(&b, uint64(len(lockingScript)))
	b.Write(lockingScript)
	cancelTestVarInt(&b, 1)
	_ = binary.Write(&b, binary.LittleEndian, uint64(1000))
	cancelTestVarInt(&b, 1)
	b.WriteByte(0x51) // OP_1
	_ = binary.Write(&b, binary.LittleEndian, uint32(0))
	return b.Bytes()
}

// cancelTestLongRunningTx is a valid transaction whose script takes seconds: the
// unlocking script pushes a 1 MB element and the locking script hashes it rounds
// times (OP_DUP OP_SHA256 OP_DROP), so the interpreter reaches a cancellation check
// every few milliseconds.
func cancelTestLongRunningTx(rounds int) []byte {
	element := bytes.Repeat([]byte{0x01}, 1<<20)
	var scriptSig bytes.Buffer
	scriptSig.WriteByte(0x4e) // OP_PUSHDATA4
	_ = binary.Write(&scriptSig, binary.LittleEndian, uint32(len(element)))
	scriptSig.Write(element)
	lockingScript := bytes.Repeat([]byte{0x76, 0xa8, 0x75}, rounds) // OP_DUP OP_SHA256 OP_DROP
	return cancelTestExtendedTx(scriptSig.Bytes(), lockingScript)
}

func requireCancelled(t *testing.T, err error, cause error) {
	t.Helper()
	require.Error(t, err)
	assert.True(t, errors.Is(err, goscript.ErrCancelled), "expected ErrCancelled, got %v", err)
	var cancelledErr *goscript.CancelledError
	require.True(t, errors.As(err, &cancelledErr), "expected *CancelledError, got %T", err)
	if cause != nil {
		assert.True(t, errors.Is(cancelledErr.ContextCause(), cause), "expected cause %v, got %v", cause, cancelledErr.ContextCause())
	}
	// The cause is not in the error chain: errors.Is/As on err see only the cancellation.
	assert.False(t, errors.Is(err, context.Canceled) || errors.Is(err, context.DeadlineExceeded),
		"the context error must not be in the chain: %v", err)
	var scriptErr goscript.ScriptError
	assert.False(t, errors.As(err, &scriptErr), "a cancelled call must not report a script error: %v", err)
	var dosErr goscript.DoSError
	assert.False(t, errors.As(err, &dosErr), "a cancelled call must not report a verdict: %v", err)
}

func cancelledContext() context.Context {
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	return ctx
}

func TestValidateTransactionContextCancelledBeforeStart(t *testing.T) {
	se := goscript.NewTxValidator("main")
	for _, consensus := range []bool{true, false} {
		err := se.ValidateTransactionContext(cancelledContext(), cancelTestValidTx(t), cancelTestValidUtxo, cancelTestValidHeight, consensus)
		requireCancelled(t, err, context.Canceled)
	}
}

func TestContextVariantsCancelledBeforeStart(t *testing.T) {
	se := goscript.NewTxValidator("main")
	tx := cancelTestValidTx(t)
	malformed := []byte{0x01, 0x02, 0x03}
	flags := []uint32{869935, 869935}

	for _, consensus := range []bool{true, false} {
		for _, etx := range [][]byte{tx, malformed} {
			requireCancelled(t, se.VerifyScriptContext(cancelledContext(), etx, cancelTestValidUtxo, cancelTestValidHeight, consensus), context.Canceled)
			requireCancelled(t, se.VerifyScriptWithCustomFlagsContext(cancelledContext(), etx, cancelTestValidUtxo, cancelTestValidHeight, consensus, flags), context.Canceled)
			requireCancelled(t, se.ValidateTransactionContext(cancelledContext(), etx, cancelTestValidUtxo, cancelTestValidHeight, consensus), context.Canceled)

			batch := goscript.NewValidateBatch()
			require.NoError(t, batch.Add(etx, cancelTestValidUtxo, cancelTestValidHeight, consensus))
			require.NoError(t, batch.Add(etx, cancelTestValidUtxo, cancelTestValidHeight, consensus))
			results := se.ValidateBatchContext(cancelledContext(), batch)
			require.Len(t, results, 2)
			for _, err := range results {
				requireCancelled(t, err, context.Canceled)
			}
		}
	}
}

// longRunningValidator admits cancelTestLongRunningTx in policy mode as well.
func longRunningValidator(t testing.TB) *goscript.TxValidator {
	se := goscript.NewTxValidator("regtest")
	require.NotNil(t, se)
	require.NoError(t, se.SetMaxScriptSizePolicy(4<<20))
	require.NoError(t, se.SetMaxOpsPerScriptPolicy(1000000))
	return se
}

func TestValidateTransactionContextDeadlineMidScript(t *testing.T) {
	se := longRunningValidator(t)
	tx := cancelTestLongRunningTx(2000) // seconds uncancelled

	for _, consensus := range []bool{true, false} {
		ctx, cancel := context.WithTimeout(context.Background(), 50*time.Millisecond)
		start := time.Now()
		err := se.ValidateTransactionContext(ctx, tx, []int32{cancelTestLongUtxoHeight}, cancelTestLongBlockHeight, consensus)
		elapsed := time.Since(start)
		cancel()
		requireCancelled(t, err, context.DeadlineExceeded)
		assert.Less(t, elapsed, time.Second, "consensus=%v: cancellation should land within a few ms of the deadline", consensus)
	}
}

func TestContextCancelCauseIsCarried(t *testing.T) {
	se := longRunningValidator(t)
	tx := cancelTestLongRunningTx(2000)
	slowLane := errors.New("moved to the slow lane")

	ctx, cancel := context.WithCancelCause(context.Background())
	time.AfterFunc(20*time.Millisecond, func() { cancel(slowLane) })
	err := se.VerifyScriptContext(ctx, tx, []int32{cancelTestLongUtxoHeight}, cancelTestLongBlockHeight, true)
	requireCancelled(t, err, slowLane)
	var cancelledErr *goscript.CancelledError
	require.True(t, errors.As(err, &cancelledErr))
	assert.False(t, errors.Is(cancelledErr.ContextCause(), context.DeadlineExceeded))
}

// errShape names a result by its type, domain and code, so two results compare equal
// only when a caller would classify them the same way.
func errShape(err error) string {
	var scriptErr goscript.ScriptError
	var dosErr goscript.DoSError
	var abiErr goscript.ABIError
	switch {
	case err == nil:
		return "ok"
	case errors.Is(err, goscript.ErrCancelled):
		return "cancelled"
	case errors.As(err, &abiErr):
		return fmt.Sprintf("abi:%d", abiErr.Code())
	case errors.As(err, &dosErr):
		return fmt.Sprintf("dos:%d", dosErr.Code())
	case errors.As(err, &scriptErr):
		return fmt.Sprintf("script:%d", scriptErr.Code())
	default:
		return fmt.Sprintf("other:%T:%v", err, err)
	}
}

func errShapes(errs []error) []string {
	if errs == nil {
		return nil
	}
	shapes := make([]string, len(errs))
	for i, err := range errs {
		shapes[i] = errShape(err)
	}
	return shapes
}

// nilDoneContext can never be cancelled: Done returns nil, as context.Background's does.
type nilDoneContext struct{ context.Context }

func (nilDoneContext) Done() <-chan struct{} { return nil }

// cancelTestP2SHOnlyFlags, one entry per input of the valid fixture, enable P2SH
// evaluation without the post-Genesis flags. The fixture's inputs spend P2SH outputs
// whose redeem scripts the calculated (post-Genesis) flags never run; run with these
// flags, they fail.
var cancelTestP2SHOnlyFlags = []uint32{1, 1}

// A cause that is itself a validation error stays out of the error chain: a caller
// checking errors.As(err, &ScriptError) must not take a cancelled call for a verdict.
func TestContextCauseIsNotAVerdict(t *testing.T) {
	se := longRunningValidator(t)
	tx := cancelTestLongRunningTx(2000)
	verdictCause := goscript.NewScriptError(goscript.SCRIPT_ERR_EVAL_FALSE)

	ctx, cancel := context.WithCancelCause(context.Background())
	time.AfterFunc(20*time.Millisecond, func() { cancel(verdictCause) })
	err := se.ValidateTransactionContext(ctx, tx, []int32{cancelTestLongUtxoHeight}, cancelTestLongBlockHeight, true)
	requireCancelled(t, err, verdictCause)

	pre, cancelPre := context.WithCancelCause(context.Background())
	cancelPre(verdictCause)
	requireCancelled(t, se.ValidateTransactionContext(pre, tx, []int32{cancelTestLongUtxoHeight}, cancelTestLongBlockHeight, true), verdictCause)
}

// Legacy cause walkers must stop at the cancelled result, even for a script cause.
func TestContextCauseIsNotAVerdictThroughPkgErrors(t *testing.T) {
	ctx, cancel := context.WithCancelCause(context.Background())
	cause := goscript.NewScriptError(goscript.SCRIPT_ERR_EVAL_FALSE)
	cancel(cause)
	se := goscript.NewTxValidator("main")
	err := se.ValidateTransactionContext(ctx, nil, nil, 0, true)
	requireCancelled(t, err, cause)
	var cancelledErr *goscript.CancelledError
	require.ErrorAs(t, err, &cancelledErr)
	assert.Same(t, cancelledErr, pkgerrors.Cause(err))
	assert.Same(t, cancelledErr, pkgerrors.Cause(pkgerrors.Wrap(err, "validation")))
	var scriptErr goscript.ScriptError
	assert.False(t, errors.As(err, &scriptErr))
}

// A context that is never cancelled, or cancelled only after the call returned, gives
// exactly the result of the non-context function, compared by error type, domain and
// code.
func TestContextResultsMatchWhenNotCancelled(t *testing.T) {
	se := goscript.NewTxValidator("main")
	customFlags := map[string][]uint32{
		"nil": nil, "empty": {}, "calculated-equivalent": {869935, 869935}, "p2sh-only": cancelTestP2SHOnlyFlags,
	}

	for name, tx := range map[string][]byte{"valid": cancelTestValidTx(t), "invalid": cancelTestInvalidTx(t)} {
		for _, consensus := range []bool{true, false} {
			live, cancelLive := context.WithCancel(context.Background())
			contexts := map[string]context.Context{
				"background": context.Background(), "live": live, "nil-done": nilDoneContext{context.Background()},
			}
			for ctxName, ctx := range contexts {
				msg := fmt.Sprintf("%s/%s/consensus=%v", name, ctxName, consensus)
				assert.Equal(t, errShape(se.ValidateTransaction(tx, cancelTestValidUtxo, cancelTestValidHeight, consensus)),
					errShape(se.ValidateTransactionContext(ctx, tx, cancelTestValidUtxo, cancelTestValidHeight, consensus)), msg)
				assert.Equal(t, errShape(se.VerifyScript(tx, cancelTestValidUtxo, cancelTestValidHeight, consensus)),
					errShape(se.VerifyScriptContext(ctx, tx, cancelTestValidUtxo, cancelTestValidHeight, consensus)), msg)
				for flagsName, flags := range customFlags {
					assert.Equal(t, errShape(se.VerifyScriptWithCustomFlags(tx, cancelTestValidUtxo, cancelTestValidHeight, consensus, flags)),
						errShape(se.VerifyScriptWithCustomFlagsContext(ctx, tx, cancelTestValidUtxo, cancelTestValidHeight, consensus, flags)),
						msg+" flags="+flagsName)
				}

				batch := goscript.NewValidateBatch()
				require.NoError(t, batch.Add(tx, cancelTestValidUtxo, cancelTestValidHeight, consensus))
				assert.Equal(t, errShapes(se.ValidateBatch(batch)), errShapes(se.ValidateBatchContext(ctx, batch)), msg)
				empty := goscript.NewValidateBatch()
				assert.Equal(t, errShapes(se.ValidateBatch(empty)), errShapes(se.ValidateBatchContext(ctx, empty)), msg+" empty batch")
				assert.Nil(t, se.ValidateBatchContext(ctx, nil), msg+" nil batch")
			}

			// Cancelled after the call returned: the verdict stands.
			err := se.ValidateTransactionContext(live, tx, cancelTestValidUtxo, cancelTestValidHeight, consensus)
			cancelLive()
			assert.Equal(t, errShape(se.ValidateTransaction(tx, cancelTestValidUtxo, cancelTestValidHeight, consensus)), errShape(err))
		}
	}

	// The fixtures keep the properties the comparisons rely on.
	var scriptErr goscript.ScriptError
	require.True(t, errors.As(se.ValidateTransaction(cancelTestInvalidTx(t), cancelTestValidUtxo, cancelTestValidHeight, true), &scriptErr),
		"the invalid fixture must fail with a script error")
	require.NoError(t, se.VerifyScript(cancelTestValidTx(t), cancelTestValidUtxo, cancelTestValidHeight, true))
	require.True(t, errors.As(se.VerifyScriptWithCustomFlags(cancelTestValidTx(t), cancelTestValidUtxo, cancelTestValidHeight, true, cancelTestP2SHOnlyFlags), &scriptErr),
		"the P2SH-only flags must change the valid fixture's verdict")
	// A pre-cancelled context on an empty batch gives what the plain call gives.
	assert.Nil(t, se.ValidateBatchContext(cancelledContext(), goscript.NewValidateBatch()))
}

func TestValidateBatchContextCancelledPartWay(t *testing.T) {
	se := longRunningValidator(t)
	valid := cancelTestLongRunningTx(1)
	long := cancelTestLongRunningTx(2000)
	invalid := cancelTestExtendedTx([]byte{0x00}, []byte{0x69}) // OP_0 / OP_VERIFY
	utxo := []int32{cancelTestLongUtxoHeight}

	batch := goscript.NewValidateBatch()
	for _, tx := range [][]byte{valid, invalid, long, valid} {
		require.NoError(t, batch.Add(tx, utxo, cancelTestLongBlockHeight, true))
	}

	ctx, cancel := context.WithTimeout(context.Background(), 100*time.Millisecond)
	defer cancel()
	results := se.ValidateBatchContext(ctx, batch)
	require.Len(t, results, 4)
	assert.NoError(t, results[0])
	var scriptErr goscript.ScriptError
	assert.True(t, errors.As(results[1], &scriptErr), "finished entry keeps its verdict: %v", results[1])
	requireCancelled(t, results[2], context.DeadlineExceeded)
	requireCancelled(t, results[3], context.DeadlineExceeded)
}

// Many goroutines share one validator, each with its own context cancelled at a random
// moment. Every result is the verdict or a cancellation, never anything else. Of the
// long calls (tens of ms) whose context was still live when they began, most must
// come back cancelled: those cancels can only arrive through the flag, since the Go
// early return needs a context that is already done. Under -race this also checks the
// Go side (the AfterFunc store, the flag's lifetime); the race detector does not see
// the C++ read of the flag, which the ThreadSanitizer CI job covers.
func TestContextConcurrentCancellation(t *testing.T) {
	mainSE := goscript.NewTxValidator("main")
	longSE := longRunningValidator(t)
	valid := cancelTestValidTx(t)
	long := cancelTestLongRunningTx(20)
	longUtxo := []int32{cancelTestLongUtxoHeight}

	var wg sync.WaitGroup
	var liveLongCalls, cancelledMidCall atomic.Int64
	for g := 0; g < 16; g++ {
		wg.Add(1)
		go func(seed int64) {
			defer wg.Done()
			rng := rand.New(rand.NewSource(seed))
			for i := 0; i < 40; i++ {
				// At least 1 ms, so the call has started before the deadline; the long
				// transaction takes tens of ms.
				ctx, cancel := context.WithTimeout(context.Background(), time.Duration(1000+rng.Intn(3000))*time.Microsecond)
				var err error
				if i%2 == 0 {
					err = mainSE.ValidateTransactionContext(ctx, valid, cancelTestValidUtxo, cancelTestValidHeight, i%4 == 0)
				} else {
					liveAtStart := ctx.Err() == nil
					err = longSE.ValidateTransactionContext(ctx, long, longUtxo, cancelTestLongBlockHeight, i%3 == 0)
					if liveAtStart {
						liveLongCalls.Add(1)
						if errors.Is(err, goscript.ErrCancelled) {
							cancelledMidCall.Add(1)
						}
					}
				}
				cancel()
				if err != nil && !errors.Is(err, goscript.ErrCancelled) {
					t.Errorf("unexpected result %v", err)
				}
			}
		}(int64(g))
	}
	wg.Wait()
	require.Positive(t, liveLongCalls.Load())
	assert.GreaterOrEqual(t, 2*cancelledMidCall.Load(), liveLongCalls.Load(),
		"only %d of %d long calls with a live context were cancelled while running", cancelledMidCall.Load(), liveLongCalls.Load())
}

// chanContext is a cancellable context the context package does not recognise, so
// context.AfterFunc has to watch it with a goroutine.
type chanContext struct {
	context.Context
	done chan struct{}
	once sync.Once
}

func newChanContext() *chanContext {
	return &chanContext{Context: context.Background(), done: make(chan struct{})}
}
func (c *chanContext) Done() <-chan struct{} { return c.done }
func (c *chanContext) Err() error {
	select {
	case <-c.done:
		return context.Canceled
	default:
		return nil
	}
}
func (c *chanContext) cancel() { c.once.Do(func() { close(c.done) }) }

func TestContextCallsLeaveNoGoroutines(t *testing.T) {
	se := goscript.NewTxValidator("main")
	tx := cancelTestValidTx(t)
	before := runtime.NumGoroutine()

	for i := 0; i < 5000; i++ {
		ctx, cancel := context.WithCancel(context.Background())
		_ = se.ValidateTransactionContext(ctx, tx, cancelTestValidUtxo, cancelTestValidHeight, true)
		cancel()

		custom := newChanContext()
		_ = se.ValidateTransactionContext(custom, tx, cancelTestValidUtxo, cancelTestValidHeight, true)
		if i%2 == 0 {
			custom.cancel()
		}
	}

	deadline := time.Now().Add(5 * time.Second)
	for runtime.NumGoroutine() > before && time.Now().Before(deadline) {
		time.Sleep(10 * time.Millisecond)
	}
	assert.LessOrEqual(t, runtime.NumGoroutine(), before, "goroutines left behind by the context calls")
}

// Mainnet tx d43ad4d4b46632b311b844117a5a9e9598afd1ee13e95d98106bf2adeae0bad7, extended:
// one P2PKH input (one signature check), valid at block 620940.
const cancelTestP2PKHTxHex = "010000000000000000ef0120fa0d2c5974cfe6e3aec71f7f6539cfa1c1e474082d2cdb41fb830f6267b7d7000000006b4830450221008788b545ebd6ebcb15f938045b71c1fa7efafd55d1f4e64e96602a04f3214cda0220717ddadfa7d1dc6a22ccb077350aef7a2073ee58fe780d86b77a31c24c664a21412103ef28c47337b05ec3f14b63d904db7ae023e897389dbdbf531221e13fd5e5b105ffffffffdc3de103000000001976a91437fb14a40d021abbb1763497f963a130286d1ad188ac017239e103000000001976a914962eba38504bcfb140ff0246afa795658812b42788ac00000000"

// Prints the cost of a live context over the plain call to the test log (ctest keeps
// it in LastTest.log), per platform. Not an assertion: CI machines are too noisy.
func TestContextOverheadReport(t *testing.T) {
	se := goscript.NewTxValidator("main")
	tx, err := hex.DecodeString(cancelTestP2PKHTxHex)
	require.NoError(t, err)
	utxo := []int32{574441}
	const height = int32(620940)
	require.NoError(t, se.ValidateTransaction(tx, utxo, height, true))
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	plain := testing.Benchmark(func(b *testing.B) {
		for i := 0; i < b.N; i++ {
			_ = se.ValidateTransaction(tx, utxo, height, true)
		}
	})
	withCtx := testing.Benchmark(func(b *testing.B) {
		for i := 0; i < b.N; i++ {
			_ = se.ValidateTransactionContext(ctx, tx, utxo, height, true)
		}
	})
	fmt.Printf("[context_overhead] goos=%s goarch=%s inputs=1 plain_ns=%d context_ns=%d delta_ns=%d\n",
		runtime.GOOS, runtime.GOARCH, plain.NsPerOp(), withCtx.NsPerOp(), withCtx.NsPerOp()-plain.NsPerOp())
}
