#!/usr/bin/env sh
set -eu

MPI_RUNNER=""

if command -v mpirun >/dev/null 2>&1; then
    MPI_RUNNER="mpirun"
elif command -v mpiexec >/dev/null 2>&1; then
    MPI_RUNNER="mpiexec"
else
    echo "mpirun/mpiexec not found" >&2
    exit 1
fi

GRID_W="${1:-1024}"
GRID_H="${2:-1024}"
STEPS="${3:-200}"
shift $(( $# > 3 ? 3 : $# )) || true
RANKS="${*:-1 2 4 8}"

for rank_count in $RANKS; do
    "$MPI_RUNNER" -n "$rank_count" ./bin/life_mpi --width "$GRID_W" --height "$GRID_H" --steps "$STEPS" --pattern random --seed 7 --density 0.30
done