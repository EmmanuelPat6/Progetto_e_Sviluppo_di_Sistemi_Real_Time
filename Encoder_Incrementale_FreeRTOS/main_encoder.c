#include <stdio.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"


/* Periodo di Base */
#define PERIOD_MS 5
//#define PERIOD_MS_10 50	//Essendo TickType_t un int avrei con 5/2 = 2.5 -> 2
							//Posso utilizzare 50 e configurare un TICK_RATE pari a 10000
/* Priorità dei task */
#define	ENCODER_TASK_PRIORITY		( tskIDLE_PRIORITY + 2 )
#define	TASK_1_PRIORITY		( tskIDLE_PRIORITY + 3 )
#define	TASK_2_PRIORITY		( tskIDLE_PRIORITY + 3 )
#define PRINT_TASK_PRIORITY		( tskIDLE_PRIORITY  ) 
#define DIAGNOSTIC_TASK_PRIORITY	( tskIDLE_PRIORITY + 1 )


/* Encoder */
struct enc_str {
    unsigned int slit;          // Valori oscillanti tra 0 e 1
    unsigned int home_slit;     // 1 se in home, 0 altrimenti
    SemaphoreHandle_t xSemaphore;
};
static struct enc_str enc_data;

/* Rising Edge Structure */
struct _rising_edge {
    unsigned int count;
    SemaphoreHandle_t xSemaphore;
};
static struct _rising_edge rising_edge;

/* Round Time Structure */
struct _round_time {
    unsigned long int tick_diff;
    SemaphoreHandle_t xSemaphore;
};
static struct _round_time round_time;

/* Slack Structures */
struct _slack_rt1{
	unsigned long int slack_time;
	SemaphoreHandle_t xSemaphore;
};
static struct _slack_rt1 slack_rt1;

struct _slack_rt2{
	unsigned long int slack_time;
	SemaphoreHandle_t xSemaphore;
};
static struct _slack_rt2 slack_rt2;


/*-----------------------------------------------------------*/

/* Definizione dei task da implementare */
static void encoderTask(void* pvParameters);
static void rtTask1(void* pvParameters);
static void rtTask2(void* pvParameters);
static void printTask(void* pvParameters);
static void diagnosticTask(void* pvParameters);

/*-----------------------------------------------------------*/

void main_encoder(void)
{
	console_print("Main!\n\n\n");

	/* Creazione Mutex */
    enc_data.xSemaphore = xSemaphoreCreateMutex();
    rising_edge.xSemaphore = xSemaphoreCreateMutex();
    round_time.xSemaphore = xSemaphoreCreateMutex();
	slack_rt1.xSemaphore = xSemaphoreCreateMutex();
	slack_rt2.xSemaphore = xSemaphoreCreateMutex();

	/* Controllo Creazione Semafori*/
	if(enc_data.xSemaphore != NULL && rising_edge.xSemaphore != NULL && round_time.xSemaphore != NULL && slack_rt1.xSemaphore != NULL && slack_rt2.xSemaphore != NULL )
	{
		console_print("Semaphores Created\n\n");

		/* Creazione Task*/
		xTaskCreate(encoderTask, "encoderTask", configMINIMAL_STACK_SIZE, NULL, ENCODER_TASK_PRIORITY, NULL);
		console_print("Encoder Task created\n");

		xTaskCreate(rtTask1, "rtTask1", configMINIMAL_STACK_SIZE, NULL, TASK_1_PRIORITY, NULL);
		console_print("RT_Task1 created\n");

		xTaskCreate(rtTask2, "rtTask2", configMINIMAL_STACK_SIZE, NULL, TASK_2_PRIORITY, NULL);
		console_print("RT_Task2 created\n");

		xTaskCreate(printTask, "printTask", configMINIMAL_STACK_SIZE, NULL, PRINT_TASK_PRIORITY, NULL);
		console_print("Print Task created\n");

		xTaskCreate(diagnosticTask, "diagnosticTask", configMINIMAL_STACK_SIZE, NULL, DIAGNOSTIC_TASK_PRIORITY, NULL);
		console_print("Diagnostic Task created\n");
	}

		/* Start */
		vTaskStartScheduler();
	/* Ciclo Infinito */
	for (;;);
}


/***********************Encoder Task*************************/
static void encoderTask(void *pvParameters) {

    TickType_t xNextWakeTime;
	//uint32_t index = (int)pvParameters;
	const TickType_t xBlockTime = pdMS_TO_TICKS(PERIOD_MS);
	//const TickType_t xBlockTime = pdMS_TO_TICKS(PERIOD_MS)/10;

	/* Inizializzazione xNextWakeTime al TICK corrente*/
	xNextWakeTime = xTaskGetTickCount();

	console_print("Start Encoder\n");

	/* Acquire Mutex */
	xSemaphoreTake(enc_data.xSemaphore, portMAX_DELAY);
	enc_data.slit = 0;
	enc_data.home_slit = 0;

	/* Release Mutex */
	xSemaphoreGive(enc_data.xSemaphore);

	unsigned int count = 0;
	unsigned int slit_count = 0;		
	unsigned int prev_slit = 0;

	/* Randomized period (75-750 RPM) */
	srand(time(NULL));
	unsigned int semi_per = (rand() % 10) + 1;	
	//semi_per = 5;								//DEBUG

	for (;;)
	{
		vTaskDelayUntil(&xNextWakeTime, xBlockTime);

		/* Acquire Mutex */
		xSemaphoreTake(enc_data.xSemaphore, portMAX_DELAY);
		
		prev_slit = enc_data.slit;
		if (count%semi_per == 0) {
			enc_data.slit++;
			enc_data.slit%=2;
		}

		if (prev_slit==0&&enc_data.slit==1) 					//Fronte di salita
			slit_count=(++slit_count)%8;

		if (slit_count==0) enc_data.home_slit=enc_data.slit;
		else enc_data.home_slit=0;

		//console_print("%d:\t\t %d %d\n",count,enc_data.slit,enc_data.home_slit);	//DEBUG encoder
		count++;

		/* Release Mutex */
		xSemaphoreGive(enc_data.xSemaphore);
	}
}

/************************rtTask1*******************************/
static void rtTask1(void* pvParameters){

    TickType_t xNextWakeTime;
//	uint32_t index = (int)pvParameters;
	const TickType_t xBlockTime = pdMS_TO_TICKS(PERIOD_MS/2);
	//const TickType_t xBlockTime = pdMS_TO_TICKS(PERIOD_MS/2)/10;

	TickType_t finish_tick;

	/* Inizializzazione xNextWakeTime al TICK corrente*/
	xNextWakeTime = xTaskGetTickCount();

	console_print("Start rtTask1\n");

	/* Acquire Mutex */
	xSemaphoreTake(rising_edge.xSemaphore, portMAX_DELAY);
	rising_edge.count = 0;

	/* Release Mutex */
	xSemaphoreGive(rising_edge.xSemaphore);

	int last_value=0;

	for (;;)
	{
		vTaskDelayUntil(&xNextWakeTime, xBlockTime);

		/* Acquire Mutex */
		xSemaphoreTake(enc_data.xSemaphore, portMAX_DELAY);
		if( last_value == 0 && enc_data.slit == 1){
			last_value = 1;

			/* Acquire Mutex */
			xSemaphoreTake(rising_edge.xSemaphore, portMAX_DELAY);
			rising_edge.count++;

			/* Release Mutex */
			xSemaphoreGive(rising_edge.xSemaphore);		
		}

		else if(last_value == 1 && enc_data.slit == 0){
			last_value = 0;
		}

		/* Release Mutex */
		xSemaphoreGive(enc_data.xSemaphore);

		/* Slack Time */
		finish_tick = xTaskGetTickCount();
		if((xNextWakeTime+xBlockTime) > finish_tick){

			/* Acquire Mutex */
			xSemaphoreTake(slack_rt1.xSemaphore, portMAX_DELAY);
			slack_rt1.slack_time = xNextWakeTime + xBlockTime - finish_tick;

			/* Release Mutex */
			xSemaphoreGive(slack_rt1.xSemaphore);
		}
		else{
			console_print("DEADLINE MISS  finish tick:%ld ms\t  deadline:%ld ms\n", finish_tick, (xNextWakeTime + xBlockTime));
		}

    }
}

/*************************rtTask2******************************/
static void rtTask2(void* pvParameters){

    TickType_t xNextWakeTime;
//	uint32_t index = (int)pvParameters;
	const TickType_t xBlockTime = pdMS_TO_TICKS(PERIOD_MS/2);
	//const TickType_t xBlockTime = pdMS_TO_TICKS(PERIOD_MS/2)/10;

	TickType_t finish_tick;

	/* Inizializzazione xNextWakeTime al TICK corrente*/
	xNextWakeTime = xTaskGetTickCount();

	console_print("Start rtTask2\n");
	int first_measure = 1;
	int last_home_slit = 0;

	/* Acquire Mutex */
	xSemaphoreTake(rising_edge.xSemaphore, portMAX_DELAY);
	rising_edge.count = 0;

	/* Release Mutex */
	xSemaphoreGive(rising_edge.xSemaphore);

	int last_value=0;

    TickType_t tick_home, last_tick_home;

	for (;;)
	{
		vTaskDelayUntil(&xNextWakeTime, xBlockTime);

		/* Acquire Mutex */
		xSemaphoreTake(enc_data.xSemaphore, portMAX_DELAY);
		
		if(enc_data.home_slit == 1 && last_home_slit == 0){
			last_home_slit = 1;
			if(first_measure){
				last_tick_home = xTaskGetTickCount();
				first_measure = 0;
			}
			else{
				tick_home = xTaskGetTickCount();
	
				/* Acquire Mutex */
				xSemaphoreTake(round_time.xSemaphore, portMAX_DELAY);
				round_time.tick_diff = tick_home - last_tick_home;

				/* Release Mutex */
				xSemaphoreGive(round_time.xSemaphore);

				last_tick_home = tick_home;	
			}
		}
		else if(enc_data.home_slit == 0){
			last_home_slit = 0;
		}
		
		/* Release Mutex */
		xSemaphoreGive(enc_data.xSemaphore);



		/* Slack Time */
		finish_tick = xTaskGetTickCount();
		
		if((xNextWakeTime + xBlockTime) > finish_tick){

			/* Acquire Mutex */
			xSemaphoreTake(slack_rt2.xSemaphore, portMAX_DELAY);
			slack_rt2.slack_time = xNextWakeTime + xBlockTime - finish_tick;

			/* Release Mutex */
			xSemaphoreGive(slack_rt2.xSemaphore);
		}
		else{
			console_print("DEADLINE MISS  finish tick:%ld ms\t  deadline:%ld ms\n", finish_tick, (xNextWakeTime + xBlockTime));
		}
	
	}	
}		

/*************************Print Task****************************/
static void printTask(void *pvParameters){

	TickType_t xNextWakeTime;
	//uint32_t index = *(uint32_t*)pvParameters;

	console_print("Start Print Task\n");
	const TickType_t xBlockTime = pdMS_TO_TICKS(PERIOD_MS*2);
	//const TickType_t xBlockTime = pdMS_TO_TICKS(PERIOD_MS*2)/10;

	unsigned int count=0;
	float diff_tick = 0;
	unsigned int rpm = 0;
	
	/* Inizializzazione xNextWakeTime al TICK corrente*/
	xNextWakeTime = xTaskGetTickCount();

	console_print("Start Print Task\n");

	for (;;)
	{
		vTaskDelayUntil(&xNextWakeTime, xBlockTime);
		
		/* Acquire Mutex */
		xSemaphoreTake(rising_edge.xSemaphore, portMAX_DELAY);
		count = rising_edge.count;

		/* Release Mutex */
		xSemaphoreGive(rising_edge.xSemaphore);
		
		console_print("Rising Edge Counter : %d\t",count);
		
		/* Acquire Mutex */
		xSemaphoreTake(round_time.xSemaphore, portMAX_DELAY);
		diff_tick = round_time.tick_diff;

		/* Release Mutex */
		xSemaphoreGive(round_time.xSemaphore);
		
		rpm = (unsigned int)(60*1000/diff_tick);	//*10000
		
		//console_print("diff : %f\t",diff_tick);				//DEBUG
		console_print("RPM : %u\n",rpm);	
	}
}

/*********************Diagnostic Task************************/
static void diagnosticTask(void* pvParameters){
	
	TickType_t xNextWakeTime;
	//uint32_t index = *(uint32_t*)pvParameters;
	console_print("Start Diagnostic Task\n");

	const TickType_t xBlockTime = pdMS_TO_TICKS(PERIOD_MS/2);
	//const TickType_t xBlockTime = pdMS_TO_TICKS(PERIOD_MS/2)/10;

	unsigned long int avg_slack=0;	//Lo metto double nel secondo caso (con TICK_RATE = 10000)
	int i = 0;
	int rounds = 100;

	/* Inizializzazione xNextWakeTime al TICK corrente*/
	xNextWakeTime = xTaskGetTickCount();

	for (;;)
	{
		vTaskDelayUntil(&xNextWakeTime, xBlockTime);
		
		/* Acquire Mutex */
		xSemaphoreTake(slack_rt1.xSemaphore, portMAX_DELAY);
		xSemaphoreTake(slack_rt2.xSemaphore, portMAX_DELAY);

		avg_slack += (slack_rt1.slack_time + slack_rt2.slack_time)/2;	//	/20

		/* Release Mutex */
		xSemaphoreGive(slack_rt1.xSemaphore);
		xSemaphoreGive(slack_rt2.xSemaphore);

		i++;
		if(i == rounds){
			avg_slack = avg_slack/rounds;
			console_print("**********SLACK TIME: %ld ms**********\n",avg_slack);	//%lf
			i = 0;
		}
		
	}
}
