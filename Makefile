CC ?= gcc
MPICC ?= mpicc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS ?=

BIN_DIR := bin
SRC_DIR := src
INCLUDE_DIR := include

COMMON_SRCS := $(SRC_DIR)/life.c
SERIAL_SRCS := $(COMMON_SRCS) $(SRC_DIR)/serial_main.c
MPI_SRCS := $(COMMON_SRCS) $(SRC_DIR)/mpi_main.c

SERIAL_TARGET := $(BIN_DIR)/life_serial
MPI_TARGET := $(BIN_DIR)/life_mpi

.PHONY: all serial mpi clean test-serial test-mpi compare

all: serial mpi

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

serial: $(SERIAL_TARGET)

$(SERIAL_TARGET): $(SERIAL_SRCS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SERIAL_SRCS) -o $@ $(LDFLAGS)

mpi: $(MPI_TARGET)

$(MPI_TARGET): $(MPI_SRCS) | $(BIN_DIR)
	@command -v $(MPICC) >/dev/null 2>&1 || { echo "MPI compiler '$(MPICC)' not found"; exit 1; }
	$(MPICC) $(CFLAGS) $(MPI_SRCS) -o $@ $(LDFLAGS)

test-serial: $(SERIAL_TARGET)
	./$(SERIAL_TARGET) --width 8 --height 8 --steps 4 --pattern glider --dump-final >/dev/null

test-mpi: $(MPI_TARGET)
	@command -v mpirun >/dev/null 2>&1 || command -v mpiexec >/dev/null 2>&1 || { echo "mpirun/mpiexec not found"; exit 1; }
	sh ./scripts/compare_small.sh

compare: test-mpi

clean:
	rm -rf $(BIN_DIR)