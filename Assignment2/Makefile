ALL: main

main: main.c 
	mpicc -Wall main.c sensor_node.c base_station.c -o mainOut -lm -fopenmp

run:
	mpirun -oversubscribe -np 5 mainOut 2 2 6000

clean :
	/bin/rm -f mainOut *.o