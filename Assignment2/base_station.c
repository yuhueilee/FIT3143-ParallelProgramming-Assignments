/*
This file contains function for simulating the base station and the satellite altimeter
*/
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

/* Constants */
#define NUM_OF_ITR 5
#define ITR_TIME 5
#define ALTIMETER_CYCLE 10
#define TSUNAMI_LOWERBOUND 6000
#define TSUNAMI_RANGE 3000

static struct altimeterRecord{                 // https://stackoverflow.com/questions/22727404/making-a-tuple-in-c/22727432
    char *time;
    int seaLvl;
}

int base_station(int threshold, MPI_Comm master_comm, MPI_Comm cart_comm) {
    int cartSize;
    MPI_Comm_Size(cart_comm, cartSize);

    // create an array that holds incoming reports for each node
    int reports[cartSize];

    // array where altimeter stores its generated sea levels
    struct altimeterRecord altimeterHeights[cartSize];   

    char *pLogFileName = "base_log.txt";
    bool terminate = false;
    int ndims = 2;
    time_t time;
    char *dateTime;

    // initialize values for all nodes in altimeterHeights array
    for (int i = 1; i < cartSize; i++) {
        // generate random sea level
        unsigned int seaLvlSeed = (unsigned int)time(NULL);
        randSeaLvl = rand_r(&seaLvlSeed) % TSUNAMI_RANGE + TSUNAMI_LOWERBOUND;
        
        // get current date and time
        time = time(NULL)
        dateTime = asctime(localtime(&time));

        // update altimeterHeights for the node with rank i
        altimeterHeights[i].time = dateTime;
        altimeterHeights[i].seaLvl = randSeaLvl;
    }

    omp_set_num_threads(2);

    #pragma omp parallel sections // shared(altimeterHeights, terminate)
    {
        // one thread for receiving and sending messages from sensor nodes
        #pragma omp section
        {
            int itr = 0;
            time_t currTime, prevTime;
            int altHeight;
            bool match;

            MPI_Request receive_request[cartSize];

            while (itr != NUM_OF_ITR) {
                // get current time
                currTime = time(NULL);

                if (currTime - prevTime >= ITR_TIME) {
                    // receive from each node
                    for (int i = 1; i < cartSize; i++) {
                        MPI_Irecv(&reports[i], 1, MPI_INT, i, 0, cart_comm, &receive_request[i]);
                    }

                    // check reports array
                    for (int i = 1; i < cartSize; i++) {
                        // if receive something
                        if (reports[i] != NULL) {
                            
                            // check altimeter array
                            altHeight = altimeterHeights[i].seaLvl;

                            if ((altHeight - reports[i]) < 100) {           // change reports later
                                match = true;
                            }   

                            // reset to NULL
                            reports[i] = NULL;                      // cannot set int to null, but in struct can set seaLvl to default value 0 i guess

                            // add report to log file
                            log_report(pLogFileName);
                        }
                    }

                    // update prevTime
                    prevTime = currTime
                }

                // next iteration
                itr++;
            }

            // set terminate flag to true 
            terminate = true;

            // broadcast termination message to sensor nodes
            MPI_Bcast(&terminate, 1, MPI_INT, 0, cart_comm);

            // generate summary report
            log_summary(pLogFileName);
        }

        // one thread for the satellite altimeter
        #pragma omp section
        {
            time_t currTime, prevTime;
            int randRank, randCoord[2], randSeaLvl;
            char *currDateTime;

            // initialize prevTime
            prevTime = time(NULL);

            while (!terminate) {
                // get current time
                currTime = time(NULL);
                
                if (currTime - prevTime >= ALTIMETER_CYCLE) {
                    // randomly generate coordinate
                    // first get random rank (an integer between 1 and cartSize)
                    unsigned int rankSeed = (unsigned int)time(NULL);
	                randRank = rand_r(&rankSeed) % cartSize + 1;       
            
                    // randomly generate sea level
                    unsigned int seaLvlSeed = (unsigned int)time(NULL);
	                randSeaLvl = rand_r(&seaLvlSeed) % TSUNAMI_RANGE + TSUNAMI_LOWERBOUND;

                    // get date and time in string
                    currDateTime = asctime(localtime(&currTime));

                    // update altimeterHeights
                    #pragma omp critical                                // not sure if this will lock since they're in diff sections
                    {
                        altimeterHeights[randRank].time = currDateTime;
                        altimeterHeights[randRank].seaLvl = randSeaLvl;    
                    }

                    // update prevTime 
                    prevTime = currTime;
                }
            }
        }
    }

    return 0;
}

void log_report(char *pLogFileName) {
    /*
    The base station writes (or logs) the key performance metrics (e.g., the simulation time, number of
    alerts detected, number of messages/events with senders’ adjacency information/addresses, total
    number of messages (for this simulation)) to an output file.
    */
    FILE *pFile = fopen(pLogFileName, "a");

    // write info to file 
	// fprintf(pFile, "format string", var);

    fclose(pFile);
}

void log_summary(char *pLogFileName) {
    FILE *pFile = fopen(pLogFileName, "a");

    // write summary

    fclose(pFile);
}