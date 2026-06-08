#!/usr/bin/env sh
set -eu

# Prefer mpirun, but accept mpiexec for runtime portability.
MPI_RUNNER=""

if command -v mpirun >/dev/null 2>&1; then
    MPI_RUNNER="mpirun"
elif command -v mpiexec >/dev/null 2>&1; then
    MPI_RUNNER="mpiexec"
else
    echo "mpirun/mpiexec not found" >&2
    exit 1
fi

OUT_DIR="${OUT_DIR:-coverage}"
DECOMP="${DECOMP:-1d}"
MPI_EXTRA_ARGS="${MPI_EXTRA_ARGS:-}"
mkdir -p "$OUT_DIR"

STRONG_CSV="$OUT_DIR/strong_scaling.csv"
WEAK_CSV="$OUT_DIR/weak_scaling.csv"

BASE_W="${1:-1024}"
BASE_H="${2:-1024}"
STEPS="${3:-200}"
shift $(( $# > 3 ? 3 : $# )) || true
RANKS="${*:-1 2 4}"

# Each run appends one machine-readable line for downstream summary scripts.
echo "ranks,width,height,steps,total_seconds,communication_seconds,computation_seconds" > "$STRONG_CSV"
echo "ranks,width,height,steps,total_seconds,communication_seconds,computation_seconds" > "$WEAK_CSV"

run_case() {
    phase="$1"
    rank_count="$2"
    width="$3"
    height="$4"

    if ! "$MPI_RUNNER" $MPI_EXTRA_ARGS -n "$rank_count" ./bin/life_mpi --width "$width" --height "$height" --steps "$STEPS" --pattern random --seed 7 --density 0.30 --csv --decomposition "$DECOMP"; then
        echo "benchmark.sh: ${phase} scaling failed for ranks=${rank_count}, grid=${width}x${height}, decomposition=${DECOMP}" >&2
        echo "benchmark.sh: if Open MPI reports insufficient slots on a local machine, rerun with MPI_EXTRA_ARGS=--oversubscribe or reduce the requested ranks" >&2
        exit 1
    fi
}

# Strong scaling: fixed problem size, increasing rank count.
for rank_count in $RANKS; do
    run_case strong "$rank_count" "$BASE_W" "$BASE_H" >> "$STRONG_CSV"
done

# Weak scaling: global grid grows approximately proportional with rank count.
for rank_count in $RANKS; do
    weak_h=$(( BASE_H * rank_count ))
    run_case weak "$rank_count" "$BASE_W" "$weak_h" >> "$WEAK_CSV"
done

echo "Generated: $STRONG_CSV"
echo "Generated: $WEAK_CSV"