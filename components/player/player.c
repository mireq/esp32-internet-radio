#include <string.h>
#include <strings.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "audio_output.h"
#include "events.h"
#include "player.h"
#include "source.h"


#define PLAY_RETRY_TIMEOUT 1000


static const char *TAG = "player";


typedef enum playback_command_t {
	NO_COMMAND,
	STOP_COMMAND,
	QUIT_COMMAND,
} playback_command_t;


char uri[MAX_URI_SIZE];
audio_sample_t audio_buffer[AUDIO_OUTPUT_BUFFER_SIZE << 1];
playback_command_t playback_command = NO_COMMAND;
SemaphoreHandle_t source_changed_semaphore = NULL;
SemaphoreHandle_t playback_stopped_semaphore = NULL;
SemaphoreHandle_t wait_data_semaphore = NULL;

source_t source;

audio_output_t audio_output = {
#ifndef SIMULATOR
	.port = AUDIO_I2S_PORT,
#else
	.handle = NULL,
#endif
	.sample_rate = 44100,
};


static void on_metadata(struct source_t *source, const char *key, const char *value) {
	ESP_LOGI(TAG, "Metadata: %s = %s", key, value);
}


void handle_playback(source_t *source) {
	if (strcmp(source->content_type, "audio/mpeg")) {
		ESP_LOGE(TAG, "wrong content-type, excepted audio/mpeg, got %s", source->content_type);
		esp_event_post_to(player_event_loop, PLAYBACK_EVENT, PLAYBACK_EVENT_ERROR, NULL, 0, portMAX_DELAY);
		return;
	}

	char buf[64];
	for (;;) {
		ssize_t size = source_read(source, buf, sizeof(buf));
		if (size == 0) {
			xSemaphoreTake(wait_data_semaphore, 1);
		}
		else if (size == SOURCE_READ_AGAIN) {
			continue;
		}
		else if (size < 0 || playback_command == STOP_COMMAND) {
			break;
		}
		else {
			audio_output_write(&audio_output, (audio_sample_t *)buf, size / 8);
		}
	}
}


void player_loop(void *parameters) {
	bzero(uri, sizeof(uri));
	source_changed_semaphore = xSemaphoreCreateBinary();
	playback_stopped_semaphore = xSemaphoreCreateBinary();
	wait_data_semaphore = xSemaphoreCreateBinary();
	xSemaphoreGive(playback_stopped_semaphore);

	ESP_LOGI(TAG, "player loop initialized");

	for (;;) {
		xSemaphoreTake(source_changed_semaphore, portMAX_DELAY);
		playback_command = NO_COMMAND;
		if (strlen(uri) > 0) {
			source_error_t status = source_init(&source, uri);
			if (status == SOURCE_NO_ERROR) {
				ESP_LOGI(TAG, "start playback");
				source.metadata_callback = on_metadata;
				handle_playback(&source);
				ESP_LOGI(TAG, "stop playback");
			}
			else {
				esp_event_post_to(player_event_loop, PLAYBACK_EVENT, PLAYBACK_EVENT_ERROR, NULL, 0, portMAX_DELAY);
			}
			source_destroy(&source);
		}
		xSemaphoreGive(playback_stopped_semaphore);
		if (playback_command == QUIT_COMMAND) {
			break;
		}
	}

	vTaskDelete(NULL);
}


void start_playback(void) {
	stop_playback();
	strcpy(uri, "http://ice1.somafm.com/illstreet-128-mp3");
	xSemaphoreGive(source_changed_semaphore);
}


void stop_playback(void) {
	playback_command = STOP_COMMAND;
	bzero(uri, sizeof(uri));
	xSemaphoreGive(wait_data_semaphore);
	xSemaphoreGive(source_changed_semaphore);
	xSemaphoreTake(playback_stopped_semaphore, portMAX_DELAY);

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
	if (xTaskCreatePinnedToCore(&player_loop, "player", 8192, NULL, 6, NULL, 0) != pdPASS) {
		ESP_LOGE(TAG, "Player task not initialized");
	}
}


void init_audio_output(void) {
	audio_output_init(&audio_output);
}
