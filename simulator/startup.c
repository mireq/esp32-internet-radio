#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


void app_main();


int main(void) {
	xTaskCreate(&app_main, "app_main", 2048, NULL, 5, NULL);
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
