#include "esp_log.h"

#include "events.h"
#include "player.h"
#include "source.h"


static const char *TAG = "player";


static void on_network_connect(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	//source_t source;
	//if (source_init(&source, "http://ice1.somafm.com/illstreet-128-mp3", callbacks) != SOURCE_NO_ERROR) {
	//	return;
	//}
}


static void on_network_disconnect(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	ESP_LOGI(TAG, " === disconnected ===");
}


void init_player_events(void) {
	esp_event_handler_register_with(player_event_loop, NETWORK_EVENT, NETWORK_EVENT_CONNECT, &on_network_connect, NULL);
	esp_event_handler_register_with(player_event_loop, NETWORK_EVENT, NETWORK_EVENT_DISCONNECT, &on_network_disconnect, NULL);
}
