#include <string.h>
#include <strings.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "audio_output.h"
#include "buffer/buffer.h"
#include "control_http.h"
#include "decoder.h"
#include "interface.h"
#include "player.h"
#include "playlist.h"
#include "source.h"
#include "terminal.h"


static const char *TAG = "player";


/* Variables */


playlist_t playlist = { .callback = NULL };
player_state_t player_state = { .volume = 0x1000 };
static source_t source = { .type = SOURCE_TYPE_UNKNOWN, .semaphore = NULL };
static decoder_t decoder;
static SemaphoreHandle_t has_source_semaphore = NULL;
static SemaphoreHandle_t source_ready_semaphore = NULL;
static buffer_t network_buffer = { .size = 0, .r_pos = 0, .w_pos = 0 };
static char audio_process_buffer[AUDIO_PROCESS_BUFFER_SIZE];
static audio_output_t audio_output = {
#ifndef SIMULATOR
	.port = AUDIO_I2S_PORT,
#endif
	.sample_rate = 44100,
};

typedef struct player_stats_t {
	uint64_t network_readed_bytes;
	uint64_t network_readed_bytes_history[10];
} player_stats_t;

static player_stats_t player_stats = {
	.network_readed_bytes = 0,
	.network_readed_bytes_history = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};


/* Events */


static void on_metadata(source_t *source, const char *key, const char *value) {
	ESP_LOGI(TAG, "Metadata: %s = %s", key, value);
}


static void on_network_event(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	switch (event_id) {
		case NETWORK_EVENT_DISCONNECT:
			playlist_cancel(&playlist);
			break;
		case NETWORK_EVENT_CONNECT:

#if CONFIG_HTTP_CONTROL
			http_control_init();
#endif
			playlist_open_current(&playlist);
			break;
	}
}


static void on_playback_event(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	switch (event_id) {
		case PLAYBACK_EVENT_PLAYLIST_ITEM_CHANGE:
			source_destroy(&source);
			buffer_clear(&network_buffer);
			const playlist_item_t *playlist_item = (const playlist_item_t *)event_data;
			if (playlist_item->loaded && strlen(playlist_item->uri) > 0) {
				xSemaphoreGive(has_source_semaphore);
			}
			break;
		case PLAYBACK_EVENT_ERROR:
			source_destroy(&source);
			buffer_clear(&network_buffer);
			playlist_open_current(&playlist);
			break;
		default:
			break;
	}
}


static void on_control_command(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	switch (event_id) {
		case CONTROL_COMMAND_SET_VOLUME:
			{
				uint16_t volume = ((uint16_t *)event_data)[0];
				player_state.volume = volume;
			}
			break;
		default:
			break;
	}
}


static void on_playlist_item_changed(playlist_t *playlist, playlist_item_t *item) {
	esp_event_post_to(player_event_loop, PLAYBACK_EVENT, PLAYBACK_EVENT_PLAYLIST_ITEM_CHANGE, item, sizeof(playlist_item_t), portMAX_DELAY);
}


/* Private functions */
static void adjust_volume(decoder_pcm_data_t *pcm) {
	uint16_t volume = player_state.volume;
	if (volume == 0xffff) {
		return;
	}
	int64_t sample;
	for (size_t i = 0; i < pcm->length * 2; ++i) {
		sample = (uint64_t)pcm->data[i] * volume;
		bool negative = sample < 0;
		pcm->data[i] = sample >> (sizeof(volume) * 8);
		if (negative) {
			pcm->data[i] |= 1L << (sizeof(int32_t)-1);
		}
	}
}


static void read_and_play_audio(void) {
	for (;;) {
		if (buffer_read(&network_buffer, audio_process_buffer, sizeof(audio_process_buffer)) == ESP_OK) {
			decoder_feed(&decoder, audio_process_buffer, sizeof(audio_process_buffer));
			for (;;) {
				decoder_pcm_data_t *pcm = decoder_decode(&decoder);
				if (!pcm) {
					esp_event_post_to(player_event_loop, PLAYBACK_EVENT, PLAYBACK_EVENT_ERROR, NULL, 0, portMAX_DELAY);
					return;
				}
				if (pcm->length > 0) {
					adjust_volume(pcm);
					audio_output_write(&audio_output, pcm->data, pcm->length);
				}
				else {
					break;
				}
			}
		}
		else {
			esp_event_post_to(player_event_loop, PLAYBACK_EVENT, PLAYBACK_EVENT_ERROR, NULL, 0, portMAX_DELAY);
			break;
		}
	}
}


/* Tasks */


void player_task(void *arg) {
	for (;;) {
		// Wait until has source
		xSemaphoreTake(has_source_semaphore, portMAX_DELAY);
		// Do not play instant
		vTaskDelay(250 / portTICK_PERIOD_MS);

		{
			// Get playlist item
			playlist_item_t playlist_item;
			playlist_get_item(&playlist, &playlist_item);
			if (!playlist_item.loaded) {
				continue;
			}

			// Initialize source
			source_error_t status = source_init(&source, playlist_item.uri);
			if (status != SOURCE_NO_ERROR) {
				source_destroy(&source);
				buffer_clear(&network_buffer);
				ESP_LOGE(TAG, "Cannot open source %s", playlist_item.uri);
				esp_event_post_to(player_event_loop, PLAYBACK_EVENT, PLAYBACK_EVENT_ERROR, NULL, 0, portMAX_DELAY);
				continue;
			}
		}

		esp_err_t status = decoder_init(&decoder, &source);
		if (status != ESP_OK) {
			decoder_destroy(&decoder);
			source_destroy(&source);
			buffer_clear(&network_buffer);
			esp_event_post_to(player_event_loop, PLAYBACK_EVENT, PLAYBACK_EVENT_ERROR, NULL, 0, portMAX_DELAY);
			continue;
		}

		// Unblock reading task
		xSemaphoreGive(source_ready_semaphore);

		buffer_open(&network_buffer);
		read_and_play_audio();

		// Cleanup
		xSemaphoreTake(source_ready_semaphore, 0);
		decoder_destroy(&decoder);
		source_destroy(&source);
		buffer_clear(&network_buffer);
	}
	vTaskDelete(NULL);
}


void read_task(void *arg) {
	char source_read_buffer[SOURCE_READ_BUFFER_SIZE];

	for (;;) {
		// Wait for new job
		xSemaphoreTake(source_ready_semaphore, portMAX_DELAY);

		// Read source until EOF or fail
		for (;;) {
			ssize_t size = source_read(&source, source_read_buffer, sizeof(source_read_buffer));
			if (size == SOURCE_READ_AGAIN) {
				continue;
			}
			else if (size < 0) {
				buffer_set_eof(&network_buffer);
				break;
			}
			else if (size > 0) {
				player_stats.network_readed_bytes += size;
				buffer_write(&network_buffer, source_read_buffer, size);
			}
		}
	}
	vTaskDelete(NULL);
}


void player_stats_task(void *parameters) {
	for (;;) {
		// Calculate network speed
		float kbps = (float)((player_stats.network_readed_bytes - player_stats.network_readed_bytes_history[0]) * 8) / 1024.0;

		printf(TERM_ERASE_LINE "\rb: %05d | %05d | %05d | %03.1f kbps\r", (int)network_buffer.r_pos, (int)network_buffer.w_pos, (int)buffer_get_full(&network_buffer), kbps);
		fflush(stdout);

		// Record download history
		memmove(&player_stats.network_readed_bytes_history[0], &player_stats.network_readed_bytes_history[1], sizeof(player_stats.network_readed_bytes_history) - sizeof(player_stats.network_readed_bytes));
		player_stats.network_readed_bytes_history[9] = player_stats.network_readed_bytes;

		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
	vTaskDelete(NULL);
}


void init_player(void) {
	source.metadata_callback = on_metadata;

	audio_output_init(&audio_output);

	has_source_semaphore = xSemaphoreCreateBinary();
	if (has_source_semaphore == NULL) {
		ESP_LOGE(TAG, "Semaphore has_source_semaphore not crated.");
		abort();
	}

	source_ready_semaphore = xSemaphoreCreateBinary();
	if (source_ready_semaphore == NULL) {
		ESP_LOGE(TAG, "Semaphore source_ready_semaphore not crated.");
		abort();
	}

	source.semaphore = xSemaphoreCreateBinary();
	if (source.semaphore == NULL) {
		ESP_LOGE(TAG, "Semaphore source.semaphore not crated.");
		abort();
	}

	source.wait_data_semaphore = xSemaphoreCreateBinary();
	if (source.wait_data_semaphore == NULL) {
		ESP_LOGE(TAG, "Semaphore source.wait_data_semaphore not crated.");
		abort();
	}

	player_state.access_mutex = xSemaphoreCreateMutex();
	if (player_state.access_mutex == NULL) {
		ESP_LOGE(TAG, "Mutex player_state.access_mutex not crated.");
		abort();
	}

	char *stream_buffer = heap_caps_malloc(STREAM_BUFFER_SIZE, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
	if (!stream_buffer) {
		ESP_LOGE(TAG, "stream buffer not allocated");
		abort();
	}

	network_buffer.buf = stream_buffer;
	network_buffer.size = STREAM_BUFFER_SIZE;
	buffer_init(&network_buffer);

	xSemaphoreGive(source.semaphore);
	playlist_init(&playlist, on_playlist_item_changed, NULL);
	esp_event_handler_register_with(player_event_loop, NETWORK_EVENT, ESP_EVENT_ANY_ID, on_network_event, NULL);
	esp_event_handler_register_with(player_event_loop, PLAYBACK_EVENT, ESP_EVENT_ANY_ID, on_playback_event, NULL);
	esp_event_handler_register_with(player_event_loop, CONTROL_COMMAND, ESP_EVENT_ANY_ID, on_control_command, NULL);
}
