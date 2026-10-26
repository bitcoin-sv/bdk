#!/usr/bin/env bash
# Builds one bdk-bench binary per gobdk version into $OUT (default ./bin).
# Usage: build.sh [v1.3.0 v1.3.1 v1.4.0 label=<version or commit> ...]
# A label=version argument names the binary bdk-bench-<label>; anything other than a
# v1.3.x or v1.4.0 release is built with parallel input verification (bdk67) unless
# TAGS says otherwise.
set -euo pipefail
SRC=$(cd "$(dirname "$0")" && pwd)
OUT=${OUT:-$SRC/bin}
WORK=${WORK:-$(mktemp -d)}
VERSIONS=("$@")
[ ${#VERSIONS[@]} -eq 0 ] && VERSIONS=(v1.3.0 v1.3.1 v1.4.0)
mkdir -p "$OUT"
for arg in "${VERSIONS[@]}"; do
	v=${arg#*=}
	label=${arg%%=*}
	d="$WORK/$label"
	rm -rf "$d"
	mkdir -p "$d"
	cp "$SRC"/*.go "$SRC"/go.mod "$d"/
	[ -f "$SRC/go.sum" ] && cp "$SRC/go.sum" "$d"/
	tags=""
	# SHA256_BACKEND() first appears in v1.4.0.
	# Parallel input verification (SetParallelInputHelpers) is in neither release.
	case "$v" in v1.3.*) tags="" ;; v1.4.0) tags="bdk14" ;; *) tags="bdk14 bdk67" ;; esac
	# TAGS overrides, e.g. TAGS=bdk14 for a later commit without parallel input verification.
	tags=${TAGS-$tags}
	(cd "$d" &&
		GOFLAGS=-mod=mod go get "github.com/bitcoin-sv/bdk/module/gobdk@$v" &&
		GOFLAGS=-mod=mod go mod tidy &&
		go build -tags "$tags" -trimpath -o "$OUT/bdk-bench-$label" .)
	echo "built $OUT/bdk-bench-$label ($(grep 'bitcoin-sv/bdk/module/gobdk ' "$d/go.mod" | tr -s ' \t' ' '))"
done
