# Conway Game of Life with MPI

This repository implements the project from `docs/tema1.txt`: a Linux-friendly Conway's Game of Life simulator in C, with:

- a serial reference implementation
- an MPI distributed implementation using row decomposition
- non-blocking halo exchange with `MPI_Isend` and `MPI_Irecv`
- periodic (toroidal) boundaries
- deterministic pattern generation for validation

## Project status

The current baseline is intentionally narrow and testable:

- serial simulator for correctness reference
- MPI simulator with 1D row striping
- overlap-friendly exchange pattern: compute interior rows while halo rows are in flight
- optional serial-vs-MPI validation on rank 0

This is a workable version to iterate on. A 2D Cartesian decomposition and more advanced profiling can be added next.

Latest local validation snapshot:

- `sh ./scripts/compare_small.sh` -> `validation: OK`
- `mpirun -n 4 ./bin/life_mpi --width 64 --height 64 --steps 50 --pattern random --seed 7 --density 0.30 --validate` -> `validation: OK`

## Requirements

- Linux
- C compiler (`gcc` or `cc`) for the serial target
- MPI C compiler/runtime (`mpicc`, `mpirun` or `mpiexec`) for the distributed target

Ubuntu/Debian example:

```sh
sudo apt update
sudo apt install openmpi-bin libopenmpi-dev
```

## Build

```sh
make serial
make mpi
```

If MPI is installed in a non-default location:

```sh
make mpi MPICC=/path/to/mpicc
```

## Logging

The executables now include centralized logging for startup, argument errors, allocation failures, and output failures.

Environment variables:

- `LIFE_LOG_LEVEL=error|warn|info|debug` (default: `info`)
- `LIFE_LOG_FILE=/path/prefix` to also write logs to files

When `LIFE_LOG_FILE` is set, logs are written to per-component, per-rank files:

- `/path/prefix.serial.rank0.log`
- `/path/prefix.mpi.rankN.log`

Example:

```sh
LIFE_LOG_LEVEL=debug LIFE_LOG_FILE=coverage/life_log mpirun -n 4 ./bin/life_mpi --width 128 --height 128 --steps 50 --pattern random --validate
```

## Backend Single Source of Truth

The serial simulation backend is now exposed through an engine API in [include/life.h](include/life.h):

- `life_engine_init`
- `life_engine_step`
- `life_engine_current_grid`
- `life_engine_copy_current`
- `life_engine_destroy`

The CLI serial runner uses this engine directly, so future optional UI modes can reuse the exact same backend stepping logic instead of duplicating simulation rules.

## Run

Serial:

```sh
./bin/life_serial --width 32 --height 16 --steps 20 --pattern glider --dump-final
```

MPI:

```sh
mpirun -n 4 ./bin/life_mpi --width 128 --height 128 --steps 200 --pattern random --density 0.35
```

MPI with correctness check against the serial reference:

```sh
mpirun -n 4 ./bin/life_mpi --width 32 --height 32 --steps 50 --pattern random --seed 7 --validate
```

MPI with explicit decomposition mode:

```sh
mpirun -n 4 ./bin/life_mpi --width 128 --height 128 --steps 200 --pattern random --seed 7 --density 0.30 --decomposition 2d
```

MPI single-line CSV output (for automation):

```sh
mpirun -n 4 ./bin/life_mpi --width 128 --height 128 --steps 200 --pattern random --seed 7 --density 0.30 --csv
```

MPI snapshot output to PGM:

```sh
mpirun -n 4 ./bin/life_mpi --width 128 --height 128 --steps 200 --pattern random --seed 7 --density 0.30 --pgm-final coverage/final.pgm
```

MPI periodic snapshots every 20 generations:

```sh
mpirun -n 4 ./bin/life_mpi --width 128 --height 128 --steps 200 --pattern random --seed 7 --density 0.30 --snapshot-every 20 --snapshot-prefix coverage/life
```

## Command line options

- `--width N`
- `--height N`
- `--steps N`
- `--seed N`
- `--density X` where `0.0 <= X <= 1.0`
- `--pattern random|glider|blinker|block|acorn`
- `--dump-final`
- `--validate` (MPI target only)
- `--csv` (MPI target only)
- `--csv-header` (MPI target only)
- `--decomposition 1d|2d` (MPI target only)
- `--pgm-final FILE`
- `--snapshot-every N`
- `--snapshot-prefix NAME`

## Benchmark CSV export

Generate strong and weak scaling CSV files under `coverage/`:

```sh
sh ./scripts/benchmark.sh 1024 1024 200 1 2 4 8
```

Use 2D Cartesian mode for benchmark generation:

```sh
DECOMP=2d sh ./scripts/benchmark.sh 1024 1024 200 1 2 4 8
```

Output files:

- `coverage/strong_scaling.csv`
- `coverage/weak_scaling.csv`

## Notes on decomposition

The MPI baseline uses 1D row decomposition because it is the smallest correct distributed design that satisfies the project brief:

- each process owns a contiguous band of rows
- left/right neighbors are local because rows remain full-width
- top/bottom ghost rows are exchanged with adjacent MPI ranks
- periodic wrapping across ranks implements the toroidal vertical boundary

This keeps the first version simple while still using the required MPI communication pattern.

## Next iterations

1. Add finer timing split for 2D mode (row/column/corner exchange).
2. Add input-file support for reproducible initial states.
3. Add simple regression test automation for both 1D and 2D modes.
4. Add optional UI layer that reuses the same simulation backend.

The current working plan is tracked in `PLAN.md`.