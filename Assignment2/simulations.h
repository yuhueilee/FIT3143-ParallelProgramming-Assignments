#ifndef simulations_h   /* Include guard */
#define simulations_h

/* prototypes */
void sensor_node(int my_rank, int my_cart_rank, MPI_Comm master_comm, MPI_Comm cart_comm);  // function declaration for sensor node simulation

#endif // simulations_h