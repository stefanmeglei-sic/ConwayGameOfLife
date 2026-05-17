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

## Command line options

- `--width N`
- `--height N`
- `--steps N`
- `--seed N`
- `--density X` where `0.0 <= X <= 1.0`
- `--pattern random|glider|blinker|block|acorn`
- `--dump-final`
- `--validate` (MPI target only)

## Notes on decomposition

The MPI baseline uses 1D row decomposition because it is the smallest correct distributed design that satisfies the project brief:

- each process owns a contiguous band of rows
- left/right neighbors are local because rows remain full-width
- top/bottom ghost rows are exchanged with adjacent MPI ranks
- periodic wrapping across ranks implements the toroidal vertical boundary

This keeps the first version simple while still using the required MPI communication pattern.

## Next iterations

1. Add PGM/PPM snapshots for large-run visualization.
2. Add benchmark output files for strong and weak scaling.
3. Add 2D Cartesian decomposition with `MPI_Cart_create`.
4. Separate computation and communication timing in more detail.

The current working plan is tracked in `PLAN.md`.