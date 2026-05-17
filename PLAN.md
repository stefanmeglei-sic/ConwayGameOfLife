# Implementation Plan

This plan is meant to stay live and evolve with the code.

## Requirements extracted from the brief

- Linux-first execution
- C implementation using the MPI C library
- large 2D toroidal grids
- distributed domain decomposition
- halo exchange between neighboring partitions
- non-blocking communication as the baseline MPI mechanism
- correctness validation against a serial reference
- timing support for later strong and weak scaling analysis

## Current baseline

- serial reference implementation: complete
- MPI 1D row decomposition: complete baseline
- MPI 2D Cartesian decomposition: complete baseline
- non-blocking top/bottom halo exchange: complete baseline
- non-blocking 2D halo exchange (rows, columns, corners): complete baseline
- deterministic initialization patterns: complete baseline
- serial-vs-MPI validation hook: complete baseline
- VS Code tasks and shell scripts for local iteration: complete baseline

## Next iterations

1. Add finer communication timing split: post, wait, gather by direction.
2. Add support for larger input files instead of generated initial states only.
3. Add a small regression suite for stable patterns and oscillators in both 1D and 2D.
4. Add optional UI mode with adjustable speed, while preserving headless SSH workflow.
5. Keep backend simulation logic UI-agnostic so CLI and UI share the same engine.

## Constraints discovered in this environment

- `gcc` and `cc` are available.
- `mpicc` and `mpirun` are installed and usable.
- The MPI target now builds and runs locally on Linux.

## Latest validation snapshot

- `sh ./scripts/compare_small.sh`: `validation: OK` on 2 ranks.
- `mpirun -n 4 ./bin/life_mpi --width 64 --height 64 --steps 50 --pattern random --seed 7 --density 0.30 --validate`: `validation: OK`.
- `mpirun -n 4 ./bin/life_mpi --width 32 --height 32 --steps 20 --pattern random --seed 7 --density 0.30 --validate --decomposition 1d`: `validation: OK`.
- `mpirun -n 4 ./bin/life_mpi --width 32 --height 32 --steps 20 --pattern random --seed 7 --density 0.30 --validate --decomposition 2d`: `validation: OK`.

## Immediate validation plan

1. Keep serial smoke tests green after each shared-logic change.
2. Keep the small 2-rank validation check green as a fast gate.
3. Expand timing runs and collect benchmark artifacts for report graphs.