#define SIG_HZ 1
#define F_SAMPLE 50
#define NSEC_PER_SEC 1000000000ULL

/* Periodi Threads*/
#define TICK_PERIOD 20000
#define PEAK_PERIOD 1000000
#define STORE_PERIOD 200000

/* Queue Name */
#define PRINT_QUEUE_NAME_1   "/print_queue_1"
#define PRINT_QUEUE_NAME_2   "/print_queue_2"
#define PRINT_QUEUE_NAME_3  "/print_queue_3"
#define PEAK_QUEUE_NAME "/peak_queue"
#define CONF_PEAK_QUEUE_NAME "/conf_peak_queue"
#define CONF_STORE_QUEUE_NAME "/conf_store_queue"

#define QUEUE_PERMISSIONS 0660

//Massimo numero di messaggi e dimensione di essi
#define MAX_MESSAGES 10
#define MAX_MSG_SIZE 256

#define BUF_SIZE 50

#define BUTTERFILT_ORD 2
#define FILTER_LENGTH 23

#define OUTFILE "signal.txt"
