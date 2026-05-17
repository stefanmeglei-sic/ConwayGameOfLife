#include "life.h"

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t local_index(int row, int column, int width) {
    return (size_t) row * (size_t) width + (size_t) column;
}

static int wrap_column(int column, int width) {
    if (column < 0) {
        return column + width;
    }
    if (column >= width) {
        return column - width;
    }
    return column;
}

static void update_row_range(const uint8_t *current, uint8_t *next, int width, int start_row, int end_row) {
    int row;

    for (row = start_row; row <= end_row; ++row) {
        int column;
        for (column = 0; column < width; ++column) {
            int left = wrap_column(column - 1, width);
            int right = wrap_column(column + 1, width);
            int neighbors = 0;
            uint8_t current_value;

            neighbors += current[local_index(row - 1, left, width)] != 0;
            neighbors += current[local_index(row - 1, column, width)] != 0;
            neighbors += current[local_index(row - 1, right, width)] != 0;
            neighbors += current[local_index(row, left, width)] != 0;
            neighbors += current[local_index(row, right, width)] != 0;
            neighbors += current[local_index(row + 1, left, width)] != 0;
            neighbors += current[local_index(row + 1, column, width)] != 0;
            neighbors += current[local_index(row + 1, right, width)] != 0;

            current_value = current[local_index(row, column, width)];
            next[local_index(row, column, width)] =
                (neighbors == 3 || (current_value && neighbors == 2)) ? 1u : 0u;
        }
    }
}

static void build_scatter_layout(int height, int width, int size, int *counts, int *displacements) {
    int rank;
    int displacement = 0;
    for (rank = 0; rank < size; ++rank) {
        int rows = life_local_row_count(height, size, rank);
        counts[rank] = rows * width;
        displacements[rank] = displacement;
        displacement += counts[rank];
    }
}

int main(int argc, char **argv) {
    int rank;
    int size;
    life_options_t options;
    const char *error_message = NULL;
    uint8_t *global_initial = NULL;
    uint8_t *global_final = NULL;
    uint8_t *serial_final = NULL;
    uint8_t *current = NULL;
    uint8_t *next = NULL;
    int local_rows;
    int width;
    int total_local_rows;
    int step;
    int top_neighbor;
    int bottom_neighbor;
    int *counts = NULL;
    int *displacements = NULL;
    double total_start;
    double total_end;
    double communication_seconds = 0.0;
    double computation_seconds = 0.0;
    double local_total_seconds;
    double max_total_seconds = 0.0;
    double max_communication_seconds = 0.0;
    double max_computation_seconds = 0.0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (!life_parse_options(argc, argv, &options, &error_message)) {
        if (rank == 0) {
            if (error_message != NULL) {
                fprintf(stderr, "Argument error: %s\n", error_message);
            }
            life_print_usage(argv[0]);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    if (options.height < size) {
        if (rank == 0) {
            fprintf(stderr, "This baseline requires height >= number of MPI ranks (height=%d, ranks=%d)\n",
                    options.height,
                    size);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    width = options.width;
    local_rows = life_local_row_count(options.height, size, rank);
    total_local_rows = local_rows + 2;
    top_neighbor = (rank - 1 + size) % size;
    bottom_neighbor = (rank + 1) % size;

    current = (uint8_t *) calloc((size_t) total_local_rows * (size_t) width, sizeof(uint8_t));
    next = (uint8_t *) calloc((size_t) total_local_rows * (size_t) width, sizeof(uint8_t));
    counts = (int *) malloc((size_t) size * sizeof(int));
    displacements = (int *) malloc((size_t) size * sizeof(int));
    if (current == NULL || next == NULL || counts == NULL || displacements == NULL) {
        if (rank == 0) {
            fprintf(stderr, "Failed to allocate MPI buffers\n");
        }
        free(current);
        free(next);
        free(counts);
        free(displacements);
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    build_scatter_layout(options.height, width, size, counts, displacements);

    if (rank == 0) {
        global_initial = life_allocate_grid(options.width, options.height);
        if (global_initial == NULL) {
            fprintf(stderr, "Failed to allocate global initial grid\n");
            free(current);
            free(next);
            free(counts);
            free(displacements);
            MPI_Finalize();
            return EXIT_FAILURE;
        }
        life_initialize_grid(global_initial, options.width, options.height, &options);

        if (options.dump_final || options.validate) {
            global_final = life_allocate_grid(options.width, options.height);
        }
        if (options.validate) {
            serial_final = life_allocate_grid(options.width, options.height);
        }

        if ((options.dump_final || options.validate) && global_final == NULL) {
            fprintf(stderr, "Failed to allocate gathered global grid\n");
            free(global_initial);
            free(global_final);
            free(serial_final);
            free(current);
            free(next);
            free(counts);
            free(displacements);
            MPI_Finalize();
            return EXIT_FAILURE;
        }

        if (options.validate && serial_final == NULL) {
            fprintf(stderr, "Failed to allocate serial validation grid\n");
            free(global_initial);
            free(global_final);
            free(serial_final);
            free(current);
            free(next);
            free(counts);
            free(displacements);
            MPI_Finalize();
            return EXIT_FAILURE;
        }
    }

    MPI_Scatterv(global_initial,
                 counts,
                 displacements,
                 MPI_UNSIGNED_CHAR,
                 &current[local_index(1, 0, width)],
                 local_rows * width,
                 MPI_UNSIGNED_CHAR,
                 0,
                 MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    total_start = MPI_Wtime();

    for (step = 0; step < options.steps; ++step) {
        MPI_Request requests[4];
        double comm_start;
        double comm_end;
        double comp_start;
        double comp_end;
        uint8_t *swap_grid;

        comm_start = MPI_Wtime();
        MPI_Irecv(&current[local_index(0, 0, width)], width, MPI_UNSIGNED_CHAR, top_neighbor, 100, MPI_COMM_WORLD, &requests[0]);
        MPI_Irecv(&current[local_index(local_rows + 1, 0, width)], width, MPI_UNSIGNED_CHAR, bottom_neighbor, 101, MPI_COMM_WORLD, &requests[1]);
        MPI_Isend(&current[local_index(1, 0, width)], width, MPI_UNSIGNED_CHAR, top_neighbor, 101, MPI_COMM_WORLD, &requests[2]);
        MPI_Isend(&current[local_index(local_rows, 0, width)], width, MPI_UNSIGNED_CHAR, bottom_neighbor, 100, MPI_COMM_WORLD, &requests[3]);
        comm_end = MPI_Wtime();
        communication_seconds += comm_end - comm_start;

        if (local_rows > 2) {
            comp_start = MPI_Wtime();
            update_row_range(current, next, width, 2, local_rows - 1);
            comp_end = MPI_Wtime();
            computation_seconds += comp_end - comp_start;
        }

        comm_start = MPI_Wtime();
        MPI_Waitall(4, requests, MPI_STATUSES_IGNORE);
        comm_end = MPI_Wtime();
        communication_seconds += comm_end - comm_start;

        if (local_rows >= 1) {
            comp_start = MPI_Wtime();
            update_row_range(current, next, width, 1, 1);
            if (local_rows > 1) {
                update_row_range(current, next, width, local_rows, local_rows);
            }
            comp_end = MPI_Wtime();
            computation_seconds += comp_end - comp_start;
        }

        swap_grid = current;
        current = next;
        next = swap_grid;
    }

    total_end = MPI_Wtime();
    local_total_seconds = total_end - total_start;

    MPI_Reduce(&local_total_seconds, &max_total_seconds, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&communication_seconds, &max_communication_seconds, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&computation_seconds, &max_computation_seconds, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (options.dump_final || options.validate) {
        MPI_Gatherv(&current[local_index(1, 0, width)],
                    local_rows * width,
                    MPI_UNSIGNED_CHAR,
                    global_final,
                    counts,
                    displacements,
                    MPI_UNSIGNED_CHAR,
                    0,
                    MPI_COMM_WORLD);
    }

    if (rank == 0) {
        printf("mpi: ranks=%d width=%d height=%d steps=%d total_seconds=%.6f communication_seconds=%.6f computation_seconds=%.6f\n",
               size,
               options.width,
               options.height,
               options.steps,
               max_total_seconds,
               max_communication_seconds,
               max_computation_seconds);

        if (options.validate) {
            life_run_serial(global_initial, serial_final, options.width, options.height, options.steps, NULL);
            if (life_compare_grids(global_final, serial_final, options.width, options.height)) {
                printf("validation: OK\n");
            } else {
                printf("validation: FAILED\n");
            }
        }

        if (options.dump_final && global_final != NULL) {
            life_dump_grid_ascii(global_final, options.width, options.height);
        }
    }

    free(global_initial);
    free(global_final);
    free(serial_final);
    free(current);
    free(next);
    free(counts);
    free(displacements);

    MPI_Finalize();
    return EXIT_SUCCESS;
}