#!/usr/bin/env bash
# Runs every shape for every version, ROUNDS times, one process per
# (version, shape, round), versions interleaved and rotated each round.
# Then, on x86_64, ROUNDS more rounds of v1.4.0 with BDK_SHA256_FORCE_SCALAR=1.
#
# Env: ROUNDS (10), CORE (2), TIMEOUT seconds per process (120),
#      VERSIONS ("v1.3.0 v1.3.1 v1.4.0"), BIN (./bin), OUT (./results), SCALAR_PASS (auto).
set -uo pipefail
SRC=$(cd "$(dirname "$0")" && pwd)
ROUNDS=${ROUNDS:-10}
CORE=${CORE:-2}
TIMEOUT=${TIMEOUT:-120}
read -r -a VERSIONS <<< "${VERSIONS:-v1.3.0 v1.3.1 v1.4.0}"
BIN=${BIN:-$SRC/bin}
OUT=${OUT:-$SRC/results}
CACHE=$OUT/cache
mkdir -p "$OUT" "$CACHE"
LOG=$OUT/progress.log
log() { echo "$(date -u +%FT%TZ) $*" >> "$LOG"; }

arch=$(uname -m)
if [ -z "${SCALAR_PASS:-}" ]; then
	[ "$arch" = x86_64 ] && SCALAR_PASS=1 || SCALAR_PASS=0
fi

host_info() {
	local token itype
	token=$(curl -sf -m 2 -X PUT http://169.254.169.254/latest/api/token -H 'X-aws-ec2-metadata-token-ttl-seconds: 60' || true)
	itype=$(curl -sf -m 2 -H "X-aws-ec2-metadata-token: $token" http://169.254.169.254/latest/meta-data/instance-type || echo unknown)
	python3 - "$itype" "$CORE" <<'PY'
import json, platform, subprocess, sys
def sh(c):
    try: return subprocess.run(c, shell=True, capture_output=True, text=True, timeout=10).stdout.strip()
    except Exception as e: return str(e)
print(json.dumps({
    "instance_type": sys.argv[1], "core": sys.argv[2], "arch": platform.machine(),
    "kernel": platform.release(), "go": sh("go version"),
    "lscpu_model": sh("lscpu | sed -n 's/^Model name: *//p' | head -1"),
    "cpuinfo_model": sh("sed -n 's/^model name\\s*: //p' /proc/cpuinfo | head -1"),
    "nproc": sh("nproc"), "lscpu": sh("lscpu"), "lscpu_e": sh("lscpu -e"),
    "os": sh(". /etc/os-release && echo $PRETTY_NAME"), "gcc": sh("gcc --version | head -1"),
}, indent=1))
PY
}

# run_one VERSION SHAPE ROUND TAG [ENV...]
run_one() {
	local v=$1 sh=$2 r=$3 tag=$4
	shift 4
	local skip="$OUT/skip.$tag.$sh"
	if [ -e "$skip" ]; then
		echo "{\"shape\":\"$sh\",\"round\":$r,\"status\":\"skipped_after_timeout\",\"version\":\"$v\"}" >> "$OUT/$tag.jsonl"
		return
	fi
	local t0=$SECONDS
	env GOMAXPROCS=1 "$@" timeout -k 5 "$TIMEOUT" taskset -c "$CORE" "$BIN/bdk-bench-$v" \
		-shape "$sh" -round "$r" -cache "$CACHE" -json "$OUT/$tag.jsonl" -bench "$OUT/$tag.bench.txt" 2>> "$OUT/$tag.stderr"
	local rc=$?
	if [ $rc -eq 124 ] || [ $rc -eq 137 ]; then
		echo "{\"shape\":\"$sh\",\"round\":$r,\"status\":\"timeout\",\"timeout_s\":$TIMEOUT,\"version\":\"$v\"}" >> "$OUT/$tag.jsonl"
		touch "$skip"
		log "TIMEOUT $tag $sh round $r"
	elif [ $rc -ne 0 ]; then
		log "FAIL rc=$rc $tag $sh round $r"
		echo fail >> "$OUT/failures"
	fi
	log "done $tag $sh round $r rc=$rc $((SECONDS - t0))s"
}

host_info > "$OUT/host.json"
mapfile -t SHAPES < <("$BIN/bdk-bench-${VERSIONS[-1]}" -list)
log "start rounds=$ROUNDS versions=${VERSIONS[*]} shapes=${#SHAPES[@]} core=$CORE scalar_pass=$SCALAR_PASS"

n=${#VERSIONS[@]}
for ((r = 1; r <= ROUNDS; r++)); do
	for sh in "${SHAPES[@]}"; do
		for ((k = 0; k < n; k++)); do
			v=${VERSIONS[$(((k + r - 1) % n))]}
			run_one "$v" "$sh" "$r" "$v"
		done
	done
	log "round $r complete"
done

if [ "$SCALAR_PASS" = 1 ]; then
	for ((r = 1; r <= ROUNDS; r++)); do
		for sh in "${SHAPES[@]}"; do
			run_one v1.4.0 "$sh" "$r" v1.4.0-scalar BDK_SHA256_FORCE_SCALAR=1
		done
		log "scalar round $r complete"
	done
fi

log "finished"
if [ -s "$OUT/failures" ]; then echo FAILED > "$OUT/DONE"; else echo OK > "$OUT/DONE"; fi
