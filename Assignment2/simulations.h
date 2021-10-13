#ifndef simulations_h   /* Include guard */
#define simulations_h

/* prototypes */
void sensor_node(int num_rows, int num_cols, float threshold, MPI_Comm world_comm, MPI_Comm cart_comm);  // function declaration for sensor node simulation
void base_station(int num_rows, int num_cols, float threshold, int max_iteration, MPI_Comm world_comm); // function declaration for base station simulation

#endif // simulations_h