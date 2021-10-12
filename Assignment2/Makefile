ALL: main

main: main.c 
	mpicc -Wall main.c sensor_node.c base_station.c -o mainOut -lm -fopenmp

run:
	mpirun -oversubscribe -np 10 mainOut 3 3 6000 100

clean :
	/bin/rm -f mainOut *.o
	/bin/rm -f base_log *.txt
	/bin/rm -f sentinel *.txt