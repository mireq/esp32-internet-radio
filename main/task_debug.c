#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sdkconfig.h"
#include "task_debug.h"


#define MAXIMUM_TASKS 16


#if CONFIG_DEBUG_PRINT_TASK_STATISTICS == 1


static void print_task_stats(void) {
	TaskStatus_t task_status_array[MAXIMUM_TASKS];
	uint32_t total_run_time;
	UBaseType_t number_of_tasks = uxTaskGetSystemState(&task_status_array[0], MAXIMUM_TASKS, &total_run_time);
	printf("%-16s %-3s %6s %10s\n", "Name", "Pri", "Run", "Stack free");
	printf("================ === ====== ==========\n");
	for (size_t i = 0; i < number_of_tasks; ++i) {
		TaskStatus_t *status = &task_status_array[i];
		double runtimeTime = 0;
		if ((total_run_time / 100) > 0) {
			runtimeTime = (double)status->ulRunTimeCounter / (double)(total_run_time / 100);
		}
		printf("%-16s %3d %6.2f %10d\n", status->pcTaskName, (int)status->uxCurrentPriority, runtimeTime, (int)status->usStackHighWaterMark);
	}
	printf("\n");
}


void task_stats_task(void *arg) {
	for (;;) {
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		print_task_stats();
	}
	vTaskDelete(NULL);
}

#endif
