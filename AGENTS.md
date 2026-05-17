# Repository Agent Notes

Use this repository as a staged MPI project, not a one-shot code dump.

## Current architecture

- `src/life.c`: shared grid logic, CLI parsing, initialization, serial stepping
- `src/serial_main.c`: serial executable entry point
- `src/mpi_main.c`: MPI executable entry point with row-wise halo exchange
- `include/life.h`: shared interfaces

## Build and validation

- Serial build: `make serial`
- MPI build: `make mpi`
- Serial smoke test: `./bin/life_serial --width 8 --height 8 --steps 4 --pattern glider --dump-final`
- MPI correctness check: `mpirun -n 2 ./bin/life_mpi --width 8 --height 8 --steps 4 --pattern glider --validate`

## Iteration priorities

1. Preserve serial-vs-MPI determinism.
2. Keep Linux and MPI portability straightforward.
3. Prefer small local changes followed by a concrete build or run check.
4. Treat 2D decomposition as a second-stage enhancement, not part of the baseline.