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

void sensor_node(int num_rows, int num_cols, double threshold, MPI_Comm world_comm, MPI_Comm nodes_comm) {
    int i, my_rank, my_cart_rank;
    MPI_Comm cart_comm;
    int ndims = 2, reorder = 1, ierr = 0;
    int dims[ndims];
    int coord[ndims];
    int wrap_around[ndims];

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
    
    MPI_Comm_free(&cart_comm);
}