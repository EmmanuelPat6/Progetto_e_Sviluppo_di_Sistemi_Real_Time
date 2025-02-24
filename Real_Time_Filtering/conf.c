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
#include "parameters.h"


int main(int argc, char ** argv)
{
    if (argc != 2) {	
	    printf("Inserire solo il numero del filtro da utilizzare\n");
	    return -1;
    }

	if (argc > 1) {	//argc sempre almeno uguale a  1 perchè conta sempre almeno il nome del programma. Se >1 ho aggiunto un parametro vicino al nome
		int conv = atoi(argv[1]);
	    if (conv < 1 || conv > 3) {
		    //error
		    printf("Errore in input, il filtro %d non esiste...Inserire un valore compreso tra 1 e 3\n", conv);
            exit(1);
	    }
    }

	struct mq_attr attr;

	attr.mq_flags = 0;
	attr.mq_maxmsg = MAX_MESSAGES;
	attr.mq_msgsize = MAX_MSG_SIZE;
	attr.mq_curmsgs = 0;

    mqd_t conf_peak_q, conf_store_q;

    if ((conf_peak_q=mq_open(CONF_PEAK_QUEUE_NAME,O_WRONLY))==-1) {
        perror("mq_open conf_peak_q");
        return -1;
    }
    
    if ((conf_store_q=mq_open(CONF_STORE_QUEUE_NAME,O_WRONLY))==-1) {
        perror("mq_open conf_store_q");
        return -1;
    }


    if(mq_send(conf_peak_q,argv[1],strlen(argv[1])+1,0)==-1) {
        perror("mq_send conf_peak_q");
        return -1;
    }
    
    if(mq_send(conf_store_q,argv[1],strlen(argv[1])+1,0)==-1) {
        perror("mq_send conf_store_q");
        return -1;
    }

    printf("Filtro scelto: %s\n",argv[1]);

    if (mq_close (conf_peak_q) == -1) {
        perror ("mq_close conf_peak_q");
        return -1;
    }
    if (mq_close (conf_store_q) == -1) {
        perror ("mq_close conf_store_q");
        return -1;
    }
    return 0;
}