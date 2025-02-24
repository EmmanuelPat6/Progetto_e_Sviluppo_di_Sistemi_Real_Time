#ifndef __RTLIB
#define __RTLIB
#include <time.h>
#include <math.h>

#define NSEC_PER_SEC 1000000000ULL

#define BUTTERFILT_ORD 2
#define FILTER_LENGTH 23

//Mi spiega come usare il programma in caso di una linea di comando sbagliata
#define USAGE_STR				\
	"Usage: %s [-s] [-n] [-f]\n"		\
	"\t -s: plot original signal\n"		\
	"\t -n: plot noisy signal\n"		\
	"\t -f: plot filtered signal\n"		\
	""

//Contiene tutte le funzioni di utilità e la struct del periodic thread

/* periodic thread */
typedef struct{
	int index;
	struct timespec r;
	int period;
	int wcet;
	int priority;
} periodic_thread;

/* time utility functions */

void timespec_add_us(struct timespec *t, unsigned long d);

unsigned long int difference_ns(struct timespec *ts1, struct timespec *ts2);

/* return 1 if t1>t2, 0 otherwise*/
int compare_time(struct timespec *t1,struct timespec *t2);

void busy_sleep(int us);	//Mette il task in sleep consumando la CPU

/* periodic threads functions */
void wait_next_activation(periodic_thread * thd);

void start_periodic_timer(periodic_thread * thd, unsigned long offs);

double get_butter(double cur, double * a, double * b);

double get_mean_filter(double cur);

double chebyshevFilter(double cur);

double medianFilter(double curr);

//void parse_cmdline(int argc, char ** argv);



#endif
