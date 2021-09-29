#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include "simulations.h" // simulations header file

int main(int argc, char *argv[]) {

    /* Variables declaration */
    int i;
    int my_rank, my_cart_rank, size, num_rows, num_cols;
    double threashold;
    MPI_Comm cart_comm;
    int ndims = 2, reorder = 1, ierr = 0;
    int dims[ndims];
    int coord[ndims];
    int wrap_around[ndims];
    
    /* Set periodic shift to false by initializing
    wrap_around array to 0 */
    for (i = 0; i < ndims; i++) {
        wrap_around[i] = 0;
    }

    /* Initialize MPI */
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* Initialize number of rows, cols and height of threashold */
    if (argc == 4) {
        num_rows = atoi(argv[1]);
        num_cols = atoi(argv[2]);
        // check if number of processes matches the 
        // value of rows * cols
        if (num_rows * num_cols != (size - 1)) {
            if(my_rank == 0) printf("ERROR: rows(%d) * cols(%d) != size - 1(%d)", num_rows, num_cols, size - 1);
            MPI_Finalize(); 
            return 0; 
        }
        // assign rows and cols to dims array
        dims[0] = num_rows;
        dims[1] = num_cols;
        char *p_end = NULL;
        threashold = strtod(argv[3], &p_end);
    } else {
        if(my_rank == 0) printf("ERROR: Number of arguments is not 3.");
        MPI_Finalize(); 
        return 0;       
    }

    if (my_rank == 0) {
        printf("Root Rank: %d; Comm Size: %d; Grid Dimension = [%d x %d]; Threashold: %f;\n", my_rank, size, dims[0], dims[1], threashold);
    } else {
        /* Create cartesian topology */
        MPI_Dims_create(size - 1, ndims, dims); // create dimensions
        ierr = MPI_Cart_create(MPI_COMM_WORLD, ndims, dims, wrap_around, reorder, &cart_comm);
        if(ierr != 0) {
            printf("ERROR[%d] creating 2D CART\n", ierr);
        }
        // find my coordinates in the cartesian communicator group
        MPI_Cart_coords(cart_comm, my_rank, ndims, coord); // coordinated is returned into the coord array
        // use my cartesian coordinates to find my rank in cartesian group
        MPI_Cart_rank(cart_comm, coord, &my_cart_rank);
        sensor_node(my_rank, my_cart_rank, MPI_COMM_WORLD, cart_comm);
        MPI_Comm_free(&cart_comm);
    }

    MPI_Finalize();

    return 0;
}