#ifndef simulations_h   /* Include guard */
#define simulations_h

/* prototypes */
void sensor_node(int num_rows, int num_cols, float threshold, MPI_Comm world_comm, MPI_Comm cart_comm);  // function declaration for sensor node simulation
int base_station(int threshold, int max_iteration, MPI_Comm world_comm);

#endif // simulations_h