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

void sensor_node(int my_rank, int my_cart_rank, MPI_Comm master_comm, MPI_Comm cart_comm) {
    printf("My rank: %d. My cart rank: %d.\n", my_rank, my_cart_rank);
}