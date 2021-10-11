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

#define NODE_CYCLE 5 // cycle for sea water column height generation
#define NODE_LOWERBOUND 6400.0 // lower bound for sea water column height
#define NODE_UPPERBOUND 6500.0 // upper bound for sea water column height
#define NODE_TOLERANCE 100.0 // tolerence range to compare SMA between nodes
#define BASE_STATION_RANK 0
#define BASE_STATION_MSG 0
#define REQ_MSG 1
#define SMA_MSG 2
#define SHIFT_ROW 0
#define SHIFT_COL 1
#define DISP 1

/* Type struct to store the alert report */
struct reportstruct { 
    double alert_time;
    float tolerance;
    float sma[5];
    int node_matched;
    int rank[5];
    int coord[2];
};

/* Function prototype */
float rand_float(unsigned int seed, float min, float max);

/* Wireless sensor node simulation */
void sensor_node(int num_rows, int num_cols, float threshold, MPI_Comm world_comm, MPI_Comm nodes_comm) {
    int g_terminate = 0; // set terminate to false initially
    int i, my_rank, my_cart_rank;
    int num_nbrs = 4;
    int ndims = 2, reorder = 1, ierr = 0;
    int p_dims[ndims], p_coord[ndims], p_wrap_around[ndims], p_nbrs[num_nbrs];
    float g_sea_moving_avg = 0.0;
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
            /* Create report typestruct instance */
            struct reportstruct report;
            MPI_Datatype report_type;
            MPI_Datatype type[6] = { MPI_DOUBLE, MPI_FLOAT, MPI_FLOAT, MPI_INT, MPI_INT, MPI_INT };
            int blocklen[6] = { 1, 1, 5, 1, 5, 2 };
            MPI_Aint disp[6];
            // get the addresses
            MPI_Get_address(&report.alert_time, &disp[0]);
            MPI_Get_address(&report.tolerance, &disp[1]);
            MPI_Get_address(&report.sma, &disp[2]);
            MPI_Get_address(&report.node_matched, &disp[3]);
            MPI_Get_address(&report.rank, &disp[4]);
            MPI_Get_address(&report.coord, &disp[5]);
            // calculate the displacements
            disp[5] = disp[5] - disp[4];
            disp[4] = disp[4] - disp[3];
            disp[3] = disp[3] - disp[2];
            disp[2] = disp[2] - disp[1];
            disp[1] = disp[1] - disp[0];
	        disp[0] = 0;
            // Create MPI struct
            MPI_Type_create_struct(6, blocklen, disp, type, &report_type);
            MPI_Type_commit(&report_type);

            /* Create request and status array for send/recv operations between nodes */
            MPI_Request p_req[num_nbrs * 2];
            MPI_Status p_status[num_nbrs * 2];
            /* Create request and status for send operation between node and base station */
            MPI_Request req;
            MPI_Status status;
            /* Create status for probing terminate message from base station */
            MPI_Status probe_status;

            char *buffer;
            int buffer_size, position;
            int j, request = 1;
            int send_recv_flag = 0, alter_flag = 0, flag = 0; // placeholder to indicate a message is received or not
            int base_station_msg;
            int l_terminate = 0; // local terminiate variable
            int count; // count the number of matched SMA
            int index = 0; // pointer to the array storing sea values
            int window_size = 2; // size of the array storing sea values
            float sum;
            float l_sea_moving_avg = 0.0; // local SMA variable
            float *p_sea_array = calloc(window_size, sizeof(float)); // initialize the array
            float p_recv_vals[4] = { -1.0, -1.0, -1.0, -1.0 };
            double curr_time;
            double time_taken;
            struct timespec p_timestamp[window_size];

            do {
                /* STEP 1: Generate random sea value */
                unsigned int seed = time(NULL); // seed value to generate different random value for each process
                float rand_sea_height = rand_float(seed, NODE_LOWERBOUND, NODE_UPPERBOUND);
                
                /* Push the new random value to array */
                p_sea_array[index] = rand_sea_height;
                /* Assign time stamp */
                clock_gettime(CLOCK_MONOTONIC, &p_timestamp[index]);
                index = (index + 1) % window_size; // update the index (circular)

                struct timespec timestamp = p_timestamp[index - 1];
                timespec_get(&timestamp, TIME_UTC);
                char buff[100];
                strftime(buff, sizeof(buff), "%D %T", gmtime(&timestamp.tv_sec));
                printf("Cart rank %d has random sea height %lf at time %s UTC.\n", my_rank, rand_sea_height, buff);
                
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
                            MPI_Isend(&request, 1, MPI_INT, p_nbrs[j], REQ_MSG, cart_comm, &p_req[j]);
                        }

                        /* Non-blocking receive SMA from all neighbors with tag SMA_MSG */
                        for (j = 0; j < num_nbrs; j++) {
                            p_recv_vals[j] = -1.0; // initialize to -1.0
                            MPI_Irecv(&p_recv_vals[j], 1, MPI_FLOAT, p_nbrs[j], SMA_MSG, cart_comm, &p_req[num_nbrs + j]);
                        }

                        curr_time = MPI_Wtime();
                        time_taken = MPI_Wtime() - curr_time;
                        /* Loop until all requests have completed and time taken less than 2 times the cycle */
                        do {
                            send_recv_flag = 0; // reset flag to false
                            /* Test for all previously initiated requests */
                            MPI_Testall(num_nbrs * 2, p_req, &send_recv_flag, p_status);
                            time_taken = MPI_Wtime() - curr_time; // calculate the time taken
                        } while ((! send_recv_flag) && time_taken < 2 * NODE_CYCLE);

                        if (! send_recv_flag) {
                            MPI_Cancel(p_req); // cancel all requests
                            printf("Cart rank %d time taken %.2f hence cancel all requests.\n", my_rank, time_taken);
                        } else {
                            send_recv_flag = 0; // reset flag to false
                            printf("Cart rank %d has SMA %.3f. Received top: %.2f, bottom: %.2f, left: %.2f, right: %.2f.\n", my_rank, l_sea_moving_avg, p_recv_vals[0], p_recv_vals[1], p_recv_vals[2], p_recv_vals[3]);
                        
                            /* STEP 2: Compare SMA between neighbors */
                            count = 0;
                            for (j = 0; j < num_nbrs; j++) {    
                                float range = fabs(p_recv_vals[j] - l_sea_moving_avg);
                                if (p_recv_vals[j] != -1.0 && p_nbrs[j] != -2 && range <= NODE_TOLERANCE) {
                                    count += 1;
                                }
                            }
                            // sends report to base station when at least two neighbors have similar SMA
                            if (count >= 2) {
                                /* Fill in the report */
                                struct timespec timestamp;
                                clock_gettime(CLOCK_MONOTONIC, &timestamp);
                                // get current time in seconds
                                report.alert_time = (timestamp.tv_sec * 1e9 + timestamp.tv_nsec) * 1e-9;
                                report.tolerance = NODE_TOLERANCE;
                                report.sma[0] = rand_sea_height;
                                for (j = 0; j < num_nbrs; j++) {
                                    report.sma[j + 1] = p_recv_vals[j];
                                }
                                report.node_matched = count;
                                report.rank[0] = my_rank;
                                for (j = 0; j < num_nbrs; j++) {
                                    report.rank[j + 1] = p_nbrs[j];
                                }                                
                                report.coord[0] = p_coord[0];
                                report.coord[1] = p_coord[1];

                                /* Pack data into buffer */
                                // get the upperbound for buffer size
                                MPI_Pack_size(6, report_type, world_comm, &buffer_size);
                                // allocate space for buffer
                                buffer = (char *)malloc((unsigned) buffer_size);
                                // reset position
                                position = 0;
                                // pack the data into a buffer
                                MPI_Pack(&report.alert_time, 1, MPI_DOUBLE, buffer, buffer_size, &position, world_comm);
                                MPI_Pack(&report.tolerance, 1, MPI_INT, buffer, buffer_size, &position, world_comm);
                                MPI_Pack(&report.sma, 5, MPI_FLOAT, buffer, buffer_size, &position, world_comm);
                                MPI_Pack(&report.node_matched, 1, MPI_INT, buffer, buffer_size, &position, world_comm);
                                MPI_Pack(&report.rank, 5, MPI_INT, buffer, buffer_size, &position, world_comm);
                                MPI_Pack(&report.coord, 2, MPI_INT, buffer, buffer_size, &position, world_comm);

                                /* Non-blocking send the packed message to base station */
                                MPI_Isend(buffer, buffer_size, MPI_PACKED, BASE_STATION_RANK, BASE_STATION_MSG, world_comm, &req);

                                curr_time = MPI_Wtime();
                                time_taken = MPI_Wtime() - curr_time;
                                /* Loop until all requests have completed and time taken less than 2 times the cycle */
                                do {
                                    alter_flag = 0; // reset flag to false
                                    MPI_Test(&req, &alter_flag, &status);
                                    time_taken = MPI_Wtime() - curr_time; // calculate the time taken
                                } while ((! alter_flag) && time_taken < 2 * NODE_CYCLE);

                                if (alter_flag) {
                                    alter_flag = 0; // reset flag to false
                                    printf("Cart rank %d sends report to base station.\n", my_rank);
                                    printf("\tAlert time: %.2f, Number of matches: %d, Coord: (%d, %d)\n", report.alert_time, report.node_matched, report.coord[0], report.coord[1]);
                                } else {
                                    MPI_Cancel(&req); // cancel all requests
                                    printf("Cart rank %d time taken %.2f hence cancel all requests for sending report.\n", my_rank, time_taken);
                                }
                                
                            }
                        }
                    }
                }
                
                /* STEP 3: Listen to base station */
                MPI_Iprobe(BASE_STATION_RANK, BASE_STATION_MSG, world_comm, &flag, &probe_status);
                
                // receive message from base station if flag is true
                if (flag) {
                    flag = 0; // reset flag to false
                    /* Blocking receive from base station with tag BASE_STATION_MSG */
                    MPI_Recv(&base_station_msg, 1, MPI_INT, probe_status.MPI_SOURCE, BASE_STATION_MSG, world_comm, MPI_STATUS_IGNORE);
                    printf("Cart rank %d received terminate from base station.\n", my_rank);
                    
                    /* Assign received terminate value to shared variable */
                    #pragma omp critical 
                    g_terminate = base_station_msg;
                }

                /* Retrieve terminate value from shared variable */
                #pragma omp critical
                l_terminate = g_terminate;

                sleep(NODE_CYCLE);
            } while (! l_terminate);
            /* Free the heap array */
            free(p_sea_array);
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
                    /* Blocking send the SMA to requester with tag SMA_MSG */
                    MPI_Send(&l_sea_moving_avg, 1, MPI_FLOAT, probe_status.MPI_SOURCE, SMA_MSG, cart_comm);
                }

                /* Retrieve terminate value from shared variable */
                #pragma omp critical
                l_terminate = g_terminate;

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