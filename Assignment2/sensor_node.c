/*
This file contains function for simulating wireless sensor node
*/
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

#define CYCLE 5 // cycle for sea water column height generation
#define LOWER_BOUND 5500.0 // lower bound for sea water column height
#define UPPER_BOUND 6500.0 // upper bound for sea water column height

/* Function prototype */
float rand_float(unsigned int seed, float min, float max);

/* Wireless sensor node simulation */
void sensor_node(int num_rows, int num_cols, float threshold, MPI_Comm world_comm, MPI_Comm nodes_comm) {
    bool terminate = 0; // set terminate to false initially
    int i, my_rank, my_cart_rank;
    int ndims = 2, reorder = 1, ierr = 0;
    int dims[ndims];
    int coord[ndims];
    int wrap_around[ndims];
    MPI_Comm cart_comm;

    // assign rows and cols to dims array
    dims[0] = num_rows;
    dims[1] = num_cols;

    /* Store the rank number from nodes_comm */
    MPI_Comm_rank(nodes_comm, &my_rank);

    /* Set periodic shift to false by initializing
    wrap_around array to 0 */
    for (i = 0; i < ndims; i++) {
        wrap_around[i] = 0;
    }

    /* Create cartesian topology using nodes communicator */    
    ierr = MPI_Cart_create(nodes_comm, ndims, dims, wrap_around, reorder, &cart_comm);
    if(ierr != 0) {
        printf("ERROR[%d] creating 2D CART\n", ierr);
    }
    
    MPI_Cart_coords(cart_comm, my_rank, ndims, coord); // use my rank to find my coordinates in the cartesian communicator group
    MPI_Cart_rank(cart_comm, coord, &my_cart_rank); // use my cartesian coordinates to find my cart rank in cartesian group
    
    printf("Cart rank: %d; Cart Coord: (%d, %d);\n", my_cart_rank, coord[0], coord[1]);
    
    sleep(my_rank); // this is to have different seed value for random float generator

    int counter = 0;
    /* Iterations */
    do {
        /* Generate random sea value */
        unsigned int seed = time(NULL); // seed value to generate different random value for each process
        float rand_sea_height = rand_float(seed, LOWER_BOUND, UPPER_BOUND);
        printf("Rank %d. Random sea level: %.2f\n", my_rank, rand_sea_height);
        
        sleep(CYCLE);
        counter++;
    } while (counter < 10);
    
    MPI_Comm_free(&cart_comm);
}

/* Random float value generator */
float rand_float(unsigned int seed, float min, float max) {
    float rand_float = (float)(rand_r(&seed) % (int)(max - min) + min);
    return rand_float;
}