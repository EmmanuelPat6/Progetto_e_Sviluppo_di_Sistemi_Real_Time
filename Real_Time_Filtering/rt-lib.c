#define _GNU_SOURCE
#include<stdio.h>
#include<unistd.h>
#include "rt-lib.h"

/******************************FUNZIONI DI UTILIÀ**************************************/
void timespec_add_us(struct timespec *t, unsigned long d)
{
    d *= 1000;
    t->tv_nsec += d;
    t->tv_sec += t->tv_nsec / NSEC_PER_SEC;
    t->tv_nsec %= NSEC_PER_SEC;
}

unsigned long int difference_ns(struct timespec *ts1, struct timespec *ts2){
	long int diff_sec, diff_nsec;
	diff_sec =(ts1->tv_sec - ts2->tv_sec);
	diff_nsec = (ts1->tv_nsec - ts2->tv_nsec);
	return diff_sec*NSEC_PER_SEC + diff_nsec;
}

/* return 1 if t1>t2, 0 otherwise*/
int compare_time(struct timespec *t1,struct timespec *t2){
    if(t1->tv_sec > t2->tv_sec){
        return 1;
    }
    else if(t1->tv_sec == t2->tv_sec && t1->tv_nsec > t2->tv_nsec){
        return 1;
    }
    else{
        return 0;
    }
}

void wait_next_activation(periodic_thread * thd)
{
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &(thd->r), NULL);
    timespec_add_us(&(thd->r), thd->period);
}

void start_periodic_timer(periodic_thread * thd, unsigned long offs)
{
    clock_gettime(CLOCK_REALTIME, &(thd->r));
    timespec_add_us(&(thd->r), offs);
}

void busy_sleep(int us){
	int ret=0;
	struct timespec start;
	struct timespec end;
	struct timespec now;
	
	ret = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	if(ret == -1){
		printf("ERROR: busy_wait %d\n",getpid());
	}
	end = start;
	timespec_add_us(&end,us);
	
	//Continua finché END(tempo di fine) è maggiore di now
	do{
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
	}while(compare_time(&end,&now)); 
	
	/*
	printf("END BUSY WAIT:\n START\ts:%ld, ns:%ld \n NOW\ts:%ld, ns:%ld \n END\ts:%ld, ns:%ld \n"	  //DEBUG
		,start.tv_sec,start.tv_nsec,now.tv_sec,now.tv_nsec,end.tv_sec,end.tv_nsec);			  //DEBUG
	*/
}

/**************************************************************************************/


/************************************Filtri********************************************/

/*Filtro Butterworth del 2 Ordine*/
double get_butter(double cur, double * a, double * b)	//Filtro passa-basso
{
	double retval;
	int i;

	static double in[BUTTERFILT_ORD+1];
	static double out[BUTTERFILT_ORD+1];
	
	// Perform sample shift
	for (i = BUTTERFILT_ORD; i > 0; --i) {
		in[i] = in[i-1];
		out[i] = out[i-1];
	}
	in[0] = cur;

	// Compute filtered value
	retval = 0;
	for (i = 0; i < BUTTERFILT_ORD+1; ++i) {
		retval += in[i] * b[i];
		if (i > 0)
			retval -= out[i] * a[i];
	}
	out[0] = retval;

	return retval;
}

static int first_mean=0;

/*Filtro a Media Mobile*/
double get_mean_filter(double cur)	//Filtro più semplice
{
	double retval;

	static double vec_mean[2];
	
	//Perform sample shift
	vec_mean[1] = vec_mean[0];
	vec_mean[0] = cur;

	//Compute filtered value
	if (first_mean == 0){
		retval = vec_mean[0];	//Nella prima media non ho un secondo valore con cui fare la media
		first_mean ++;
	}
	else{
		retval = (vec_mean[0] + vec_mean[1])/2;	//Media
	}
	return retval;
}

/*Filtro Chebyshev del 2 Ordine*/
double chebyshevFilter(double cur){
	
	static double a[3] = {1.0, -1.951967589, 0.952366993}; // Coefficienti di Retroazione
	static double b[3] = {-0.0038, -0.0076, -0.0038}; // Coefficienti di Feedforward
	static double x[3] = {0.0}; // Input Passato
    static double y[3] = {0.0}; // Output Passato
	
    //Perform sample shift
    x[2] = x[1];
    x[1] = x[0];
    x[0] = cur;

    //Compute filtered value
    y[2] = y[1];
    y[1] = y[0];
    y[0] = b[0] * x[0] + b[1] * x[1] + b[2] * x[2] - a[1] * y[1] - a[2] * y[2];

    return y[0];
}


/*Median Filter*/
double medianFilter(double curr) {
    // Dichiarazione dei parametri del filtro mediano
    static double buffer[FILTER_LENGTH] = {0};
    static int index = 0;

    // Aggiungi il valore corrente al buffer circolare
    buffer[index] = curr;
    index = (index + 1) % FILTER_LENGTH;

    // Ordina il buffer
    double sortedBuffer[FILTER_LENGTH];
    for (int i = 0; i < FILTER_LENGTH; i++) {
        sortedBuffer[i] = buffer[i];
    }
    for (int i = 0; i < FILTER_LENGTH - 1; i++) {
        for (int j = 0; j < FILTER_LENGTH - i - 1; j++) {
            if (sortedBuffer[j] > sortedBuffer[j + 1]) {
                double temp = sortedBuffer[j];
                sortedBuffer[j] = sortedBuffer[j + 1];
                sortedBuffer[j + 1] = temp;
            }
        }
    }

    // Trova il valore mediano nel buffer ordinato
    double median;
    if (FILTER_LENGTH % 2 == 0) {
        median = (sortedBuffer[FILTER_LENGTH / 2 - 1] + sortedBuffer[FILTER_LENGTH / 2]) / 2.0;
    } else {
        median = sortedBuffer[FILTER_LENGTH / 2];
    }

    return median;
}
/**************************************************************************************/

//void parse_cmdline(int argc, char ** argv)
//{
//	int flag_signal = 0;
//	int flag_noise = 0;
//	int flag_filtered = 0;
//	int opt;
	//Fa uno switch case	
//	while ((opt = getopt(argc, argv, "snf")) != -1) {
//		switch (opt) {
//		case 's':
//			flag_signal = 1;
//			break;
//		case 'n':
//			flag_noise = 1;
//			break;
//		case 'f':
//			flag_filtered = 1;
//			break;
//		default: /* '?' */	//Sbaglio l'input
//			fprintf(stderr, USAGE_STR, argv[0]);
//			exit(1);
//		}
//	}
	
//	if ((flag_signal | flag_noise | flag_filtered) == 0)
//	{
//		flag_signal = flag_noise = flag_filtered = 1;
//	}
//}