ALL: main

main: main.c 
	mpicc -Wall main.c sensor_node.c -o mainOut -lm

run:
	mpirun -oversubscribe -np 4 mainOut 3 1 6000

clean :
	/bin/rm -f mainOut *.o