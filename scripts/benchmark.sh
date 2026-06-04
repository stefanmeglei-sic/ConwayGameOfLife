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
mkdir -p "$OUT_DIR"

STRONG_CSV="$OUT_DIR/strong_scaling.csv"
WEAK_CSV="$OUT_DIR/weak_scaling.csv"

BASE_W="${1:-1024}"
BASE_H="${2:-1024}"
STEPS="${3:-200}"
shift $(( $# > 3 ? 3 : $# )) || true
RANKS="${*:-1 2 4 8}"

# Each run appends one machine-readable line for downstream summary scripts.
echo "ranks,width,height,steps,total_seconds,communication_seconds,computation_seconds" > "$STRONG_CSV"
echo "ranks,width,height,steps,total_seconds,communication_seconds,computation_seconds" > "$WEAK_CSV"

# Strong scaling: fixed problem size, increasing rank count.
for rank_count in $RANKS; do
    "$MPI_RUNNER" -n "$rank_count" ./bin/life_mpi --width "$BASE_W" --height "$BASE_H" --steps "$STEPS" --pattern random --seed 7 --density 0.30 --csv --decomposition "$DECOMP" >> "$STRONG_CSV"
done

# Weak scaling: global grid grows approximately proportional with rank count.
for rank_count in $RANKS; do
    weak_h=$(( BASE_H * rank_count ))
    "$MPI_RUNNER" -n "$rank_count" ./bin/life_mpi --width "$BASE_W" --height "$weak_h" --steps "$STEPS" --pattern random --seed 7 --density 0.30 --csv --decomposition "$DECOMP" >> "$WEAK_CSV"
done

echo "Generated: $STRONG_CSV"
echo "Generated: $WEAK_CSV"