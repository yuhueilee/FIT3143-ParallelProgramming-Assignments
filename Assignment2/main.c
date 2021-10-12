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

#define MAX_RANGE 500.0

int main(int argc, char *argv[]) {

    /* Variables declaration */
    int my_rank, size, num_rows, num_cols, max_iterations, provided;
    float threshold;
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

    /* Initialize number of rows, cols and height of threshold */
    if (argc == 5) {
        num_rows = atoi(argv[1]);
        num_cols = atoi(argv[2]);
        threshold = atof(argv[3]);
        max_iterations = atof(argv[4]);
        // check if number of processes matches the value of rows * cols
        if (num_rows * num_cols != (size - 1)) {
            if(my_rank == 0) printf("ERROR: rows(%d) * cols(%d) != size - 1(%d).\n", num_rows, num_cols, size - 1);
            MPI_Finalize(); 
            return 0; 
        } else if (threshold < MAX_RANGE) {
            if(my_rank == 0) printf("ERROR: threshold (%.2f) is less than the minimum boundary (%.2f).\n", threshold, MAX_RANGE);
            MPI_Finalize(); 
            return 0;
        } else if (max_iterations < 1) {
            if(my_rank == 0) printf("ERROR: max_iterations (%d) has to be at least 1.\n", max_iterations);
            MPI_Finalize(); 
            return 0;
        }
    } else {
        if(my_rank == 0) printf("ERROR: Number of arguments is not 4.\n");
        MPI_Finalize(); 
        return 0;       
    }

    /* Switch case to call different function based on rank number */
    switch (my_rank) {
        case 0: {
            // create a sentinel file for user to terminate the program
            char *p_sentinel_name = "sentinel.txt";
            FILE *pSentinelFile = fopen(p_sentinel_name, "w");
            fprintf(pSentinelFile, "0");
            fclose(pSentinelFile);

            // print statement for user to terminate the program
            printf("To terminate the program, change the value to 1 in sentinel.txt file.\n");
            printf("Root Rank: %d. Comm Size: %d. Grid Dimension = [%d x %d]. Threshold: %f.\n", my_rank, size, num_rows, num_cols, threshold);
            
            /* Base station */
            base_station(p_sentinel_name, threshold, max_iterations, MPI_COMM_WORLD);
            break;
        }
        default: {
            /* Wireless Sensor Node */
            sensor_node(num_rows, num_cols, threshold, MPI_COMM_WORLD, nodes_comm);
            break;
        }
    }

    MPI_Finalize();

    return 0;
}