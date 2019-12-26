#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "audio_output.h"

static const char *TAG = "audio_output";

#ifdef SIMULATOR
static char *device = "default";
#endif


esp_err_t audio_output_init(audio_output_t *output) {
#ifdef SIMULATOR
	int err;

	if ((err = snd_pcm_open(&output->handle, device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) {
		ESP_LOGE(TAG, "Playback open error: %s", snd_strerror(err));
		return ESP_FAIL;
	}

	snd_pcm_hw_params_t *hw_params;

	snd_pcm_hw_params_malloc (&hw_params);
	snd_pcm_hw_params_any(output->handle, hw_params);

	unsigned int rrate = 48000;
	snd_pcm_hw_params_set_access (output->handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(output->handle, hw_params, SND_PCM_FORMAT_S32_LE);
	snd_pcm_hw_params_set_rate_near(output->handle, hw_params, &rrate, NULL);
	snd_pcm_hw_params_set_channels(output->handle, hw_params, 2);
	snd_pcm_uframes_t buffer_size = 8192;
	snd_pcm_uframes_t period_size = 1;

	snd_pcm_hw_params_set_buffer_size_near(output->handle, hw_params, &buffer_size);
	snd_pcm_hw_params_set_period_size_near(output->handle, hw_params, &period_size, NULL);
	snd_pcm_hw_params(output->handle, hw_params);
	if ((err = snd_pcm_hw_params(output->handle, hw_params)) < 0) {
		ESP_LOGE(TAG, "HW set params error: %s", snd_strerror(err));
		snd_pcm_close(output->handle);
		snd_pcm_hw_params_free(hw_params);
		output->handle = NULL;
		return ESP_FAIL;
	}
	snd_pcm_hw_params_free(hw_params);

	if ((err = snd_pcm_set_params(output->handle, SND_PCM_FORMAT_S32_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, output->sample_rate, 1, 500000)) < 0) {
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


esp_err_t audio_output_write(audio_output_t *output, audio_sample_t *buf, size_t samples_count) {
#ifdef SIMULATOR
	snd_pcm_sframes_t frames;
	frames = snd_pcm_writei(output->handle, buf, samples_count);
	while (frames == -EAGAIN) {
		vTaskDelay(1);
		frames = snd_pcm_writei(output->handle, buf, samples_count);
	}
	if (frames < 0) {
		frames = snd_pcm_recover(output->handle, frames, 0);
	}
	if (frames < 0) {
		ESP_LOGE(TAG, "snd_pcm_writei failed: %s", snd_strerror(frames));
	}

	return ESP_OK;
#endif
}


esp_err_t  audio_output_destroy(audio_output_t *output) {
#ifdef SIMULATOR
	snd_pcm_close(output->handle);
	output->handle = NULL;
	return ESP_OK;
#endif
}
