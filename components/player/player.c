#include <strings.h>

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
source_t source;
TaskHandle_t player_task = NULL;


void player_loop(void *parameters) {
	char buf[64];
	bzero(buf, sizeof(buf));

	for (;;) {
		ssize_t size = source_read(&source, buf, sizeof(buf));
		if (size == 0) {
			vTaskDelay(1);
		}
		//ssize_t size = source_read(&source, buf, sizeof(buf) - 1);
		//if (size > 0) {
		//	printf("%d\n", size);
		//}
	}

	esp_event_post_to(player_event_loop, PLAYBACK_EVENT, PLAYBACK_EVENT_FINISHED, NULL, 0, portMAX_DELAY);
	vTaskDelete(NULL);
	player_task = NULL;
}


void start_playback(void) {
	ESP_LOGI(TAG, "start playback");
	stop_playback();
	xSemaphoreTake(play_semaphore, portMAX_DELAY);
	source_error_t status = source_init(&source, "http://ice1.somafm.com/illstreet-128-mp3");
	source_initialized = true;
	if (status != SOURCE_NO_ERROR) {
		xSemaphoreGive(play_semaphore);
		esp_event_post_to(player_event_loop, PLAYBACK_EVENT, PLAYBACK_EVENT_ERROR, NULL, 0, portMAX_DELAY);
	}
	if (xTaskCreatePinnedToCore(&player_loop, "player", 2048, &source, 6, &player_task, 0) != pdPASS) {
		player_task = NULL;
	}
	xSemaphoreGive(play_semaphore);
}


void stop_playback(void) {
	xSemaphoreTake(play_semaphore, portMAX_DELAY);
	if (!source_initialized) {
		xSemaphoreGive(play_semaphore);
		return;
	}
	ESP_LOGI(TAG, "stop playback");
	source_destroy(&source);
	source_initialized = false;
	if (player_task != NULL) {
		vTaskDelete(player_task);
		player_task = NULL;
	}
	ESP_LOGI(TAG, "stopped");
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
