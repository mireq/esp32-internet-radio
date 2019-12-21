#pragma once

#include "esp_err.h"

esp_err_t esp_event_loop_create_default(void);
typedef const char *esp_event_base_t;
