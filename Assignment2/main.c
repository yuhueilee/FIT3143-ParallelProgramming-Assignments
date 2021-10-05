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
    int my_rank, size, num_rows, num_cols;
    double threashold;
    MPI_Comm nodes_comm;

    /* Initialize MPI */
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* Split the communicator based on rank number */
    MPI_Comm_split(MPI_COMM_WORLD, my_rank > 0, 0, &nodes_comm);

    /* Initialize number of rows, cols and height of threashold */
    if (argc == 4) {
        num_rows = atoi(argv[1]);
        num_cols = atoi(argv[2]);
        // check if number of processes matches the value of rows * cols
        if (num_rows * num_cols != (size - 1)) {
            if(my_rank == 0) printf("ERROR: rows(%d) * cols(%d) != size - 1(%d)", num_rows, num_cols, size - 1);
            MPI_Finalize(); 
            return 0; 
        }
        char *p_end = NULL;
        threashold = strtod(argv[3], &p_end);
    } else {
        if(my_rank == 0) printf("ERROR: Number of arguments is not 3.");
        MPI_Finalize(); 
        return 0;       
    }

    /* Switch case to call different function based on rank number */
    switch (my_rank) {
        case 0: {
            printf("Root Rank: %d; Comm Size: %d; Grid Dimension = [%d x %d]; Threashold: %f;\n", my_rank, size, num_rows, num_cols, threashold);
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