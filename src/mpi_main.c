#include "life.h"
#include "life_log.h"

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

static int local_extent_2d(int total, int parts, int coord) {
    int base = total / parts;
    int remainder = total % parts;
    return base + (coord < remainder ? 1 : 0);
}

static int local_offset_2d(int total, int parts, int coord) {
    int base = total / parts;
    int remainder = total % parts;
    return coord * base + (coord < remainder ? coord : remainder);
}

static void unpack_local_block(uint8_t *local, int local_h, int local_w, const uint8_t *packed) {
    int row;
    int stride = local_w + 2;
    for (row = 0; row < local_h; ++row) {
        memcpy(&local[local_index(row + 1, 1, stride)],
               &packed[(size_t) row * (size_t) local_w],
               (size_t) local_w);
    }
}

static void pack_local_block(const uint8_t *local, int local_h, int local_w, uint8_t *packed) {
    int row;
    int stride = local_w + 2;
    for (row = 0; row < local_h; ++row) {
        memcpy(&packed[(size_t) row * (size_t) local_w],
               &local[local_index(row + 1, 1, stride)],
               (size_t) local_w);
    }
}

static void fill_packed_from_global(const uint8_t *global,
                                    int global_w,
                                    int row_offset,
                                    int col_offset,
                                    int local_h,
                                    int local_w,
                                    uint8_t *packed) {
    int row;
    for (row = 0; row < local_h; ++row) {
        memcpy(&packed[(size_t) row * (size_t) local_w],
               &global[local_index(row_offset + row, col_offset, global_w)],
               (size_t) local_w);
    }
}

static void place_packed_into_global(uint8_t *global,
                                     int global_w,
                                     int row_offset,
                                     int col_offset,
                                     int local_h,
                                     int local_w,
                                     const uint8_t *packed) {
    int row;
    for (row = 0; row < local_h; ++row) {
        memcpy(&global[local_index(row_offset + row, col_offset, global_w)],
               &packed[(size_t) row * (size_t) local_w],
               (size_t) local_w);
    }
}

static void update_rect_2d(const uint8_t *current,
                           uint8_t *next,
                           int local_w,
                           int row_start,
                           int row_end,
                           int col_start,
                           int col_end) {
    int row;
    int stride = local_w + 2;

    for (row = row_start; row <= row_end; ++row) {
        int col;
        for (col = col_start; col <= col_end; ++col) {
            int neighbors = 0;
            uint8_t value;
            neighbors += current[local_index(row - 1, col - 1, stride)] != 0;
            neighbors += current[local_index(row - 1, col, stride)] != 0;
            neighbors += current[local_index(row - 1, col + 1, stride)] != 0;
            neighbors += current[local_index(row, col - 1, stride)] != 0;
            neighbors += current[local_index(row, col + 1, stride)] != 0;
            neighbors += current[local_index(row + 1, col - 1, stride)] != 0;
            neighbors += current[local_index(row + 1, col, stride)] != 0;
            neighbors += current[local_index(row + 1, col + 1, stride)] != 0;
            value = current[local_index(row, col, stride)];
            next[local_index(row, col, stride)] = (neighbors == 3 || (value && neighbors == 2)) ? 1u : 0u;
        }
    }
}

static void update_border_2d(const uint8_t *current, uint8_t *next, int local_h, int local_w) {
    int row;
    int stride = local_w + 2;

    for (row = 1; row <= local_h; ++row) {
        int col;
        for (col = 1; col <= local_w; ++col) {
            int on_border = (row == 1 || row == local_h || col == 1 || col == local_w);
            if (on_border) {
                int neighbors = 0;
                uint8_t value;
                neighbors += current[local_index(row - 1, col - 1, stride)] != 0;
                neighbors += current[local_index(row - 1, col, stride)] != 0;
                neighbors += current[local_index(row - 1, col + 1, stride)] != 0;
                neighbors += current[local_index(row, col - 1, stride)] != 0;
                neighbors += current[local_index(row, col + 1, stride)] != 0;
                neighbors += current[local_index(row + 1, col - 1, stride)] != 0;
                neighbors += current[local_index(row + 1, col, stride)] != 0;
                neighbors += current[local_index(row + 1, col + 1, stride)] != 0;
                value = current[local_index(row, col, stride)];
                next[local_index(row, col, stride)] = (neighbors == 3 || (value && neighbors == 2)) ? 1u : 0u;
            }
        }
    }
}

static int write_step_snapshot(const char *prefix, int step, const uint8_t *grid, int width, int height) {
    char path[LIFE_PATH_MAX];
    int written = snprintf(path, sizeof(path), "%s_step_%06d.pgm", prefix, step);
    if (written <= 0 || written >= (int) sizeof(path)) {
        return 0;
    }
    return life_write_pgm(path, grid, width, height);
}

static void print_run_line(const life_options_t *options,
                           int size,
                           double total_seconds,
                           double communication_seconds,
                           double computation_seconds) {
    if (options->csv_header) {
        printf("ranks,width,height,steps,total_seconds,communication_seconds,computation_seconds\n");
    }

    if (options->csv) {
        printf("%d,%d,%d,%d,%.9f,%.9f,%.9f\n",
               size,
               options->width,
               options->height,
               options->steps,
               total_seconds,
               communication_seconds,
               computation_seconds);
    } else {
        printf("mpi: ranks=%d width=%d height=%d steps=%d total_seconds=%.6f communication_seconds=%.6f computation_seconds=%.6f\n",
               size,
               options->width,
               options->height,
               options->steps,
               total_seconds,
               communication_seconds,
               computation_seconds);
    }
}

static int gather_global_2d(MPI_Comm cart_comm,
                            int rank,
                            int size,
                            const uint8_t *local,
                            int local_h,
                            int local_w,
                            int global_h,
                            int global_w,
                            int dims[2],
                            uint8_t *global_out) {
    int packed_size = local_h * local_w;
    uint8_t *packed = (uint8_t *) malloc((size_t) packed_size);
    if (packed == NULL) {
        return 0;
    }

    pack_local_block(local, local_h, local_w, packed);

    if (rank == 0) {
        int src;
        int coords[2];
        place_packed_into_global(global_out,
                                 global_w,
                                 local_offset_2d(global_h, dims[0], 0),
                                 local_offset_2d(global_w, dims[1], 0),
                                 local_extent_2d(global_h, dims[0], 0),
                                 local_extent_2d(global_w, dims[1], 0),
                                 packed);

        for (src = 1; src < size; ++src) {
            int src_h;
            int src_w;
            int src_size;
            uint8_t *src_packed;
            MPI_Cart_coords(cart_comm, src, 2, coords);
            src_h = local_extent_2d(global_h, dims[0], coords[0]);
            src_w = local_extent_2d(global_w, dims[1], coords[1]);
            src_size = src_h * src_w;
            src_packed = (uint8_t *) malloc((size_t) src_size);
            if (src_packed == NULL) {
                free(packed);
                return 0;
            }
            MPI_Recv(src_packed, src_size, MPI_UNSIGNED_CHAR, src, 201, cart_comm, MPI_STATUS_IGNORE);
            place_packed_into_global(global_out,
                                     global_w,
                                     local_offset_2d(global_h, dims[0], coords[0]),
                                     local_offset_2d(global_w, dims[1], coords[1]),
                                     src_h,
                                     src_w,
                                     src_packed);
            free(src_packed);
        }
    } else {
        MPI_Send(packed, packed_size, MPI_UNSIGNED_CHAR, 0, 201, cart_comm);
    }

    free(packed);
    return 1;
}

static int run_mpi_2d(const life_options_t *options, int rank, int size) {
    int dims[2] = {0, 0};
    int periods[2] = {1, 1};
    int coords[2] = {0, 0};
    MPI_Comm cart_comm = MPI_COMM_NULL;
    int local_h;
    int local_w;
    int stride;
    int step;
    int need_gathered_grid;
    uint8_t *current = NULL;
    uint8_t *next = NULL;
    uint8_t *global_initial = NULL;
    uint8_t *global_final = NULL;
    uint8_t *serial_final = NULL;
    double total_start;
    double total_end;
    double communication_seconds = 0.0;
    double computation_seconds = 0.0;
    double local_total_seconds;
    double max_total_seconds = 0.0;
    double max_communication_seconds = 0.0;
    double max_computation_seconds = 0.0;

    MPI_Dims_create(size, 2, dims);
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &cart_comm);
    if (cart_comm == MPI_COMM_NULL) {
        if (rank == 0) {
            LIFE_LOG_ERROR("Failed to create 2D Cartesian communicator");
        }
        return EXIT_FAILURE;
    }

    MPI_Cart_coords(cart_comm, rank, 2, coords);

    if (options->height < dims[0] || options->width < dims[1]) {
        if (rank == 0) {
            LIFE_LOG_ERROR("2D decomposition requires height >= %d and width >= %d (got %d x %d)",
                           dims[0],
                           dims[1],
                           options->height,
                           options->width);
        }
        MPI_Comm_free(&cart_comm);
        return EXIT_FAILURE;
    }

    need_gathered_grid =
        options->dump_final ||
        options->validate ||
        options->pgm_final_path[0] != '\0' ||
        options->snapshot_every > 0;

    local_h = local_extent_2d(options->height, dims[0], coords[0]);
    local_w = local_extent_2d(options->width, dims[1], coords[1]);
    stride = local_w + 2;

    current = (uint8_t *) calloc((size_t) (local_h + 2) * (size_t) (local_w + 2), sizeof(uint8_t));
    next = (uint8_t *) calloc((size_t) (local_h + 2) * (size_t) (local_w + 2), sizeof(uint8_t));
    if (current == NULL || next == NULL) {
        if (rank == 0) {
            LIFE_LOG_ERROR("Failed to allocate 2D local grids");
        }
        free(current);
        free(next);
        MPI_Comm_free(&cart_comm);
        return EXIT_FAILURE;
    }

    if (rank == 0) {
        int target_rank;
        global_initial = life_allocate_grid(options->width, options->height);
        if (global_initial == NULL) {
            LIFE_LOG_ERROR("Failed to allocate global initial grid");
            free(current);
            free(next);
            MPI_Comm_free(&cart_comm);
            return EXIT_FAILURE;
        }
        life_initialize_grid(global_initial, options->width, options->height, options);

        if (need_gathered_grid) {
            global_final = life_allocate_grid(options->width, options->height);
            if (global_final == NULL) {
                LIFE_LOG_ERROR("Failed to allocate global gather grid");
                free(global_initial);
                free(current);
                free(next);
                MPI_Comm_free(&cart_comm);
                return EXIT_FAILURE;
            }
        }

        if (options->validate) {
            serial_final = life_allocate_grid(options->width, options->height);
            if (serial_final == NULL) {
                LIFE_LOG_ERROR("Failed to allocate serial validation grid");
                free(global_initial);
                free(global_final);
                free(current);
                free(next);
                MPI_Comm_free(&cart_comm);
                return EXIT_FAILURE;
            }
        }

        for (target_rank = 0; target_rank < size; ++target_rank) {
            int tcoords[2];
            int th;
            int tw;
            int roff;
            int coff;
            int tsize;
            uint8_t *packed;

            MPI_Cart_coords(cart_comm, target_rank, 2, tcoords);
            th = local_extent_2d(options->height, dims[0], tcoords[0]);
            tw = local_extent_2d(options->width, dims[1], tcoords[1]);
            roff = local_offset_2d(options->height, dims[0], tcoords[0]);
            coff = local_offset_2d(options->width, dims[1], tcoords[1]);
            tsize = th * tw;
            packed = (uint8_t *) malloc((size_t) tsize);
            if (packed == NULL) {
                LIFE_LOG_ERROR("Failed to allocate temporary init block");
                free(global_initial);
                free(global_final);
                free(serial_final);
                free(current);
                free(next);
                MPI_Comm_free(&cart_comm);
                return EXIT_FAILURE;
            }

            fill_packed_from_global(global_initial, options->width, roff, coff, th, tw, packed);

            if (target_rank == 0) {
                unpack_local_block(current, local_h, local_w, packed);
            } else {
                MPI_Send(packed, tsize, MPI_UNSIGNED_CHAR, target_rank, 200, cart_comm);
            }
            free(packed);
        }
    } else {
        int my_size = local_h * local_w;
        uint8_t *packed = (uint8_t *) malloc((size_t) my_size);
        if (packed == NULL) {
            free(current);
            free(next);
            MPI_Comm_free(&cart_comm);
            return EXIT_FAILURE;
        }
        MPI_Recv(packed, my_size, MPI_UNSIGNED_CHAR, 0, 200, cart_comm, MPI_STATUS_IGNORE);
        unpack_local_block(current, local_h, local_w, packed);
        free(packed);
    }

    MPI_Barrier(cart_comm);
    total_start = MPI_Wtime();

    for (step = 0; step < options->steps; ++step) {
        int north;
        int south;
        int west;
        int east;
        int nw;
        int ne;
        int sw;
        int se;
        int north_west_coords[2];
        int north_east_coords[2];
        int south_west_coords[2];
        int south_east_coords[2];
        MPI_Request requests[16];
        int req_count = 0;
        uint8_t *send_left;
        uint8_t *send_right;
        uint8_t *recv_left;
        uint8_t *recv_right;
        uint8_t recv_nw = 0;
        uint8_t recv_ne = 0;
        uint8_t recv_sw = 0;
        uint8_t recv_se = 0;
        uint8_t send_nw;
        uint8_t send_ne;
        uint8_t send_sw;
        uint8_t send_se;
        double comm_start;
        double comm_end;
        double comp_start;
        double comp_end;
        uint8_t *swap_grid;
        int row;

        MPI_Cart_shift(cart_comm, 0, 1, &north, &south);
        MPI_Cart_shift(cart_comm, 1, 1, &west, &east);

        north_west_coords[0] = (coords[0] - 1 + dims[0]) % dims[0];
        north_west_coords[1] = (coords[1] - 1 + dims[1]) % dims[1];
        north_east_coords[0] = (coords[0] - 1 + dims[0]) % dims[0];
        north_east_coords[1] = (coords[1] + 1) % dims[1];
        south_west_coords[0] = (coords[0] + 1) % dims[0];
        south_west_coords[1] = (coords[1] - 1 + dims[1]) % dims[1];
        south_east_coords[0] = (coords[0] + 1) % dims[0];
        south_east_coords[1] = (coords[1] + 1) % dims[1];
        MPI_Cart_rank(cart_comm, north_west_coords, &nw);
        MPI_Cart_rank(cart_comm, north_east_coords, &ne);
        MPI_Cart_rank(cart_comm, south_west_coords, &sw);
        MPI_Cart_rank(cart_comm, south_east_coords, &se);

        send_left = (uint8_t *) malloc((size_t) local_h);
        send_right = (uint8_t *) malloc((size_t) local_h);
        recv_left = (uint8_t *) malloc((size_t) local_h);
        recv_right = (uint8_t *) malloc((size_t) local_h);
        if (send_left == NULL || send_right == NULL || recv_left == NULL || recv_right == NULL) {
            free(send_left);
            free(send_right);
            free(recv_left);
            free(recv_right);
            free(global_initial);
            free(global_final);
            free(serial_final);
            free(current);
            free(next);
            MPI_Comm_free(&cart_comm);
            return EXIT_FAILURE;
        }

        for (row = 0; row < local_h; ++row) {
            send_left[row] = current[local_index(row + 1, 1, stride)];
            send_right[row] = current[local_index(row + 1, local_w, stride)];
        }

        send_nw = current[local_index(1, 1, stride)];
        send_ne = current[local_index(1, local_w, stride)];
        send_sw = current[local_index(local_h, 1, stride)];
        send_se = current[local_index(local_h, local_w, stride)];

        comm_start = MPI_Wtime();
        MPI_Irecv(&current[local_index(0, 1, stride)], local_w, MPI_UNSIGNED_CHAR, north, 100, cart_comm, &requests[req_count++]);
        MPI_Irecv(&current[local_index(local_h + 1, 1, stride)], local_w, MPI_UNSIGNED_CHAR, south, 101, cart_comm, &requests[req_count++]);
        MPI_Irecv(recv_left, local_h, MPI_UNSIGNED_CHAR, west, 102, cart_comm, &requests[req_count++]);
        MPI_Irecv(recv_right, local_h, MPI_UNSIGNED_CHAR, east, 103, cart_comm, &requests[req_count++]);
        MPI_Irecv(&recv_nw, 1, MPI_UNSIGNED_CHAR, nw, 104, cart_comm, &requests[req_count++]);
        MPI_Irecv(&recv_ne, 1, MPI_UNSIGNED_CHAR, ne, 105, cart_comm, &requests[req_count++]);
        MPI_Irecv(&recv_sw, 1, MPI_UNSIGNED_CHAR, sw, 106, cart_comm, &requests[req_count++]);
        MPI_Irecv(&recv_se, 1, MPI_UNSIGNED_CHAR, se, 107, cart_comm, &requests[req_count++]);

        MPI_Isend(&current[local_index(1, 1, stride)], local_w, MPI_UNSIGNED_CHAR, north, 101, cart_comm, &requests[req_count++]);
        MPI_Isend(&current[local_index(local_h, 1, stride)], local_w, MPI_UNSIGNED_CHAR, south, 100, cart_comm, &requests[req_count++]);
        MPI_Isend(send_left, local_h, MPI_UNSIGNED_CHAR, west, 103, cart_comm, &requests[req_count++]);
        MPI_Isend(send_right, local_h, MPI_UNSIGNED_CHAR, east, 102, cart_comm, &requests[req_count++]);
        MPI_Isend(&send_nw, 1, MPI_UNSIGNED_CHAR, nw, 107, cart_comm, &requests[req_count++]);
        MPI_Isend(&send_ne, 1, MPI_UNSIGNED_CHAR, ne, 106, cart_comm, &requests[req_count++]);
        MPI_Isend(&send_sw, 1, MPI_UNSIGNED_CHAR, sw, 105, cart_comm, &requests[req_count++]);
        MPI_Isend(&send_se, 1, MPI_UNSIGNED_CHAR, se, 104, cart_comm, &requests[req_count++]);
        comm_end = MPI_Wtime();
        communication_seconds += comm_end - comm_start;

        if (local_h > 2 && local_w > 2) {
            comp_start = MPI_Wtime();
            update_rect_2d(current, next, local_w, 2, local_h - 1, 2, local_w - 1);
            comp_end = MPI_Wtime();
            computation_seconds += comp_end - comp_start;
        }

        comm_start = MPI_Wtime();
        MPI_Waitall(req_count, requests, MPI_STATUSES_IGNORE);
        comm_end = MPI_Wtime();
        communication_seconds += comm_end - comm_start;

        for (row = 0; row < local_h; ++row) {
            current[local_index(row + 1, 0, stride)] = recv_left[row];
            current[local_index(row + 1, local_w + 1, stride)] = recv_right[row];
        }
        current[local_index(0, 0, stride)] = recv_nw;
        current[local_index(0, local_w + 1, stride)] = recv_ne;
        current[local_index(local_h + 1, 0, stride)] = recv_sw;
        current[local_index(local_h + 1, local_w + 1, stride)] = recv_se;

        comp_start = MPI_Wtime();
        update_border_2d(current, next, local_h, local_w);
        comp_end = MPI_Wtime();
        computation_seconds += comp_end - comp_start;

        free(send_left);
        free(send_right);
        free(recv_left);
        free(recv_right);

        swap_grid = current;
        current = next;
        next = swap_grid;

        if (options->snapshot_every > 0 && ((step + 1) % options->snapshot_every) == 0) {
            if (!gather_global_2d(cart_comm,
                                  rank,
                                  size,
                                  current,
                                  local_h,
                                  local_w,
                                  options->height,
                                  options->width,
                                  dims,
                                  global_final)) {
                if (rank == 0) {
                    LIFE_LOG_ERROR("Failed to gather 2D snapshot");
                }
                free(global_initial);
                free(global_final);
                free(serial_final);
                free(current);
                free(next);
                MPI_Comm_free(&cart_comm);
                return EXIT_FAILURE;
            }

            if (rank == 0 && !write_step_snapshot(options->snapshot_prefix, step + 1, global_final, options->width, options->height)) {
                LIFE_LOG_ERROR("Failed to write step snapshot for step %d", step + 1);
            }
        }
    }

    total_end = MPI_Wtime();
    local_total_seconds = total_end - total_start;

    MPI_Reduce(&local_total_seconds, &max_total_seconds, 1, MPI_DOUBLE, MPI_MAX, 0, cart_comm);
    MPI_Reduce(&communication_seconds, &max_communication_seconds, 1, MPI_DOUBLE, MPI_MAX, 0, cart_comm);
    MPI_Reduce(&computation_seconds, &max_computation_seconds, 1, MPI_DOUBLE, MPI_MAX, 0, cart_comm);

    if (need_gathered_grid) {
        if (!gather_global_2d(cart_comm,
                              rank,
                              size,
                              current,
                              local_h,
                              local_w,
                              options->height,
                              options->width,
                              dims,
                              global_final)) {
            if (rank == 0) {
                LIFE_LOG_ERROR("Failed final 2D gather");
            }
            free(global_initial);
            free(global_final);
            free(serial_final);
            free(current);
            free(next);
            MPI_Comm_free(&cart_comm);
            return EXIT_FAILURE;
        }
    }

    if (rank == 0) {
        print_run_line(options, size, max_total_seconds, max_communication_seconds, max_computation_seconds);

        if (options->validate) {
            life_run_serial(global_initial, serial_final, options->width, options->height, options->steps, NULL);
            if (life_compare_grids(global_final, serial_final, options->width, options->height)) {
                printf("validation: OK\n");
            } else {
                printf("validation: FAILED\n");
            }
        }

        if (options->pgm_final_path[0] != '\0') {
            if (!life_write_pgm(options->pgm_final_path, global_final, options->width, options->height)) {
                LIFE_LOG_ERROR("Failed to write PGM file: %s", options->pgm_final_path);
            }
        }

        if (options->dump_final && global_final != NULL) {
            life_dump_grid_ascii(global_final, options->width, options->height);
        }
    }

    free(global_initial);
    free(global_final);
    free(serial_final);
    free(current);
    free(next);
    MPI_Comm_free(&cart_comm);
    return EXIT_SUCCESS;
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
    int need_gathered_grid;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    life_log_init("mpi", rank);
    LIFE_LOG_INFO("MPI runner started with %d ranks", size);

    if (!life_parse_options(argc, argv, &options, &error_message)) {
        if (rank == 0) {
            if (error_message != NULL) {
                LIFE_LOG_ERROR("Argument error: %s", error_message);
            }
            life_print_usage(argv[0]);
        }
        life_log_shutdown();
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    if (options.decomposition_2d) {
        int status = run_mpi_2d(&options, rank, size);
        life_log_shutdown();
        MPI_Finalize();
        return status;
    }

    if (options.height < size) {
        if (rank == 0) {
            LIFE_LOG_ERROR("This baseline requires height >= number of MPI ranks (height=%d, ranks=%d)",
                           options.height,
                           size);
        }
        life_log_shutdown();
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    need_gathered_grid =
        options.dump_final ||
        options.validate ||
        options.pgm_final_path[0] != '\0' ||
        options.snapshot_every > 0;

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
            LIFE_LOG_ERROR("Failed to allocate MPI buffers");
        }
        free(current);
        free(next);
        free(counts);
        free(displacements);
        life_log_shutdown();
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    build_scatter_layout(options.height, width, size, counts, displacements);

    if (rank == 0) {
        global_initial = life_allocate_grid(options.width, options.height);
        if (global_initial == NULL) {
            LIFE_LOG_ERROR("Failed to allocate global initial grid");
            free(current);
            free(next);
            free(counts);
            free(displacements);
            life_log_shutdown();
            MPI_Finalize();
            return EXIT_FAILURE;
        }
        life_initialize_grid(global_initial, options.width, options.height, &options);

        if (need_gathered_grid) {
            global_final = life_allocate_grid(options.width, options.height);
        }
        if (options.validate) {
            serial_final = life_allocate_grid(options.width, options.height);
        }

        if (need_gathered_grid && global_final == NULL) {
            LIFE_LOG_ERROR("Failed to allocate gathered global grid");
            free(global_initial);
            free(global_final);
            free(serial_final);
            free(current);
            free(next);
            free(counts);
            free(displacements);
            life_log_shutdown();
            MPI_Finalize();
            return EXIT_FAILURE;
        }

        if (options.validate && serial_final == NULL) {
            LIFE_LOG_ERROR("Failed to allocate serial validation grid");
            free(global_initial);
            free(global_final);
            free(serial_final);
            free(current);
            free(next);
            free(counts);
            free(displacements);
            life_log_shutdown();
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

        if (options.snapshot_every > 0 && ((step + 1) % options.snapshot_every) == 0) {
            MPI_Gatherv(&current[local_index(1, 0, width)],
                        local_rows * width,
                        MPI_UNSIGNED_CHAR,
                        global_final,
                        counts,
                        displacements,
                        MPI_UNSIGNED_CHAR,
                        0,
                        MPI_COMM_WORLD);

            if (rank == 0 && !write_step_snapshot(options.snapshot_prefix, step + 1, global_final, options.width, options.height)) {
                LIFE_LOG_ERROR("Failed to write step snapshot for step %d", step + 1);
            }
        }
    }

    total_end = MPI_Wtime();
    local_total_seconds = total_end - total_start;

    MPI_Reduce(&local_total_seconds, &max_total_seconds, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&communication_seconds, &max_communication_seconds, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&computation_seconds, &max_computation_seconds, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (need_gathered_grid) {
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
        print_run_line(&options, size, max_total_seconds, max_communication_seconds, max_computation_seconds);

        if (options.validate) {
            life_run_serial(global_initial, serial_final, options.width, options.height, options.steps, NULL);
            if (life_compare_grids(global_final, serial_final, options.width, options.height)) {
                printf("validation: OK\n");
            } else {
                printf("validation: FAILED\n");
            }
        }

        if (options.pgm_final_path[0] != '\0') {
            if (!life_write_pgm(options.pgm_final_path, global_final, options.width, options.height)) {
                LIFE_LOG_ERROR("Failed to write PGM file: %s", options.pgm_final_path);
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

    LIFE_LOG_INFO("MPI runner finished successfully");
    life_log_shutdown();
    MPI_Finalize();
    return EXIT_SUCCESS;
}