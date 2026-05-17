CC ?= gcc
MPICC ?= mpicc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS ?=

BIN_DIR := bin
SRC_DIR := src
INCLUDE_DIR := include

COMMON_SRCS := $(SRC_DIR)/life.c $(SRC_DIR)/life_log.c
SERIAL_SRCS := $(COMMON_SRCS) $(SRC_DIR)/serial_main.c
MPI_SRCS := $(COMMON_SRCS) $(SRC_DIR)/mpi_main.c
UI_SRCS := $(COMMON_SRCS) $(SRC_DIR)/sdl_ui.c $(SRC_DIR)/ui_main.c

SERIAL_TARGET := $(BIN_DIR)/life_serial
MPI_TARGET := $(BIN_DIR)/life_mpi
UI_TARGET := $(BIN_DIR)/life_ui

# Check for SDL2
SDL2_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL2_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)
HAVE_SDL2 := $(if $(SDL2_CFLAGS),1,0)

ifeq ($(HAVE_SDL2),1)
  UI_CFLAGS := -DHAVE_SDL2
  UI_LDFLAGS := $(SDL2_LIBS)
else
  UI_CFLAGS :=
  UI_LDFLAGS :=
endif

.PHONY: all serial mpi ui clean test-serial test-mpi compare

all: serial mpi ui

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

serial: $(SERIAL_TARGET)

$(SERIAL_TARGET): $(SERIAL_SRCS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SERIAL_SRCS) -o $@ $(LDFLAGS)

mpi: $(MPI_TARGET)

$(MPI_TARGET): $(MPI_SRCS) | $(BIN_DIR)
	@command -v $(MPICC) >/dev/null 2>&1 || { echo "MPI compiler '$(MPICC)' not found"; exit 1; }
	$(MPICC) $(CFLAGS) $(MPI_SRCS) -o $@ $(LDFLAGS)

ui: $(UI_TARGET)

$(UI_TARGET): $(UI_SRCS) | $(BIN_DIR)
ifeq ($(HAVE_SDL2),1)
	$(CC) $(CFLAGS) $(UI_CFLAGS) $(UI_SRCS) -o $@ $(LDFLAGS) $(UI_LDFLAGS)
	@echo "SDL2 UI built successfully: $@"
else
	$(CC) $(CFLAGS) $(UI_SRCS) -o $@ $(LDFLAGS)
	@echo "SDL2 not available; UI stub built (will print error when run)"
endif

test-serial: $(SERIAL_TARGET)
	./$(SERIAL_TARGET) --width 8 --height 8 --steps 4 --pattern glider --dump-final >/dev/null

test-mpi: $(MPI_TARGET)
	@command -v mpirun >/dev/null 2>&1 || command -v mpiexec >/dev/null 2>&1 || { echo "mpirun/mpiexec not found"; exit 1; }
	sh ./scripts/compare_small.sh

test-ui: $(UI_TARGET)
	@if [ "$(HAVE_SDL2)" = "1" ]; then \
		echo "Running UI test (headless mode not supported, skipping interactive test)"; \
	else \
		echo "SDL2 not available, UI stub will print error"; \
		./$(UI_TARGET) --width 8 --height 8 --steps 2 --pattern glider 2>&1 | grep -q "SDL2 not available" && echo "UI stub working correctly"; \
	fi

compare: test-mpi

clean:
	rm -rf $(BIN_DIR)