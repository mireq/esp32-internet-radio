#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "audio_output.h"

static const char *TAG = "audio_output";

#ifdef SIMULATOR
#include <pthread.h>
static char *device = "default";
pthread_t write_thread;

typedef struct write_args_t {
	audio_output_t *output;
	audio_sample_t *buf;
	size_t buf_size;
} write_args_t;
#endif


#ifdef SIMULATOR
void *alsa_perform_write(void *arguments) {
	write_args_t *args = (write_args_t *)arguments;
	snd_pcm_sframes_t frames;
	frames = snd_pcm_writei(args->output->handle, args->buf, args->buf_size);
	if (frames < 0) {
		frames = snd_pcm_recover(args->output->handle, frames, 0);
	}
	if (frames < 0) {
		ESP_LOGE(TAG, "snd_pcm_writei failed: %s", snd_strerror(frames));
	}
	xSemaphoreGive(args->output->write_semaphore);
}
#endif


esp_err_t audio_output_init(audio_output_t *output) {
#ifdef SIMULATOR
	output->write_semaphore = xSemaphoreCreateBinary();

	int err;
	if ((err = snd_pcm_open(&output->handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		ESP_LOGE(TAG, "Playback open error: %s", snd_strerror(err));
		return ESP_FAIL;
	}

	if ((err = snd_pcm_set_params(output->handle, SND_PCM_FORMAT_S32_BE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, output->sample_rate, 1, 500000)) < 0) {
		ESP_LOGE(TAG, "Playback set params error: %s", snd_strerror(err));
		snd_pcm_close(output->handle);
		output->handle = NULL;
		return ESP_FAIL;
	}

	return ESP_OK;
#endif
}


esp_err_t audio_output_set_sample_rate(audio_output_t *output, int rate) {
#ifdef SIMULATOR
	int err;
	if ((err = snd_pcm_set_params(output->handle, SND_PCM_FORMAT_S32_BE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, output->sample_rate, 1, 500000)) < 0) {
		ESP_LOGE(TAG, "Playback set params error: %s", snd_strerror(err));
		return ESP_FAIL;
	}

	return ESP_OK;
#endif
}


esp_err_t audio_output_write(audio_output_t *output, audio_sample_t *buf, size_t buf_size) {
#ifdef SIMULATOR
	write_args_t write_args = {
		.output = output,
		.buf = buf,
		.buf_size = buf_size,
	};
	pthread_create(&write_thread, NULL, alsa_perform_write, &write_args);
	xSemaphoreTake(output->write_semaphore, portMAX_DELAY);
	pthread_join(write_thread, NULL);
	return ESP_OK;
#endif
}


esp_err_t  audio_output_destroy(audio_output_t *output) {
#ifdef SIMULATOR
	xSemaphoreGive(output->write_semaphore);
	vSemaphoreDelete(output->write_semaphore);
	snd_pcm_close(output->handle);
	output->handle = NULL;
	return ESP_OK;
#endif
}
