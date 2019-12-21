#pragma once

#include "esp_event.h"


ESP_EVENT_DECLARE_BASE(NETWORK_EVENT);


typedef enum {
	NETWORK_EVENT_DISCONNECT,
	NETWORK_EVENT_CONNECT,
} network_event_t;


extern esp_event_loop_handle_t player_event_loop;
