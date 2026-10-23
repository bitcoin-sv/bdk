#!/usr/bin/env bash
# Compares v1.4.0 with parallel input verification (bin/bdk-bench-pr67) at
# SetParallelInputHelpers 0, 2 and automatic. Every process runs unpinned at
# GOMAXPROCS = online CPUs. Per round: every shape for every config, then the
# concurrency variants, configs rotated each round. Then, if GOLOAD_DIR is set,
# the GoLoad benchmark from that checkout's cgobench package.
#
# Env: ROUNDS (10), TIMEOUT seconds per process (120), BIN (./bin), OUT (./results67),
#      WINDOW concurrency window (10s), GOLOAD_DIR, GOLOAD_COUNT (5).
set -uo pipefail
SRC=$(cd "$(dirname "$0")" && pwd)
ROUNDS=${ROUNDS:-10}
TIMEOUT=${TIMEOUT:-120}
BIN=${BIN:-$SRC/bin}
OUT=${OUT:-$SRC/results67}
WINDOW=${WINDOW:-10s}
GOLOAD_COUNT=${GOLOAD_COUNT:-5}
CACHE=$OUT/cache
mkdir -p "$OUT" "$CACHE"
LOG=$OUT/progress.log
log() { echo "$(date -u +%FT%TZ) $*" >> "$LOG"; }

NPROC=$(nproc)
export GOMAXPROCS=$NPROC
CONFIGS=("v1.4.0:" "pr67:0" "pr67:2" "pr67:-1")
SHAPES=(p2pkh_in1 p2pkh_in10 p2pkh_in15 p2pkh_in16 p2pkh_in17 p2pkh_in32
	p2pkh_in1000 p2pkh_in8000 p2pkh_in86584
	p2pkh_in8000_badsig_first p2pkh_in8000_badsig_last p2pkh_lockscript100KB_in64_badsig_first
	sighash_original_in1000 p2pkh_lockscript10KB_in1000 sha256_preimage10KB_in1000)
# name|args
CONC=("full|-small-goroutines $NPROC"
	"half|-small-goroutines $((NPROC / 2))"
	"smallonly|-small-goroutines $NPROC -large=")

tag_of() {
	local bin=${1%%:*} h=${1#*:}
	case "$h" in "") echo "$bin" ;; -1) echo "$bin-hauto" ;; *) echo "$bin-h$h" ;; esac
}

# run_one CONFIG ROUND ARGS...
run_one() {
	local cfg=$1 r=$2
	shift 2
	local bin=${cfg%%:*} h=${cfg#*:} tag
	tag=$(tag_of "$cfg")
	local hargs=()
	[ -n "$h" ] && hargs=(-helpers "$h")
	local t0=$SECONDS
	timeout -k 5 "$TIMEOUT" "$BIN/bdk-bench-$bin" "${hargs[@]}" -round "$r" -cache "$CACHE" \
		-json "$OUT/$tag.jsonl" -bench "$OUT/$tag.bench.txt" "$@" 2>> "$OUT/$tag.stderr"
	local rc=$?
	if [ $rc -eq 124 ] || [ $rc -eq 137 ]; then
		echo "{\"args\":\"$*\",\"round\":$r,\"status\":\"timeout\",\"timeout_s\":$TIMEOUT}" >> "$OUT/$tag.jsonl"
		log "TIMEOUT $tag $* round $r"
		echo fail >> "$OUT/failures"
	elif [ $rc -ne 0 ]; then
		log "FAIL rc=$rc $tag $* round $r"
		echo fail >> "$OUT/failures"
	fi
	log "done $tag $* round $r rc=$rc $((SECONDS - t0))s"
}

python3 "$SRC/hostinfo.py" > "$OUT/host.json" 2>/dev/null || true
log "start rounds=$ROUNDS nproc=$NPROC configs=${CONFIGS[*]} shapes=${#SHAPES[@]}"

n=${#CONFIGS[@]}
for ((r = 1; r <= ROUNDS; r++)); do
	for sh in "${SHAPES[@]}"; do
		for ((k = 0; k < n; k++)); do
			run_one "${CONFIGS[$(((k + r - 1) % n))]}" "$r" -shape "$sh"
		done
	done
	for c in "${CONC[@]}"; do
		read -r -a cargs <<< "${c#*|}"
		for ((k = 0; k < n; k++)); do
			run_one "${CONFIGS[$(((k + r - 1) % n))]}" "$r" -mode concurrency -window "$WINDOW" "${cargs[@]}"
		done
	done
	log "round $r complete"
done

if [ -n "${GOLOAD_DIR:-}" ]; then
	log "goload start"
	(cd "$GOLOAD_DIR/module/gobdk/cgobench" &&
		timeout -k 10 3600 go test -run '^$' -bench GoLoad -benchtime 10x -count "$GOLOAD_COUNT" -timeout 55m . > "$OUT/goload.txt" 2>&1) ||
		{ log "FAIL goload"; echo fail >> "$OUT/failures"; }
	log "goload done"
fi

log "finished"
if [ -s "$OUT/failures" ]; then echo FAILED > "$OUT/DONE"; else echo OK > "$OUT/DONE"; fi
