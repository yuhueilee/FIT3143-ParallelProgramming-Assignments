/*
This file contains function for simulating wireless sensor node

Abbreviations:
    - SMA: Sea moving average
*/
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <omp.h>
#include <mpi.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

#define CYCLE 5 // cycle for sea water column height generation
#define LOWER_BOUND 6400.0 // lower bound for sea water column height
#define UPPER_BOUND 6500.0 // upper bound for sea water column height
#define RANGE 100.0 // tolerence range to compare SMA between nodes
#define SHIFT_ROW 0
#define SHIFT_COL 1
#define DISP 1

/* Function prototype */
float rand_float(unsigned int seed, float min, float max);

/* Wireless sensor node simulation */
void sensor_node(int num_rows, int num_cols, float threshold, MPI_Comm world_comm, MPI_Comm nodes_comm) {
    bool terminate = 0; // set terminate to false initially
    bool notification = 0; // set the notification to send report to base station to false initially
    int i, my_rank, my_cart_rank;
    int num_nbrs = 4;
    int ndims = 2, reorder = 1, ierr = 0;
    int p_dims[ndims], p_coord[ndims], p_wrap_around[ndims], p_nbrs[num_nbrs];
    int index = 0; // pointer to the array storing sea values
    int window_size = 5; // size of the array storing sea values
    float *p_sea_array = calloc(window_size, sizeof(float)); // initialize the array
    float sea_moving_avg = 0.0;
    float p_recv_vals[4] = { -1.0, -1.0, -1.0, -1.0 };
    MPI_Comm cart_comm;

    // assign rows and cols to dims array
    p_dims[0] = num_rows;
    p_dims[1] = num_cols;

    /* Store the rank number from nodes_comm */
    MPI_Comm_rank(nodes_comm, &my_rank);

    /* Set periodic shift to false by initializing wrap_around array to 0 */
    for (i = 0; i < ndims; i++) {
        p_wrap_around[i] = 0;
    }

    /* Create cartesian topology using nodes communicator */    
    ierr = MPI_Cart_create(nodes_comm, ndims, p_dims, p_wrap_around, reorder, &cart_comm);
    if(ierr != 0) {
        printf("ERROR[%d] creating 2D CART\n", ierr);
    }
    
    MPI_Cart_coords(cart_comm, my_rank, ndims, p_coord); // use my rank to find my coordinates in the cartesian communicator group
    MPI_Cart_rank(cart_comm, p_coord, &my_cart_rank); // use my cartesian coordinates to find my cart rank in cartesian group
    /* Get the adjacent neighbor's rank number (top, bottom, left, right) */
    MPI_Cart_shift(cart_comm, SHIFT_ROW, DISP, &p_nbrs[0], &p_nbrs[1]);
	MPI_Cart_shift(cart_comm, SHIFT_COL, DISP, &p_nbrs[2], &p_nbrs[3]);

    printf("Cart rank: %d; Cart Coord: (%d, %d);\n", my_cart_rank, p_coord[0], p_coord[1]);
    
    sleep(my_rank); // this is to have different seed value for random float generator

    /* Set the number of thread to two */
    omp_set_num_threads(2);

    #pragma omp parallel sections 
    {
        /* 
            1. Generate random sea value
            2. Compare SMA with its neighbors and send report to base station
            3. Listen to base station
         */
        #pragma omp section 
        {
            /* Create request and status array */
            MPI_Request p_req[num_nbrs];
            MPI_Status p_status[num_nbrs];

            int counter = 0;
            do {
                /* STEP 1: Generate random sea value */
                unsigned int seed = time(NULL); // seed value to generate different random value for each process
                float rand_sea_height = rand_float(seed, LOWER_BOUND, UPPER_BOUND);
                
                /* Push the new random value to array */
                p_sea_array[index] = rand_sea_height;
                index = (index + 1) % window_size; // update the index (circular)
                
                // check if the array is filled up with values
                if (p_sea_array[index] != 0.0) {
                    float sum = 0.0;
                    for (i = 0; i < window_size; i++) {
                        sum += p_sea_array[i];
                    }
                    /* Calculate the new SMA */
                    #pragma omp critical 
                    sea_moving_avg = sum / window_size; // lock the shared variable

                    // check if the SMA exceeds the threshold
                    if (sea_moving_avg > threshold) {
                        /* Non-blocking send request to all neighbors with tag 1 */
                        for (i = 0; i < num_nbrs; i++) {
                            int request = 1;
                            MPI_Isend(&request, 1, MPI_INT, p_nbrs[i], 1, cart_comm, &p_req[i]);
                        }
                        MPI_Waitall(num_nbrs, p_req, p_status);
                        
                        /* Non-blocking receive SMA from all neighbors with tag 1 */
                        for (i = 0; i < num_nbrs; i++) {
                            MPI_Irecv(&p_recv_vals[i], 1, MPI_FLOAT, p_nbrs[i], 1, cart_comm, &p_req[i]);
                        }
                        MPI_Waitall(num_nbrs, p_req, p_status);

                        printf("Cart rank: %d; Received top: %.2f; bottom: %.2f; left: %.2f; right: %.2f;\n", my_rank, p_recv_vals[0], p_recv_vals[1], p_recv_vals[2], p_recv_vals[3]);
                        
                        /* STEP 2: Compare SMA between neighbors */
                        int count = 0;
                        for (i = 0; i < num_nbrs; i++) {
                            float range = fabs(p_recv_vals[i] - sea_moving_avg);
                            if (p_recv_vals[i] != -1 && range <= RANGE) {
                                count += 1;
                            }
                        }
                        // set notification to true when at least two neighbors have similar SMA
                        if (count >= 2) {
                            notification = 1;
                            printf("Cart rank: %d; Matched occur;\n", my_rank);
                            notification = 0;
                        }
                    }
                }
                
                sleep(CYCLE);
                counter++;
            } while (counter < 10);
        }
        /* 
            1. Listen to neighbors and send its own SMA
         */
        #pragma omp section 
        {
            int counter = 0;
            do {
                printf("Cart rank %d; Sea moving average: %.2f;\n", my_rank, sea_moving_avg);
                MPI_Status status, temp_status;
                int temp;
                int flag = 0; // placeholder to indicate a message is received or not
                
                /* Non-blocking test for a message */
                MPI_Iprobe(MPI_ANY_SOURCE, 1, cart_comm, &flag, &status);

                // send back SMA if flag is true
                if (flag) {
                    MPI_Recv(&temp, 1, MPI_INT, status.MPI_SOURCE, 1, cart_comm, &temp_status);
                    printf("Cart rank %d sends SMA %.2f.\n", my_rank, sea_moving_avg);
                    /* Blocking send the SMA to requester */
                    MPI_Send(&sea_moving_avg, 1, MPI_FLOAT, status.MPI_SOURCE, 1, cart_comm);
                }
                
                sleep(CYCLE);
                counter++;
            } while (counter < 10);
        }
    }
    
    MPI_Comm_free(&cart_comm);
}

/* Random float value generator */
float rand_float(unsigned int seed, float min, float max) {
    float rand_float = (float)(rand_r(&seed) % (int)(max - min) + min);
    return rand_float;
}