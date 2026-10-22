#!/usr/bin/env bash
# Builds one bdk-bench binary per gobdk version into $OUT (default ./bin).
# Usage: build.sh [v1.3.0 v1.3.1 v1.4.0 ...]
set -euo pipefail
SRC=$(cd "$(dirname "$0")" && pwd)
OUT=${OUT:-$SRC/bin}
WORK=${WORK:-$(mktemp -d)}
VERSIONS=("$@")
[ ${#VERSIONS[@]} -eq 0 ] && VERSIONS=(v1.3.0 v1.3.1 v1.4.0)
mkdir -p "$OUT"
for v in "${VERSIONS[@]}"; do
	d="$WORK/$v"
	rm -rf "$d"
	mkdir -p "$d"
	cp "$SRC"/*.go "$SRC"/go.mod "$d"/
	[ -f "$SRC/go.sum" ] && cp "$SRC/go.sum" "$d"/
	tags=""
	# SHA256_BACKEND() first appears in v1.4.0.
	case "$v" in v1.3.*) tags="" ;; *) tags="bdk14" ;; esac
	(cd "$d" &&
		go mod edit -require="github.com/bitcoin-sv/bdk/module/gobdk@$v" &&
		GOFLAGS=-mod=mod go mod tidy &&
		go build -tags "$tags" -trimpath -o "$OUT/bdk-bench-$v" .)
	echo "built $OUT/bdk-bench-$v"
done
