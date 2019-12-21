#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <unistd.h>


void app_main();
static void run_main(void *data);
static void exit_app(void *data);


int main(void) {
	xTaskCreate(&run_main, "run_main", 128, NULL, 5, NULL);
	vTaskStartScheduler();
	return 0;
}


void vApplicationIdleHook(void) {
	sleep(1);
}


void vMainQueueSendPassed(void) {
}


static void exit_app(void *data) {
	vTaskEndScheduler();
}


static void run_main(void *data) {
	app_main();
	vTaskDelete(NULL);
}
