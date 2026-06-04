#!/usr/bin/env sh
set -eu

OUT_DIR="${OUT_DIR:-coverage}"
LOG_FILE="${LOG_FILE:-$OUT_DIR/large_grid_10k.log}"
MPI_RANKS="${MPI_RANKS:-4}"
MPI_EXTRA_ARGS="${MPI_EXTRA_ARGS:---oversubscribe}"

mkdir -p "$OUT_DIR"

{
    echo "# Large Grid Smoke Test"
    echo "date=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "cwd=$(pwd)"
    echo ""

    echo "## Serial 10k x 10k"
    ./bin/life_serial --width 10000 --height 10000 --steps 5 --pattern random --seed 7 --density 0.30
    echo ""

    echo "## MPI 10k x 10k (2D)"
    mpirun -n "$MPI_RANKS" $MPI_EXTRA_ARGS ./bin/life_mpi --width 10000 --height 10000 --steps 5 --pattern random --seed 7 --density 0.30 --decomposition 2d --csv
} | tee "$LOG_FILE"

echo "Saved: $LOG_FILE"