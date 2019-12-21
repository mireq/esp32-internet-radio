#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include "events.h"


#ifndef SIMULATOR
static const char *NET = "network";
extern esp_netif_t *network_interface;
#endif


void init_events(void) {
	ESP_ERROR_CHECK(esp_event_loop_create_default());
}


void network_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
#ifndef SIMULATOR
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		// retry
		esp_wifi_connect();
		ESP_LOGW(NET, "not connected to AP, retry.");
	}
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(NET, "connected with ip address:" IPSTR, IP2STR(&event->ip_info.ip));
	}
#endif
}
