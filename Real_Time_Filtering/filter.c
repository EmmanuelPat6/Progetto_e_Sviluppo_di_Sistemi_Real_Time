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


// 2nd-order Butterw. filter, cutoff at 2Hz @ fc = 50Hz
double b [3] = {0.0134,    0.0267,    0.0134};
double a [3] = {1.0000,   -1.6475,    0.7009};


/*Buffer*/
struct shared_buffer {
	double buffer_1[BUF_SIZE];
	double buffer_2[BUF_SIZE];
	double buffer_3[BUF_SIZE];
	pthread_mutex_t mutex;
};

static struct shared_buffer sig_filtered;

static int keep_on_running = 1;	//Viene usato dai loop dei 3 thread. Faccio i loop di ogni thread finchè keep_on_running è vera

/*Variabili Temporali*/
static struct timespec glob_time;
static struct timespec start_time;
static double tempo;

/*Signal*/
struct shared_double{
	double value;
	pthread_mutex_t mutex;
};

static struct shared_double sig_noise;

static double sig_val;
static double sig_filt_1, sig_filt_2, sig_filt_3;

/* Thread Generator*/
void * generator(void* par) {

	periodic_thread *th_g = (periodic_thread *) par;
	start_periodic_timer(th_g,TICK_PERIOD);
	
	pthread_mutex_lock(&sig_noise.mutex);
	sig_noise.value = 0;	//Inizializzo il rumore a 0
	pthread_mutex_unlock(&sig_noise.mutex);
	
	double noise;
	
	clock_gettime(CLOCK_REALTIME, &start_time);	//Prendo il tempo iniziale prima del ciclo while

	while (keep_on_running)
	{
		wait_next_activation(th_g);

		pthread_mutex_lock(&sig_noise.mutex);
		noise = sig_noise.value;
		pthread_mutex_unlock(&sig_noise.mutex);
		
		/* Calcolo del tempo attuale */
		clock_gettime(CLOCK_REALTIME, &glob_time);
		tempo = (double)(glob_time.tv_sec-start_time.tv_sec) + (double)(glob_time.tv_nsec-start_time.tv_nsec)/NSEC_PER_SEC;
		//tempo = (double)(difference_ns(&glob_time,&start_time))/NSEC_PER_SEC;

		/* Generazione del segnale */
		sig_val = sin(2*M_PI*SIG_HZ*tempo);	//Segnale Sinusoidale con f=1Hz

		/* Contributi rumorosi cosinusoidali */
		noise = sig_val + 0.5*cos(2*M_PI*10*tempo);
		noise += 0.9*cos(2*M_PI*4*tempo);
		noise += 0.9*cos(2*M_PI*12*tempo);
		noise += 0.8*cos(2*M_PI*15*tempo);
		noise += 0.7*cos(2*M_PI*18*tempo);

		//In questo modo la sezione critica viene ridotta al minimo
		pthread_mutex_lock(&sig_noise.mutex);
		sig_noise.value = noise;
		pthread_mutex_unlock(&sig_noise.mutex);

	}

	return 0;
}

/* Thread Filter */
void * filter(void* par) {
	
	/* Message */
	char message_1 [MAX_MSG_SIZE];
	char message_2 [MAX_MSG_SIZE];
	char message_3 [MAX_MSG_SIZE];

	periodic_thread *th_f = (periodic_thread *) par;
	start_periodic_timer(th_f,TICK_PERIOD);
	
	/* Queue */
	struct mq_attr attr;
	mqd_t print_q_1, print_q_2, print_q_3;
	attr.mq_flags = 0;				
	attr.mq_maxmsg = MAX_MESSAGES;	
	attr.mq_msgsize = MAX_MSG_SIZE; 
	attr.mq_curmsgs = 0;

	double value = 0;

	/* Open Queue */
	if ((print_q_1 = mq_open (PRINT_QUEUE_NAME_1, O_WRONLY | O_CREAT | O_NONBLOCK, QUEUE_PERMISSIONS, &attr)) == -1) {
		perror ("mq_open print_q_1");
		exit (1);
	}

	if ((print_q_2 = mq_open (PRINT_QUEUE_NAME_2, O_WRONLY | O_CREAT | O_NONBLOCK, QUEUE_PERMISSIONS, &attr)) == -1) {
		perror ("mq_open print_q_2");
		exit (1);
	}

	if ((print_q_3 = mq_open (PRINT_QUEUE_NAME_3, O_WRONLY | O_CREAT | O_NONBLOCK, QUEUE_PERMISSIONS, &attr)) == -1) {
		perror ("mq_open print_q_3");
		exit (1);
	}



	double buffer_1[BUF_SIZE], buffer_2[BUF_SIZE], buffer_3[BUF_SIZE];
	int head = 0;	//Gestisco il buffer, grazie a questa variabile, in maniera circolare
	int cnt = BUF_SIZE;	
	
	while (keep_on_running) {

		wait_next_activation(th_f);

		pthread_mutex_lock(&sig_noise.mutex);
		value = sig_noise.value;
		pthread_mutex_unlock(&sig_noise.mutex);

		/* Filter */
		sig_filt_1 = get_mean_filter(value);	//Filtro a Media Mobile
		sig_filt_2 = get_butter(value, a, b);	//Filtro Butterworth del 2 Ordine
		//sig_filt_3 = chebyshevFilter(value);	//Filtro di Chebyshev del 2 Ordine
		sig_filt_3 = medianFilter(value);		//Median Filter

		//Scrivo i 50 elementi in un buffer a parte per non entrare ogni volta in sezione critica (ci entro solo quando ho raggiunto i 50 elementi)
		buffer_1[head] = sig_filt_1;
		buffer_2[head] = sig_filt_2;
		buffer_3[head] = sig_filt_3;

		head = (head+1)%BUF_SIZE;	//La testa sarà pari a 0 1 2 3 4 ... 49  , 0 1 2 3 4 ... 49  ,  0 1 2 3 4 ... 49   e così via 
		cnt--;	//Decrementa finchè non diventa 0. Questo mi indica che il buffer si è riempito e ha raggiunto i 50 elementi

		if (cnt == 0) {
			cnt = BUF_SIZE;	//Riporto cnt al valore iniziale

			//Solo qui entro in sezione critica
			pthread_mutex_lock(&sig_filtered.mutex);

			/*Copia*/
			for(int i=0; i < BUF_SIZE; i++){
				sig_filtered.buffer_1[i] = buffer_1[i];
				sig_filtered.buffer_2[i] = buffer_2[i];
				sig_filtered.buffer_3[i] = buffer_3[i];
				//printf("%lf  ",sig_filtered.buffer_1[i]);	//DEBUG
			}
			//printf("\n\n");

			pthread_mutex_unlock(&sig_filtered.mutex);

		}	

		/*Creazione Messaggi*/
		pthread_mutex_lock(&sig_noise.mutex);
		sprintf (message_1, "%lf, %lf, %lf, %lf\n", tempo, sig_val, sig_noise.value, sig_filt_1);
		sprintf (message_2, "%lf, %lf, %lf, %lf\n", tempo, sig_val, sig_noise.value, sig_filt_2);
		sprintf (message_3, "%lf, %lf, %lf, %lf\n", tempo, sig_val, sig_noise.value, sig_filt_3);
		pthread_mutex_unlock(&sig_noise.mutex);

		/* Send Message */
		if (mq_send (print_q_1, message_1, strlen (message_1) + 1, 0) == -1) {
		    //perror ("mq_send print_q_1");
			//break;
		}

		if (mq_send (print_q_2, message_2, strlen (message_2) + 1, 0) == -1) {
		    //perror ("mq_send print_q_2");
			//break;
		}

		if (mq_send (print_q_3, message_3, strlen (message_3) + 1, 0) == -1) {
		    //perror ("mq_send print_q_3");
			//break;
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
        //exit (1);
    }

	return 0;
}



/* Thread Peak Counter */
void * peak_counter(void* par) {

	periodic_thread *th_p = (periodic_thread *) par;
	start_periodic_timer(th_p,PEAK_PERIOD);

	int peak_count = 0;
	int filtro = 1;	//Inizializzo a 1

	/* Message */
	char message_peak [MAX_MSG_SIZE];
	char message_conf_peak [MAX_MSG_SIZE];

	/* Queue */
	struct mq_attr attr;
	mqd_t peak_q ,conf_peak_q;
	attr.mq_flags = 0;				
	attr.mq_maxmsg = MAX_MESSAGES;	
	attr.mq_msgsize = MAX_MSG_SIZE; 
	attr.mq_curmsgs = 0;

	/* Queue Open*/
	if ((peak_q = mq_open (PEAK_QUEUE_NAME, O_WRONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
		perror ("mq_open peak_q");
		exit (1);
	}

	if ((conf_peak_q = mq_open (CONF_PEAK_QUEUE_NAME, O_RDONLY | O_CREAT | O_NONBLOCK, QUEUE_PERMISSIONS, &attr)) == -1) {
		perror ("mq_open conf_peak_q");
		exit (1);
	}

	while (keep_on_running)
	{
		wait_next_activation(th_p);

        if (mq_receive(conf_peak_q, message_conf_peak,MAX_MSG_SIZE,NULL) == -1){
			//perror ("mq_receive conf_peak_q");	
			//break;
		}
		else{ 
			filtro=atoi(message_conf_peak);	//Conversione
		}

		pthread_mutex_lock(&sig_filtered.mutex);

		switch(filtro){

			case 1:
				//printf("Filtro 1");	//DEBUG
				for (int i = 1; i<BUF_SIZE-1; i++) {
					/* Massimo Locale*/
					if(sig_filtered.buffer_1[i] > sig_filtered.buffer_1[i-1] && sig_filtered.buffer_1[i]>sig_filtered.buffer_1[i+1]){
						peak_count++;
					}
					/* Minimo Locale */
					if(-sig_filtered.buffer_1[i] > -sig_filtered.buffer_1[i-1] && -sig_filtered.buffer_1[i]>-sig_filtered.buffer_1[i+1]){
						peak_count++;
					}
				}
				break;

			case 2:
				//printf("Filtro 2");	//DEBUG
				for (int i = 1; i<BUF_SIZE-1; i++) {
					/* Massimo Locale*/
					if(sig_filtered.buffer_2[i] > sig_filtered.buffer_2[i-1] && sig_filtered.buffer_2[i]>sig_filtered.buffer_2[i+1]){
						peak_count++;
					}
					/* Minimo Locale */
					if(-sig_filtered.buffer_2[i] > -sig_filtered.buffer_2[i-1] && -sig_filtered.buffer_2[i]>-sig_filtered.buffer_2[i+1]){
						peak_count++;
					}
				}
				break;

			case 3:
				//printf("Filtro 3");	//DEBUG
				for (int i = 1; i<BUF_SIZE-1; i++) {
					/* Massimo Locale*/
					if(sig_filtered.buffer_3[i] > sig_filtered.buffer_3[i-1] && sig_filtered.buffer_3[i]>sig_filtered.buffer_3[i+1]){
						peak_count++;
					}
					/* Minimo Locale */
					if(-sig_filtered.buffer_3[i] > -sig_filtered.buffer_3[i-1] && -sig_filtered.buffer_3[i]>-sig_filtered.buffer_3[i+1]){
						peak_count++;
					}
				}
				break;
		}

		sprintf (message_peak, "%d\n", peak_count);	//Inserisco nel messaggio in numero di picchi contati
		//printf(message_peak);
		

		pthread_mutex_unlock(&sig_filtered.mutex);
		
		/* Send Message */
		if (mq_send (peak_q, message_peak, strlen (message_peak) + 1, 0) == -1) {
		    perror ("mq_send peak_q");
		    continue;
		}

		peak_count = 0;	//Azzero il peak_count
	}

	/* Clear */
    if (mq_close (peak_q) == -1) {
        perror ("mq_close peak_q");
        exit (1);
    }

    if (mq_close (conf_peak_q) == -1) {
        perror ("mq_close conf_peak_q");
        exit (1);
    }

	return 0;	
}


/* Main */
int main(void)
{
	printf("Generator & Filter STARTED! [press 'q' to stop]\n");

	/* Thread */
	pthread_t generator_thread;
	pthread_t filter_thread;
	pthread_t peak_counter_thread;

	/* Mutex */
	pthread_mutexattr_t mymutexattr;

	/*Protocollo: Priority Inheritance*/
	pthread_mutexattr_init(&mymutexattr);
	pthread_mutexattr_setprotocol(&mymutexattr, PTHREAD_PRIO_INHERIT);

	pthread_mutex_init(&sig_noise.mutex, &mymutexattr);
	pthread_mutex_init(&sig_filtered.mutex, &mymutexattr);

	pthread_mutexattr_destroy(&mymutexattr);

	pthread_attr_t myattr;
	struct sched_param myparam;

	pthread_attr_init(&myattr);
	pthread_attr_setschedpolicy(&myattr, SCHED_FIFO);
	pthread_attr_setinheritsched(&myattr, PTHREAD_EXPLICIT_SCHED); 

	/* Thread Generator */
	periodic_thread generator_th;
	generator_th.period = TICK_PERIOD;	//Periodo 20ms
	generator_th.priority = 50;	//Priorità

	myparam.sched_priority = generator_th.priority;
	pthread_attr_setschedparam(&myattr, &myparam); 
	pthread_create(&generator_thread,&myattr,generator,(void*)&generator_th);

	/* Thread Filter */
	periodic_thread filter_th;
	filter_th.period = TICK_PERIOD;
	filter_th.priority = 50;

	myparam.sched_priority = filter_th.priority;
	pthread_attr_setschedparam(&myattr, &myparam); 
	pthread_create(&filter_thread,&myattr,filter,(void*)&filter_th);

	/* Thread Peak_Counter*/
	periodic_thread peak_counter_th;
	peak_counter_th.period = PEAK_PERIOD;
	peak_counter_th.priority = 5;

	myparam.sched_priority = peak_counter_th.priority;
	pthread_attr_setschedparam(&myattr, &myparam); 
	pthread_create(&peak_counter_thread,&myattr,peak_counter,(void*)&peak_counter_th);


	pthread_attr_destroy(&myattr);
	
	
	/* Wait user exit commands*/
	while (1) {
   		if (getchar() == 'q') break;
  	}

	keep_on_running = 0;

	/* Unlink */
	if (mq_unlink (PRINT_QUEUE_NAME_1) == -1) {
        perror ("Main: mq_unlink print_q_1");
        exit (1);
    }

	if (mq_unlink (PRINT_QUEUE_NAME_2) == -1) {
        perror ("Main: mq_unlink print_q_2");
        exit (1);
    }

	if (mq_unlink (PRINT_QUEUE_NAME_3) == -1) {
        perror ("Main: mq_unlink print_q_3");
        exit (1);
    }


	if (mq_unlink (PEAK_QUEUE_NAME) == -1) {
        perror ("Main: mq_unlink peak queue");
        exit (1);
    }


	if (mq_unlink (CONF_PEAK_QUEUE_NAME) == -1) {
        perror ("Main: mq_unlink conf_peak_q");
        exit (1);
    }
 	printf("Generator & FIlter STOPPED\n");
	return 0;
}


