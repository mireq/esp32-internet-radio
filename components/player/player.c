#include <string.h>
#include <strings.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "audio_output.h"
#include "buffer/buffer.h"
#include "decoder.h"
#include "events.h"
#include "player.h"
#include "source.h"
#include "terminal.h"


static const char *TAG = "player";


typedef enum playback_command_t {
	NO_COMMAND,
	STOP_COMMAND,
	QUIT_COMMAND,
} playback_command_t;


static char uri[MAX_URI_SIZE];
static playback_command_t playback_command = NO_COMMAND;
static SemaphoreHandle_t source_changed_semaphore = NULL;
static SemaphoreHandle_t playback_stopped_semaphore = NULL;
static SemaphoreHandle_t wait_data_semaphore = NULL;

static source_t source;
static decoder_t decoder;

static audio_output_t audio_output = {
#ifdef SIMULATOR
#else
	.port = AUDIO_I2S_PORT,
#endif
	.sample_rate = 44100,
};

static buffer_t network_buffer = {
	.size = 0,
	.r_pos = 0,
	.w_pos = 0,
};


static void on_metadata(source_t *source, const char *key, const char *value) {
	ESP_LOGI(TAG, "Metadata: %s = %s", key, value);
}


static void on_decoder_event(decoder_t *decoder, decoder_event_type_t event_type, void *data) {
	if (event_type == DECODER_PCM) {
		decoder_pcm_data_t *pcm_data = (decoder_pcm_data_t *)data;
		audio_output_write(&audio_output, pcm_data->data, pcm_data->length);
	}
}


static esp_err_t process_data(source_t *source, buffer_t *network_buffer) {
	if (strcmp(source->content_type, "audio/mpeg")) {
		ESP_LOGE(TAG, "wrong content-type, excepted audio/mpeg, got %s", source->content_type);
		esp_event_post_to(player_event_loop, PLAYBACK_EVENT, PLAYBACK_EVENT_ERROR, NULL, 0, portMAX_DELAY);
		return ESP_FAIL;
	}

	esp_err_t status = ESP_OK;
	static char source_read_buffer[SOURCE_READ_BUFFER_SIZE];
	decoder.callback = on_decoder_event;
	status = decoder_init(&decoder, source);

	if (status == ESP_OK) {
		for (;;) {
			ssize_t size = source_read(source, source_read_buffer, sizeof(source_read_buffer));
			if (size == 0) {
				xSemaphoreTake(wait_data_semaphore, 1);
			}
			else if (size == SOURCE_READ_AGAIN) {
				continue;
			}
			else if (size < 0) {
				status = ESP_FAIL;
				break;
			}
			else if (playback_command == STOP_COMMAND || playback_command == QUIT_COMMAND) {
				break;
			}
			else if (size > 0) {
				buffer_write(network_buffer, source_read_buffer, size);
			}
		}
	}

	decoder_destroy(&decoder);
	return ESP_OK;
}


static void player_loop(void *parameters) {
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
				esp_err_t status = process_data(&source, &network_buffer);
				buffer_clear(&network_buffer);
				ESP_LOGI(TAG, "stop playback");
				if (status != ESP_OK) {
					esp_event_post_to(player_event_loop, PLAYBACK_EVENT, PLAYBACK_EVENT_ERROR, NULL, 0, portMAX_DELAY);
				}
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


static void decoder_loop(void *parameters) {
	ESP_LOGI(TAG, "audio loop");
	static char audio_process_buffer[AUDIO_PROCESS_BUFFER_SIZE];

	char *stream_buffer = heap_caps_malloc(STREAM_BUFFER_SIZE, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
	if (!stream_buffer) {
		ESP_LOGE(TAG, "stream buffer not allocated");
		vTaskDelete(NULL);
		return;
	}
	network_buffer.buf = stream_buffer;
	network_buffer.size = STREAM_BUFFER_SIZE;
	buffer_init(&network_buffer);

	for (;;) {
		if (buffer_read(&network_buffer, audio_process_buffer, sizeof(audio_process_buffer)) == ESP_OK) {
			if (decoder_feed(&decoder, audio_process_buffer, sizeof(audio_process_buffer)) != ESP_OK) {
				ESP_LOGE(TAG, "decoder error");
				esp_event_post_to(player_event_loop, PLAYBACK_EVENT, PLAYBACK_EVENT_ERROR, NULL, 0, portMAX_DELAY);
			}
		}
	}

	buffer_destroy(&network_buffer);
	heap_caps_free(stream_buffer);
	vTaskDelete(NULL);
}


void start_playback(void) {
	stop_playback();
	//strcpy(uri, "http://ice1.somafm.com/illstreet-128-mp3");
	strcpy(uri, "http://icecast.stv.livebox.sk:80/fm_128.mp3");
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


static void player_status_loop(void *parameters) {
	while (1) {
		printf(TERM_ERASE_LINE "\rb: %05d | %05d | %05d\r", (int)network_buffer.r_pos, (int)network_buffer.w_pos, (int)buffer_get_full(&network_buffer));
		fflush(stdout);
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}


void init_player(void) {
	if (xTaskCreatePinnedToCore(&decoder_loop, "decoder", 8192, NULL, 5, NULL, 0) != pdPASS) {
		ESP_LOGE(TAG, "Decoder loop not itialized");
	}
	if (xTaskCreatePinnedToCore(&player_loop, "player", 8192, NULL, 5, NULL, 0) != pdPASS) {
		ESP_LOGE(TAG, "Player task not initialized");
	}
	if (xTaskCreatePinnedToCore(&player_status_loop, "player_status", 1024, NULL, 4, NULL, 0) != pdPASS) {
		ESP_LOGE(TAG, "Player status task not initialized");
	}
}


void init_audio_output(void) {
	audio_output_init(&audio_output);
}
