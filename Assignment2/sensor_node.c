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
#define BASE_STATION_MSG 0
#define REQ_MSG 1
#define SMA_MSG 2
#define SHIFT_ROW 0
#define SHIFT_COL 1
#define DISP 1

/* Function prototype */
float rand_float(unsigned int seed, float min, float max);

/* Wireless sensor node simulation */
void sensor_node(int num_rows, int num_cols, float threshold, MPI_Comm world_comm, MPI_Comm nodes_comm) {
    int g_terminate = 0; // set terminate to false initially
    int i, my_rank, my_cart_rank;
    int num_nbrs = 4;
    int ndims = 2, reorder = 1, ierr = 0;
    int p_dims[ndims], p_coord[ndims], p_wrap_around[ndims], p_nbrs[num_nbrs];
    int index = 0; // pointer to the array storing sea values
    int window_size = 5; // size of the array storing sea values
    float *p_sea_array = calloc(window_size, sizeof(float)); // initialize the array
    float g_sea_moving_avg = 0.0;
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

    printf("Cart rank: %d. Cart Coord: (%d, %d).\n", my_cart_rank, p_coord[0], p_coord[1]);
    
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
            MPI_Status probe_status;
            int j;
            int flag; // placeholder to indicate a message is received or not
            int base_station_msg;
            int l_terminate = 0; // local terminiate variable
            int count; // count the number of matched SMA
            float sum;
            float l_sea_moving_avg = 0.0; // local SMA variable

            do {
                /* STEP 1: Generate random sea value */
                unsigned int seed = time(NULL); // seed value to generate different random value for each process
                float rand_sea_height = rand_float(seed, LOWER_BOUND, UPPER_BOUND);
                
                /* Push the new random value to array */
                p_sea_array[index] = rand_sea_height;
                index = (index + 1) % window_size; // update the index (circular)
                
                // check if the array is filled up with values
                if (p_sea_array[index] != 0.0) {
                    sum = 0.0;
                    for (j = 0; j < window_size; j++) {
                        sum += p_sea_array[j];
                    }
                    /* Calculate the new SMA */
                    #pragma omp critical 
                    g_sea_moving_avg = sum / window_size; // lock the shared variable
                    #pragma omp critical 
                    l_sea_moving_avg = g_sea_moving_avg; // assign shared SMA to local SMA

                    // check if the SMA exceeds the threshold
                    if (l_sea_moving_avg > threshold) {
                        /* Non-blocking send request to all neighbors with tag REQ_MSG */
                        for (j = 0; j < num_nbrs; j++) {
                            int request = 1;
                            MPI_Isend(&request, 1, MPI_INT, p_nbrs[j], REQ_MSG, cart_comm, &p_req[j]);
                        }
                        MPI_Waitall(num_nbrs, p_req, p_status);
                        
                        /* Non-blocking receive SMA from all neighbors with tag SMA_MSG */
                        for (j = 0; j < num_nbrs; j++) {
                            MPI_Irecv(&p_recv_vals[j], 1, MPI_FLOAT, p_nbrs[j], SMA_MSG, cart_comm, &p_req[j]);
                        }
                        MPI_Waitall(num_nbrs, p_req, p_status);

                        printf("Cart rank: %d. Received top: %.2f. bottom: %.2f. left: %.2f. right: %.2f.\n", my_rank, p_recv_vals[0], p_recv_vals[1], p_recv_vals[2], p_recv_vals[3]);
                        
                        /* STEP 2: Compare SMA between neighbors */
                        count = 0;
                        for (j = 0; j < num_nbrs; j++) {    
                            float range = fabs(p_recv_vals[j] - l_sea_moving_avg);
                            if (p_nbrs[j] != -2 && range <= RANGE) {
                                count += 1;
                            }
                        }
                        // set notification to true when at least two neighbors have similar SMA
                        if (count >= 2) {
                            printf("Cart rank %d reports to base station.\n", my_rank);
                        }
                    }
                }
                
                /* STEP 3: Listen to base station */
                MPI_Iprobe(0, BASE_STATION_MSG, world_comm, &flag, &probe_status);
                
                // receive message from base station if flag is true
                if (flag) {
                    /* Blocking receive from base station with tag BASE_STATION_MSG */
                    MPI_Recv(&base_station_msg, 1, MPI_INT, probe_status.MPI_SOURCE, BASE_STATION_MSG, world_comm, &probe_status);
                    #pragma omp critical 
                    g_terminate = base_station_msg;
                }

                /* Retrieve terminate value from shared variable */
                #pragma omp critical
                l_terminate = g_terminate;

                sleep(CYCLE);
            } while (! l_terminate);
        }
        /* 
            1. Listen to neighbors and send its own SMA
         */
        #pragma omp section 
        {
            MPI_Status probe_status;
            int req_msg;
            int flag = 0; // placeholder to indicate a message is received or not
            int l_terminate = 0; // local terminiate variable
            float l_sea_moving_avg = 0.0; // local SMA variable

            do {
                #pragma omp critical 
                l_sea_moving_avg = g_sea_moving_avg; // assign shared SMA to local SMA

                /* Non-blocking test for a message */
                MPI_Iprobe(MPI_ANY_SOURCE, REQ_MSG, cart_comm, &flag, &probe_status);

                // send back SMA if flag is true
                if (flag) {
                    flag = 0; // reset flag to false
                    /* Blocking receive from source with tag REQ_MSG */
                    MPI_Recv(&req_msg, 1, MPI_INT, probe_status.MPI_SOURCE, REQ_MSG, cart_comm, MPI_STATUS_IGNORE);
                    printf("Cart rank %d sends SMA %.2f to %d.\n", my_rank, l_sea_moving_avg, probe_status.MPI_SOURCE);
                    /* Blocking send the SMA to requester with tag SMA_MSG */
                    MPI_Send(&l_sea_moving_avg, 1, MPI_FLOAT, probe_status.MPI_SOURCE, SMA_MSG, cart_comm);
                }

                /* Retrieve terminate value from shared variable */
                #pragma omp critical
                l_terminate = g_terminate;

                sleep(1);
            } while (! l_terminate);
        }
    }
    
    MPI_Comm_free(&cart_comm);
}

/* Random float value generator */
float rand_float(unsigned int seed, float min, float max) {
    float rand_float = (float)(rand_r(&seed) % (int)(max - min) + min);
    return rand_float;
}