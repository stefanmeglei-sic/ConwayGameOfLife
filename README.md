# Conway Game of Life with MPI

This repository implements a Linux-friendly Conway's Game of Life simulator in C, with:

- a serial reference implementation
- an MPI distributed implementation with 1D and 2D decomposition
- non-blocking halo exchange with `MPI_Isend` and `MPI_Irecv`
- periodic (toroidal) boundaries
- deterministic pattern generation for validation

## Project status

Current implementation includes:

- serial simulator for correctness reference
- MPI simulator with selectable decomposition: `--decomposition 1d|2d`
- overlap-friendly non-blocking halo exchange
- serial-vs-MPI validation on rank 0 (`--validate`)
- benchmark scripts and report-ready summary/plot generation

Latest local checks:

- `sh ./scripts/compare_small.sh` -> `validation: OK`
- `mpirun -n 4 --oversubscribe ./bin/life_mpi --width 64 --height 64 --steps 50 --pattern random --seed 7 --density 0.30 --validate --decomposition 2d` -> `validation: OK`

## Requirements

- Linux
- C compiler (`gcc` or `cc`) for the serial target
- MPI C compiler/runtime (`mpicc`, `mpirun` or `mpiexec`) for the distributed target

Ubuntu/Debian example:

```sh
sudo apt update
sudo apt install openmpi-bin libopenmpi-dev
```

Optional tools (not required for the core C/MPI workflow):

- `python3` for helper scripts (`summarize_benchmarks.py`, `generate_svg_plots.py`)
- `soffice` (LibreOffice) for Markdown -> `.docx` conversion

## Build

```sh
make serial
make mpi
make ui       # requires SDL2 (see below)
```

Quick smoke test after build:

```sh
./bin/life_serial --width 32 --height 16 --steps 20 --pattern glider --dump-final
mpirun -n 4 ./bin/life_mpi --width 128 --height 128 --steps 100 --pattern random --seed 7 --density 0.30 --decomposition 2d --csv
```

If your machine has fewer slots than requested ranks, use one of:

- `mpirun -n 8 --oversubscribe ...`
- `OMPI_MCA_rmaps_base_oversubscribe=1 mpirun -n 8 ...`

If MPI is installed in a non-default location:

```sh
make mpi MPICC=/path/to/mpicc
```

### SDL2 Graphical UI

The optional SDL2 graphical UI is built with `make ui` if SDL2 development libraries are detected.

**Installation:**

Ubuntu/Debian:

```sh
sudo apt install libsdl2-dev pkg-config
```

Fedora/RHEL:

```sh
sudo dnf install SDL2-devel pkg-config
```

macOS:

```sh
brew install sdl2 pkg-config
```

Once installed, the `life_ui` binary will be a full graphical application. If SDL2 is not installed, the UI binary will be a stub that prints an error.

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
- `life_engine_run`
- `life_engine_current_grid`
- `life_engine_copy_current`
- `life_engine_destroy`

Shared run adapter:

- `life_run_with_options`

The CLI serial runner uses this engine directly, so future optional UI modes can reuse the exact same backend stepping logic instead of duplicating simulation rules.

Generation callback hook:

- `life_engine_run` accepts a callback of type `life_generation_callback_t`.
- The callback is invoked after each completed generation.
- Returning `0` from the callback stops the run, which is useful for UI controls such as pause/stop or stepping.

This keeps UI concerns out of the simulation rules while preserving the same backend behavior for headless and SSH-friendly runs.

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

### SDL2 Graphical UI Controls

The graphical UI is **interactive and menu-driven**. It starts with a configuration menu where you can set the game parameters, then displays the simulation in a window.

```sh
./bin/life_ui
```

You can also pass default parameters on the command line:

```sh
./bin/life_ui --width 256 --height 256 --steps 1000 --pattern glider --seed 42
```

**Menu Controls:**

- **Arrow Keys** (Left/Right) – Adjust selected parameter
- **Arrow Keys** (Up/Down) – Navigate menu options
- **R** – Generate random seed
- **Space** – Start simulation
- **Q** – Quit

**Simulation Controls (while running or paused):**

- **Space** – Pause/resume simulation
- **Enter** or **P** – Resume running after a pause
- **N** – Single step one generation (while paused)
- **Arrow Keys** – Pan camera around the grid (while paused)
- **I** – Zoom in
- **O** – Zoom out
- **M** – Return to menu
- **Q** or **Esc** – Quit

**Notes:**

- The UI requires a display server (X11, Wayland, Windows, macOS).
- For headless SSH servers, use the serial or MPI CLI targets.
- The UI uses the same backend engine as the serial simulator, so behavior is identical.
- Logging still works: `LIFE_LOG_LEVEL=info LIFE_LOG_FILE=coverage/ui_log ./bin/life_ui ...`

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

## Validation and test scripts

Fast correctness check (default 1D):

```sh
sh ./scripts/compare_small.sh
```

Fast correctness check (2D):

```sh
DECOMP=2d sh ./scripts/compare_small.sh
```

Explicit 2D validation run:

```sh
mpirun -n 4 --oversubscribe ./bin/life_mpi --width 64 --height 64 --steps 50 --pattern random --seed 7 --density 0.30 --validate --decomposition 2d
```

Large-grid smoke test script (serial + MPI 2D 10k x 10k):

```sh
sh ./scripts/run_large_grid_check.sh
```

Output log:

- `coverage/large_grid_10k.log`

## Benchmark, summary, and graph pipeline

The project supports two usage modes:

- **Mode A (recommended): C/MPI-first, no Python environment management**
- **Mode B (optional): local Python virtual environment for helper scripts**

### Mode A: C/MPI-first (no venv)

1. Generate raw benchmark CSVs for 1D and 2D under separate folders:

```sh
OUT_DIR=coverage/bench_1d sh ./scripts/benchmark.sh 1024 1024 200 1 2 4 8
OUT_DIR=coverage/bench_2d DECOMP=2d sh ./scripts/benchmark.sh 1024 1024 200 1 2 4 8
```

1. Build report-ready summary CSV/Markdown:

```sh
python3 ./scripts/summarize_benchmarks.py
```

Generated files:

- `coverage/report_ready/strong_scaling_summary.csv`
- `coverage/report_ready/weak_scaling_summary.csv`
- `coverage/report_ready/benchmark_summary.md`

1. Generate SVG plots (no matplotlib required):

```sh
python3 ./scripts/generate_svg_plots.py
```

### Mode B: Optional local Python virtual environment

Use this only if you prefer isolating helper-script execution from your system Python.

Create and activate a local venv:

```sh
python3 -m venv .venv
. .venv/bin/activate
```

Run helper scripts inside the venv:

```sh
python ./scripts/summarize_benchmarks.py
python ./scripts/generate_svg_plots.py
```

Deactivate when done:

```sh
deactivate
```

Note: the current helper scripts use only Python standard library modules, so no `pip install` step is required.

Generated plot files:

- `coverage/report_ready/plots/strong_speedup.svg`
- `coverage/report_ready/plots/strong_efficiency.svg`
- `coverage/report_ready/plots/strong_comm_vs_compute.svg`
- `coverage/report_ready/plots/weak_relative_time.svg`
- `coverage/report_ready/plots/weak_communication_percent.svg`

Figure-numbered aliases used by the final report:

- `coverage/report_ready/plots/figura1.svg`
- `coverage/report_ready/plots/figura2.svg`
- `coverage/report_ready/plots/figura3.svg`
- `coverage/report_ready/plots/figura4.svg`
- `coverage/report_ready/plots/figura5.svg`

## Report conversion to Word

Convert a Markdown report to `.docx` (LibreOffice):

```sh
soffice --headless --convert-to docx --outdir . ./report_final.md
```

Typical output file:

- `./report_final.docx`

## Notes on decomposition

- `1d`: row striping with top/bottom halo exchange.
- `2d`: Cartesian-style block decomposition with row/column/corner halo exchange.
- Both modes use non-blocking communication and periodic boundaries.

## Next iterations

1. Add finer timing split for 2D mode (row/column/corner exchange).
2. Add input-file support for reproducible initial states.
3. Add simple regression test automation for both 1D and 2D modes.
4. Add optional UI layer that reuses the same simulation backend.

The current working plan is tracked in `PLAN.md`.
