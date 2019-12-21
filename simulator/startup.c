#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


void app_main();
static void run_main();
static void exit_app();


int main(void) {
	xTaskCreate(&run_main, "run_main", 2048, NULL, 5, NULL);
	vTaskStartScheduler();
	return 0;
}


void vApplicationIdleHook(void) {
#ifdef __GCC_POSIX__
	struct timespec xTimeToSleep, xTimeSlept;
	xTimeToSleep.tv_sec = 1;
	xTimeToSleep.tv_nsec = 0;
	nanosleep(&xTimeToSleep, &xTimeSlept);
#endif
}


void vMainQueueSendPassed(void) {
}


static void exit_app() {
	vTaskEndScheduler();
}


static void run_main() {
	app_main();
	exit_app();
}
