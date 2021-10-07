#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <omp.h>
#include <mpi.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include "simulations.h" // simulations header file

int main(int argc, char *argv[]) {

    /* Variables declaration */
    int my_rank, size, num_rows, num_cols, provided;
    float threashold;
    MPI_Comm nodes_comm;

    /* Initialize MPI */
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    // check if the threading support level macthes with the one provided by the implementation
    if(provided < MPI_THREAD_MULTIPLE) {
        printf("The threading support level is lesser than that demanded.\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* Split the communicator based on rank number */
    MPI_Comm_split(MPI_COMM_WORLD, my_rank > 0, 0, &nodes_comm);

    /* Initialize number of rows, cols and height of threashold */
    if (argc == 4) {
        num_rows = atoi(argv[1]);
        num_cols = atoi(argv[2]);
        threashold = atof(argv[3]);
        // check if number of processes matches the value of rows * cols
        if (num_rows * num_cols != (size - 1)) {
            if(my_rank == 0) printf("ERROR: rows(%d) * cols(%d) != size - 1(%d)", num_rows, num_cols, size - 1);
            MPI_Finalize(); 
            return 0; 
        }
    } else {
        if(my_rank == 0) printf("ERROR: Number of arguments is not 3.");
        MPI_Finalize(); 
        return 0;       
    }

    /* Switch case to call different function based on rank number */
    switch (my_rank) {
        case 0: {
            printf("Root Rank: %d. Comm Size: %d. Grid Dimension = [%d x %d]. Threshold: %f.\n", my_rank, size, num_rows, num_cols, threashold);
            /* Base station */
            break;
        }
        default: {
            /* Wireless Sensor Node */
            sensor_node(num_rows, num_cols, threashold, MPI_COMM_WORLD, nodes_comm);
            break;
        }
    }

    MPI_Finalize();

    return 0;
}