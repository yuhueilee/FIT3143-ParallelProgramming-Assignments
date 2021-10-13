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
#include <omp.h>

/* Constants */
#define BASE_CYCLE 1
#define ALTIMETER_RANGE 250.0
#define ALTIMETER_CYCLE 5
#define ALTIMETER_TOLERANCE 100.00
#define DATE_TIME "%a %F %T"

/* Struct types */
struct seaheightrecord {
    struct timespec time;
    float sea_height;
};

struct reportstruct { 
    double alert_time;
    double nbr_comm_time;
    float tolerance;
    float sma[5];
    int node_matched;
    int rank[5];
    int coord[2];
    int num_messages;
};

struct basereport { 
    int filled;
    int iteration;
    char* match;
    struct reportstruct alert;
    char* alt_time;
    float alt_sea_height;
    double comm_time;
};

struct basesummary {
    double sim_time;
    int total_alert;
    int total_match;
    int total_mismatch;
    double avg_comm_time;
    int **coord;
    int *total_alert_per_node;
};

/* Function declarations */
void log_report(char *p_log_name, struct basereport report);
void log_summary(char *p_log_name, int cart_size, struct basesummary summary);

void base_station(int num_rows, int num_cols, float threshold, int max_iteration, MPI_Comm world_comm) {
    int i, size;
    MPI_Comm_size(world_comm, &size);
    int cart_size = size - 1;

    // array where altimeter stores its generated sea levels
    struct seaheightrecord altimeter_heights[cart_size];   

    char *p_log_name = "base_log.txt";
    char *p_sentinel_name = "sentinel.txt";
    int tsunami_upperbound = threshold + ALTIMETER_RANGE;
    int terminate = 0, quit;
    struct basesummary summary;
    struct timespec start;

    // initialize the array to store the coordinate of reporting node
    summary.coord = (int**)malloc(cart_size * sizeof(int*));
    for (i = 0; i < cart_size; i++) {
        summary.coord[i] = (int*)malloc(2 * sizeof(int));
    }
    for (i = 0; i < cart_size; i++) {
        summary.coord[i][0] = (int)round(i / num_cols);
        summary.coord[i][1] = (int)round(i % num_cols);
    }

    // initialize the array to store the total alerts per node
    summary.total_alert_per_node = calloc(cart_size, sizeof(int));

    // get start time of base simulation              
    timespec_get(&start, TIME_UTC);

    omp_set_num_threads(2);

    #pragma omp parallel sections
    {
        /* one thread for receiving and sending messages from sensor nodes */ 
        #pragma omp section
        {
            struct basereport reports[max_iteration];   
            struct basereport report;
            struct reportstruct alert;
            struct timespec alt_time, end, comm_end;
            double comm_time, time_taken;
            char alt_time_str[50];
            int i, l_terminate = 0;

            MPI_Request send_request[cart_size];
            MPI_Status send_status[cart_size];

            // initialize values in report and summary
            report.filled = 0;
            report.iteration = 0;
            summary.total_alert = 0;
            summary.total_match = 0;
            summary.total_mismatch = 0;
            summary.avg_comm_time = 0;

            do {
                char *buffer;
                int flag = 0, buffer_size, position = 0;
                MPI_Status probe_status;
                MPI_Iprobe(MPI_ANY_SOURCE, 0, world_comm, &flag, &probe_status);

                if (flag) {
                    flag = 0;
                    MPI_Get_count(&probe_status, MPI_PACKED, &buffer_size);
                    buffer = (char *)malloc((unsigned) buffer_size);
                    MPI_Recv(buffer, buffer_size, MPI_PACKED, probe_status.MPI_SOURCE, 0, world_comm, MPI_STATUS_IGNORE);

                    // end of communication time between node and base
                    timespec_get(&comm_end, TIME_UTC);
                    comm_time = (comm_end.tv_sec * 1e9 + comm_end.tv_nsec) * 1e-9;

                    // unpack the data into a buffer
                    MPI_Unpack(buffer, buffer_size, &position, &alert.alert_time, 1, MPI_DOUBLE, world_comm);
                    MPI_Unpack(buffer, buffer_size, &position, &alert.nbr_comm_time, 1, MPI_DOUBLE, world_comm);
                    MPI_Unpack(buffer, buffer_size, &position, &alert.tolerance, 1, MPI_INT, world_comm);
                    MPI_Unpack(buffer, buffer_size, &position, &alert.sma, 5, MPI_FLOAT, world_comm);
                    MPI_Unpack(buffer, buffer_size, &position, &alert.node_matched, 1, MPI_INT, world_comm);
                    MPI_Unpack(buffer, buffer_size, &position, &alert.rank, 5, MPI_INT, world_comm);
                    MPI_Unpack(buffer, buffer_size, &position, &alert.coord, 2, MPI_INT, world_comm);
                    MPI_Unpack(buffer, buffer_size, &position, &alert.num_messages, 1, MPI_INT, world_comm);

                    /* Handle reports received from sensor nodes */

                    // update report comm_time
                    comm_time = comm_time - alert.alert_time;
                    report.comm_time = comm_time;

                    printf("BASE STATION received %d report:\n", alert.rank[0]);
                    printf("\tAlert time: %.2f, Number of matches: %d, Rank: %d, Coord: (%d, %d)\n", alert.alert_time, alert.node_matched, alert.rank[0], alert.coord[0], alert.coord[1]);

                    // update total number of alerts received
                    summary.total_alert += 1;

                    // update total number of alerts received from reporting node
                    summary.total_alert_per_node[alert.rank[0]] += 1;

                    // update average communication time in summary
                    summary.avg_comm_time += (comm_time / summary.total_alert);

                    // set match value to Mismatch by default
                    report.match = "Mismatch";
                        
                    // check altimeter array
                    #pragma omp critical
                    {
                        alt_time = altimeter_heights[alert.rank[0]].time;
                        report.alt_sea_height = altimeter_heights[alert.rank[0]].sea_height;                        
                    }

                    // change timespec to string of date and time
                    strftime(alt_time_str, sizeof(alt_time_str), DATE_TIME, gmtime(&alt_time.tv_sec));
                    report.alt_time = alt_time_str;
                    
                    // check whether there is a match or not depending on the difference 
                    // between sea height recorded by altimeter and reported by sensor node
                    if (fabs(report.alt_sea_height - alert.sma[0]) <= ALTIMETER_TOLERANCE) { 
                        report.match = "Match";
                        summary.total_match += 1;
                    } else {
                        summary.total_mismatch += 1;
                    }

                    // add alert to report
                    report.alert = alert;

                    // add report information to array
                    report.filled = 1;
                    reports[report.iteration] = report;
                }

                /* read from text file to get the sentinel value */
                // open log file to append new information
                FILE *pSentinelFile = fopen(p_sentinel_name, "r");
                fscanf(pSentinelFile, "%d", &quit);
                fclose(pSentinelFile);
                // if quit has been changed to 1 then break loop and stop simulation
                if (quit == 1) {
                    break;
                }

                // sleep for a specified amount of time before going to the next iteration
                sleep(BASE_CYCLE);
                report.iteration++;
            } while (report.iteration < max_iteration);

            // set terminate flag to true 
            #pragma omp critical
            {
                terminate = 1;
                l_terminate = terminate;
            }

            // broadcast termination message to sensor nodes
            for (i = 0; i < cart_size; i++) {
                MPI_Isend(&l_terminate, 1, MPI_INT, i, 0, world_comm, &send_request[i]);
            }
            // wait for the terminate message to send to all the sensor nodes
            MPI_Waitall(cart_size, send_request, send_status);

            // add reports to log file
            for (i = 0; i < max_iteration; i++) {
                if (reports[i].filled == 1) {
                    log_report(p_log_name, reports[i]);
                }
            }

            // end of simulation time
            timespec_get(&end, TIME_UTC);
            time_taken = end.tv_sec - start.tv_sec;
            time_taken = (time_taken + (end.tv_nsec - start.tv_nsec) * 1e-9);
            summary.sim_time = time_taken;

            // generate summary report
            log_summary(p_log_name, cart_size, summary);
        }


        /* one thread for the satellite altimeter */ 
        #pragma omp section
        {
            float rand_sea_height;
            struct timespec alt_time;
            int i, l_terminate;

            do {
                // read global terminate flag and store it in local terminate flag
                #pragma omp critical 
                l_terminate = terminate;   

                unsigned int sea_height_seed = (unsigned int)time(NULL);
                srand(sea_height_seed);
                for (i = 0; i < cart_size; i++) {
                    // randomly generate sea level
                    float scale = ((float)rand()/(float)(RAND_MAX)); /* [0, 1.0] */
                    rand_sea_height = threshold + scale * (tsunami_upperbound - threshold);

                    // get current time
                    timespec_get(&alt_time, TIME_UTC);

                    // update altimeter_heights
                    #pragma omp critical                        
                    {
                        altimeter_heights[i].time = alt_time;  
                        altimeter_heights[i].sea_height = rand_sea_height;   
                    }
                }
                
                // sleep for a specified amount of time before going to the next iteration
                sleep(ALTIMETER_CYCLE);
            } while (l_terminate != 1);
        }
    }

    free(summary.coord);
    free(summary.total_alert_per_node);
    fflush(stdout);
}

void log_report(char *p_log_name, struct basereport report) {
    struct reportstruct alert = report.alert;
    // get log time
    struct timespec log_time;
    timespec_get(&log_time, TIME_UTC);
    const time_t alert_time_convert = alert.alert_time;
    char buff[100];

    // open log file to append new information
    FILE *pFile = fopen(p_log_name, "a");

    // Write the report into the log file
    // header information
    fprintf(pFile, "------------------------------------------------------------------------------------------------\n");
    fprintf(pFile, "Iteration: %d\n", report.iteration + 1);
    // change log time to date time string
    strftime(buff, sizeof buff, DATE_TIME, gmtime(&log_time.tv_sec));
    fprintf(pFile, "Logged time:\t\t\t\t%s\n", buff);
    // change alert time to date time string
    strftime(buff, sizeof buff, DATE_TIME, gmtime(&alert_time_convert));
    fprintf(pFile, "Alert reported time:\t\t%s\n", buff);
    fprintf(pFile, "Alert type: %s\n\n", report.match);

    // information from the reporting node
    fprintf(pFile, "Reporting Node\t\tCoord\t\tHeight(m)\n");     
    fprintf(pFile, "%d\t\t\t\t\t(%d, %d)\t\t%.3lf\n\n", alert.rank[0], alert.coord[0], alert.coord[1], alert.sma[0]);

    // information from the nodes adjacent to reporting node
    fprintf(pFile, "Adjacent Nodes\t\tCoord\t\tHeight(m)\n");   
    int i, row_disp, col_disp;
    for (i = 1; i < 5; i++) {
        // reset row and column displacement
        row_disp = 0;
        col_disp = 0;
        if (alert.rank[i] != -2) {    // if neighbour is non existant, then rank will be -2                                                
            // switch to find the displacement for neighbour coordinates
            switch (i) {
                case 1: {           // top
                    row_disp = -1;
                    break;
                } case 2: {         // bottom
                    row_disp = 1;
                    break;
                } case 3: {         // left
                    col_disp = -1;
                    break;
                } case 4: {         // right
                    col_disp = 1;
                    break;
                }
            }
            int nbr_coord_row = alert.coord[0] + row_disp;
            int nbr_coord_col = alert.coord[1] + col_disp;
            fprintf(pFile, "%d\t\t\t\t\t(%d, %d)\t\t%.3lf\n", alert.rank[i], nbr_coord_row, nbr_coord_col, alert.sma[i]);
        }
    }
    fprintf(pFile, "\n");

    // infromation from the satellite altimeter
    fprintf(pFile, "Satellite altimeter reporting time: %s\n", report.alt_time);
    fprintf(pFile, "Satellite altimeter reporting height(m): %.3lf\n", report.alt_sea_height);
    fprintf(pFile, "Satellite altimeter reporting sensor node coordinates: (%d, %d)\n\n", alert.coord[0], alert.coord[1]);

    // extra information
    int msg = 1;
    fprintf(pFile, "Total Communication time for reporting node (seconds): %lf\n", report.comm_time);
    fprintf(pFile, "\tIncluding communication time between its neighbours: %lf\n", alert.nbr_comm_time);
    fprintf(pFile, "Total messages sent by reporting node for this alert: %d\n", msg + alert.num_messages);    
    fprintf(pFile, "\tMessages to base station: %d\n", msg);    
    fprintf(pFile, "\tMessages to neighbours: %d\n", alert.num_messages);    
    fprintf(pFile, "Number of adjacent matches to reporting node: %d\n", alert.node_matched);
    fprintf(pFile, "Max. tolerance range between nodes readings(m): %.3f\n", alert.tolerance);
    fprintf(pFile, "Max. tolerance range between satellite altimeter and reporting nodes readings(m): %.3f\n", ALTIMETER_TOLERANCE);
    fprintf(pFile, "------------------------------------------------------------------------------------------------\n");

    // close the log file
    fclose(pFile);
}

void log_summary(char *p_log_name, int cart_size, struct basesummary summary) {
    int i;
    // open log file to append summary
    FILE *pFile = fopen(p_log_name, "a");

    // write summary
    fprintf(pFile, "------------------------------------------------------------------------------------------------\n\n");
    fprintf(pFile, "SUMMARY\n\n");
    fprintf(pFile, "Total simulation time: %lf\n", summary.sim_time);
    fprintf(pFile, "Total number of alerts: %d\n", summary.total_alert);
    fprintf(pFile, "\tMatched alerts: \t%d\n", summary.total_match);
    fprintf(pFile, "\tMismatched alerts: \t%d\n", summary.total_mismatch);
    fprintf(pFile, "Total number of alerts per node:\n");
    fprintf(pFile, "\tCoordinate\tNumber of alerts\n");
    for (i = 0; i < cart_size; i++) {
        fprintf(pFile, "\t(%d, %d)\t\t%d\n", summary.coord[i][0], summary.coord[i][1], summary.total_alert_per_node[i]);
    }
    fprintf(pFile, "Average communication time: %lf\n", summary.avg_comm_time);
    fprintf(pFile, "\n------------------------------------------------------------------------------------------------\n\n");

    // close the log file
    fclose(pFile);
}