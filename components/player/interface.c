#include "interface.h"


ESP_EVENT_DEFINE_BASE(NETWORK_EVENT);
ESP_EVENT_DEFINE_BASE(PLAYBACK_EVENT);
ESP_EVENT_DEFINE_BASE(CONTROL_COMMAND);


esp_event_loop_handle_t player_event_loop;
