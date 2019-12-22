#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "events.h"
#include "player.h"
#include "source.h"


#define PLAY_RETRY_TIMEOUT 1000


static const char *TAG = "player";
SemaphoreHandle_t play_semaphore = NULL;


bool source_initialized = false;
source_config_t callbacks = {
	.on_data = NULL,
	.on_status = NULL,
};
source_t source = {
};


void start_playback(void) {
	ESP_LOGI(TAG, "start playback");
	stop_playback();
	xSemaphoreTake(play_semaphore, portMAX_DELAY);
	source_error_t status = source_init(&source, "http://ice1.somafm.com/illstreet-128-mp3", callbacks);
	source_initialized = true;
	xSemaphoreGive(play_semaphore);
	printf("%d\n", status);
	if (status != SOURCE_NO_ERROR) {
		esp_event_post_to(player_event_loop, PLAYBACK_EVENT, PLAYBACK_EVENT_ERROR, NULL, 0, portMAX_DELAY);
	}
}


void stop_playback(void) {
	xSemaphoreTake(play_semaphore, portMAX_DELAY);
	if (!source_initialized) {
		xSemaphoreGive(play_semaphore);
		return;
	}
	ESP_LOGI(TAG, "stop playback");
	source_destroy(&source);
	ESP_LOGI(TAG, "stopped");
	source_initialized = false;
	xSemaphoreGive(play_semaphore);
}


static void on_network_connect(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	start_playback();
}


static void on_network_disconnect(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	stop_playback();
}


static void on_playback_event_error(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	stop_playback();
	vTaskDelay(PLAY_RETRY_TIMEOUT / portTICK_PERIOD_MS);
	start_playback();
}


void init_player_events(void) {
	esp_event_handler_register_with(player_event_loop, NETWORK_EVENT, NETWORK_EVENT_CONNECT, &on_network_connect, NULL);
	esp_event_handler_register_with(player_event_loop, NETWORK_EVENT, NETWORK_EVENT_DISCONNECT, &on_network_disconnect, NULL);
	esp_event_handler_register_with(player_event_loop, PLAYBACK_EVENT, PLAYBACK_EVENT_ERROR, &on_playback_event_error, NULL);
}


void init_player(void) {
	play_semaphore = xSemaphoreCreateMutex();
}
