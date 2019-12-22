#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "events.h"
#include "player.h"
#include "source.h"


static const char *TAG = "player";
SemaphoreHandle_t play_semaphore = NULL;
SemaphoreHandle_t stop_semaphore = NULL;
SemaphoreHandle_t destroy_semaphore = NULL;


void player_task(void *data) {
	ESP_LOGI(TAG, "player task");
	for (;;) {
		xSemaphoreTake(play_semaphore, portMAX_DELAY);
		printf("initialize\n");
		xSemaphoreTake(stop_semaphore, portMAX_DELAY);
		printf("destroy\n");
		xSemaphoreGive(destroy_semaphore);
	}
}


void start_playback(void) {
	//stop_playback();
	ESP_LOGI(TAG, "start playback");
	xSemaphoreGive(play_semaphore);
}


void stop_playback(void) {
	/*
	xSemaphoreTake(playback_handle_semaphore, portMAX_DELAY);
	if (player_task_handle == NULL) {
		xSemaphoreGive(playback_handle_semaphore);
		return;
	}
	*/
	ESP_LOGI(TAG, "stop playback");
	xSemaphoreGive(stop_semaphore);
	xSemaphoreTake(destroy_semaphore, portMAX_DELAY);
	ESP_LOGI(TAG, "stopped");
}


static void on_network_connect(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	start_playback();
	//source_t source;
	//if (source_init(&source, "http://ice1.somafm.com/illstreet-128-mp3", callbacks) != SOURCE_NO_ERROR) {
	//	return;
	//}
}


static void on_network_disconnect(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	stop_playback();
}


static void on_playback_event_error(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	stop_playback();
}


void init_player_events(void) {
	esp_event_handler_register_with(player_event_loop, NETWORK_EVENT, NETWORK_EVENT_CONNECT, &on_network_connect, NULL);
	esp_event_handler_register_with(player_event_loop, NETWORK_EVENT, NETWORK_EVENT_DISCONNECT, &on_network_disconnect, NULL);
	esp_event_handler_register_with(player_event_loop, PLAYBACK_EVENT, PLAYBACK_EVENT_ERROR, &on_playback_event_error, NULL);
}


void init_player(void) {
	play_semaphore = xSemaphoreCreateBinary();
	stop_semaphore = xSemaphoreCreateBinary();
	destroy_semaphore = xSemaphoreCreateBinary();
	ESP_LOGI(TAG, "initialized semaphores");
	xTaskCreate(&player_task, "playback", 1024, NULL, 5, NULL);
}
