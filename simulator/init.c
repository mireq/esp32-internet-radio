#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "init.h"
#include "interface.h"


void init_nvs(void) {
}


static void fake_network_init(void *data) {
	vTaskDelay(100 / portTICK_PERIOD_MS);
	esp_event_post_to(player_event_loop, NETWORK_EVENT, NETWORK_EVENT_CONNECT, NULL, 0, portMAX_DELAY);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	esp_event_post_to(player_event_loop, NETWORK_EVENT, NETWORK_EVENT_DISCONNECT, NULL, 0, portMAX_DELAY);
	vTaskDelay(100 / portTICK_PERIOD_MS);
	esp_event_post_to(player_event_loop, NETWORK_EVENT, NETWORK_EVENT_CONNECT, NULL, 0, portMAX_DELAY);
	vTaskDelete(NULL);
}


void init_network(void) {
	xTaskCreate(&fake_network_init, "fake_network_init", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}
