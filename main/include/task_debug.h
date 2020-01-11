#pragma once

#include "sdkconfig.h"

#if CONFIG_DEBUG_PRINT_TASK_STATISTICS == 1

void task_stats_task(void *arg);

#endif
