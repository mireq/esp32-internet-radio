#include "esp_log.h"

#include "audio_output.h"

static const char *TAG = "audio_output";

#ifdef SIMULATOR
static char *device = "default";
snd_output_t *output = NULL;
#endif


esp_err_t audio_output_init(audio_output_t *output) {
#ifdef SIMULATOR
	int err;
	if ((err = snd_pcm_open(&output->handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		ESP_LOGE(TAG, "Playback open error: %s", snd_strerror(err));
		return ESP_FAIL;
	}

	if ((err = snd_pcm_set_params(output->handle, SND_PCM_FORMAT_U8, SND_PCM_ACCESS_RW_INTERLEAVED, 2, output->sample_rate, 1, 500000)) < 0) {
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
	if ((err = snd_pcm_set_params(output->handle, SND_PCM_FORMAT_U8, SND_PCM_ACCESS_RW_INTERLEAVED, 2, output->sample_rate, 1, 500000)) < 0) {
		ESP_LOGE(TAG, "Playback set params error: %s", snd_strerror(err));
		return ESP_FAIL;
	}

	return ESP_OK;
#endif
}


esp_err_t audio_output_write(audio_output_t *output, audio_sample_t *buf, size_t buf_size) {
#ifdef SIMULATOR
	snd_pcm_sframes_t frames;
	frames = snd_pcm_writei(output->handle, buf, buf_size);
	if (frames < 0) {
		frames = snd_pcm_recover(output->handle, frames, 0);
	}
	if (frames < 0) {
		ESP_LOGE(TAG, "snd_pcm_writei failed: %s", snd_strerror(frames));
		return ESP_FAIL;
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
