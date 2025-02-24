#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <mqueue.h>
#include "rt-lib.h"
#include "parameters.h"

static int keep_on_running = 1;

/* File */
FILE * outfd;	//Descrittore del file
int outfile;

void * store(void* par) {

	periodic_thread *th_s = (periodic_thread *) par;
	start_periodic_timer(th_s,STORE_PERIOD);

	/*Apertura e/o Creazione File*/
	outfile = open(OUTFILE, O_WRONLY | O_CREAT | O_TRUNC, 0640);
	outfd = fdopen(outfile, "w");

	if (outfile < 0 || !outfd) {
		perror("Unable to open/create output file. Exiting.");
		exit(1);	//CHECK in caso di errore di aprtura/creazione file
	}

    /* Message */
	char message [MAX_MSG_SIZE];
	char message_peak [MAX_MSG_SIZE];
	char message_conf_store [MAX_MSG_SIZE];

    /* Queue */
	struct mq_attr attr;

	attr.mq_flags = 0;
	attr.mq_maxmsg = MAX_MESSAGES;
	attr.mq_msgsize = MAX_MSG_SIZE;
	attr.mq_curmsgs = 0;

    mqd_t print_q_1, print_q_2, print_q_3;
	mqd_t peak_q, conf_store_q;

	/* Open Queue*/
    if ((print_q_1 = mq_open (PRINT_QUEUE_NAME_1, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS,&attr)) == -1) {
        perror ("mq_open print_q_1");
        exit (1);
    }

    if ((print_q_2 = mq_open (PRINT_QUEUE_NAME_2, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS,&attr)) == -1) {
        perror ("mq_open print_q_2");
        exit (1);
    }


    if ((print_q_3 = mq_open (PRINT_QUEUE_NAME_3, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS,&attr)) == -1) {
        perror ("mq_open print_q_3");
        exit (1);
    }

    if ((peak_q = mq_open (PEAK_QUEUE_NAME, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS,&attr)) == -1) {
        perror ("mq_open peak_q");
        exit (1);
    }


    if ((conf_store_q = mq_open (CONF_STORE_QUEUE_NAME, O_RDONLY | O_CREAT | O_NONBLOCK, QUEUE_PERMISSIONS,&attr)) == -1){
        perror ("mq_open conf_store_q");
        exit (1);
    }

	int cnt = 5;
	int filtro = 1;
	
	while(keep_on_running){

		/* Receive Message */

		if (mq_receive(conf_store_q, message_conf_store,MAX_MSG_SIZE,NULL) == -1){
			//perror ("mq_receive conf_store_q");	
			//break;
		}
		else{ 
			filtro=atoi(message_conf_store);
		}

		for(int i = 0; i < MAX_MESSAGES; i++){

			switch(filtro){

				case 1:
					//printf("Filtro 1");	//DEBUG

					/* Receive Message */
					if (mq_receive(print_q_1, message, MAX_MSG_SIZE,NULL) == -1){
						perror ("mq_receive print_q_1");	
						break;
					}
					/* Stampa su file*/
					else{ 
						fprintf(outfd, "%s\n", message);
						fflush(outfd);
					}
				break;

				case 2:
					//printf("Filtro 2");	//DEBUG
					/* Receive Message */
					if (mq_receive(print_q_2, message,MAX_MSG_SIZE,NULL) == -1){
						perror ("mq_receive print_q_2");	
						break;
					}
					/* Stampa su file*/
					else{ 
						fprintf(outfd, "%s\n", message);
						fflush(outfd);
					}
				break;

				case 3:
					//printf("Filtro 3");	//DEBUG
					/* Receive Message */
					if (mq_receive(print_q_3, message,MAX_MSG_SIZE,NULL) == -1){
						perror ("mq_receive print_q_3");	
						break;
					}
					/* Stampa su file*/
					else{ 
						fprintf(outfd, "%s\n", message);
						fflush(outfd);
					}

				break;

			}
		}

		cnt--;

		if (cnt == 0) {
			cnt = 5;	//Riporto cnt al valore iniziale
			if (mq_receive(peak_q, message_peak,MAX_MSG_SIZE,NULL) == -1){
				perror ("mq_receive peak_q");	
				break;
			}
			/* Stampa del numero di picchi*/
			else{ 
				printf("Last Peaks: %s",message_peak);
			}

		}
	}

	/* Clear */
	if (mq_close (print_q_1) == -1) {
        perror ("mq_close print_q_1");
        exit (1);
    }

	if (mq_close (print_q_2) == -1) {
        perror ("mq_close print_q_2");
        exit (1);
    }

	if (mq_close (print_q_3) == -1) {
        perror ("mq_close print_q_3");
        exit (1);
    }

	if (mq_close (conf_store_q) == -1) {
        perror ("mq_close conf_store_q");
        exit (1);
    }

    if (mq_close (peak_q) == -1) {
        perror ("mq_close peak_q");
        exit (1);
    }
	return 0;
}

int main(void)
{
	printf("Store STARTED! [press 'q' to stop]\n");
	
	/* Thread*/
	pthread_t store_thread;

	pthread_attr_t myattr;
	struct sched_param myparam;

	pthread_attr_init(&myattr);
	pthread_attr_setschedpolicy(&myattr, SCHED_FIFO);
	pthread_attr_setinheritsched(&myattr, PTHREAD_EXPLICIT_SCHED);

	/* Thread Store */
	periodic_thread store_th;
	store_th.period = STORE_PERIOD;	//Periodo 200ms
	store_th.priority = 10;	//Priorità

	myparam.sched_priority = store_th.priority;
	pthread_attr_setschedparam(&myattr, &myparam); 
	pthread_create(&store_thread,&myattr,store,(void*)&store_th);

	pthread_attr_destroy(&myattr);
	
	/* Wait user exit commands*/
	while (1) {
   		if (getchar() == 'q') break;
  	}

	keep_on_running = 0;

	/*Unlink*/
	if (mq_unlink (CONF_STORE_QUEUE_NAME) == -1) {
        perror ("Main: mq_unlink conf_store_q");
        exit (1);
    }

 	printf("Store STOPPED\n");
	return 0;
}



