#!/usr/bin/env sh
set -eu

# Tiny deterministic case for quick serial-vs-MPI correctness checks.
MPI_RUNNER=""
DECOMP="${DECOMP:-1d}"

if command -v mpirun >/dev/null 2>&1; then
    MPI_RUNNER="mpirun"
elif command -v mpiexec >/dev/null 2>&1; then
    MPI_RUNNER="mpiexec"
else
    echo "mpirun/mpiexec not found" >&2
    exit 1
fi

"$MPI_RUNNER" -n 2 ./bin/life_mpi --width 8 --height 8 --steps 4 --pattern glider --validate --decomposition "$DECOMP"