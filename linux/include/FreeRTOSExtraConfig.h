#pragma once

#include "FreeRTOS.h"
#include "freertos/portmacro.h"


void trcKERNEL_HOOKS_TASK_CREATE(UBaseType_t task_number, const char *task_name);
void trcKERNEL_HOOKS_TASK_SWITCH(UBaseType_t task_number);


#undef traceTASK_CREATE
#define traceTASK_CREATE(pxNewTCB) \
	vPortAddTaskHandle(pxNewTCB); \
	trcKERNEL_HOOKS_TASK_CREATE(pxNewTCB->uxTCBNumber, pxNewTCB->pcTaskName);


#undef traceTASK_SWITCHED_IN
#define traceTASK_SWITCHED_IN() \
	trcKERNEL_HOOKS_TASK_SWITCH(pxCurrentTCB->uxTCBNumber);
